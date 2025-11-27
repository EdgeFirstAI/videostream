#!/bin/bash
# Generate complete SBOM using scancode-toolkit
#
# This script implements SBOM generation per the Au-Zone Software Process Specification.
# Last synchronized with policy version: 2.0 (2025-11-22)
#
# This script generates a comprehensive Software Bill of Materials (SBOM)
# in CycloneDX format by:
#   1. Scanning source code directories with scancode
#   2. Scanning package manifests for dependencies
#   3. Merging all SBOMs into a single file
#   4. Validating license policy compliance
#   5. Validating NOTICE file (if present)
#
# CUSTOMIZE FOR YOUR PROJECT:
# - Update PROJECT_NAME and PROJECT_TYPE below
# - Update SOURCE_DIRS list for your source directories
# - Update VERSION_FILE location (single source of truth for version)
# - Update MANIFEST_FILES for your package managers
# - For C/C++ projects: Add system dependencies in Step 3
# - For multi-language: Adjust language-specific sections

set -e  # Exit on error

# ===========================================================================
# PROJECT CONFIGURATION - CUSTOMIZE THESE
# ===========================================================================

PROJECT_NAME="videostream"
PROJECT_TYPE="library"  # Options: library, application, framework
VERSION_FILE="include/videostream.h"  # Single source of truth for version

# Source directories to scan (space-separated)
SOURCE_DIRS="src lib include videostream gst"

# Package manifest files (for dependency parsing)
MANIFEST_FILES="Cargo.toml Cargo.lock pyproject.toml requirements.txt package.json package-lock.json go.mod go.sum"

# ===========================================================================
# SBOM GENERATION
# ===========================================================================

echo "=================================================="
echo "Generating Complete SBOM for $PROJECT_NAME"
echo "=================================================="
echo

# Extract version from header file
if [ -f "$VERSION_FILE" ]; then
    VERSION=$(grep '^#define VSL_VERSION ' "$VERSION_FILE" | head -1 | cut -d'"' -f2)
    echo "Detected version: $VERSION"
else
    VERSION="unknown"
    echo "Warning: VERSION file not found, using version: $VERSION"
fi
echo

# Step 1: Generate source code SBOM with scancode
echo "[1/6] Generating source code SBOM with scancode..."
if [ ! -f "venv/bin/scancode" ]; then
    echo "Error: scancode not found. Please install:"
    echo "  python3 -m venv venv"
    echo "  venv/bin/pip install scancode-toolkit"
    exit 1
fi

# Scan each source directory separately (MUCH faster than scanning all at once)
SBOM_FILES=""
for dir in $SOURCE_DIRS; do
    if [ -d "$dir" ]; then
        echo "  Scanning $dir/..."
        OUTPUT_FILE="source-sbom-$(basename $dir).json"
        venv/bin/scancode -clpieu \
            --cyclonedx "$OUTPUT_FILE" \
            --timeout 300 \
            "$dir/"
        SBOM_FILES="$SBOM_FILES $OUTPUT_FILE"
    fi
done

# Note: Manifest files (Cargo.toml, requirements.txt, etc.) are handled by
# language-specific tools in Step 3, not by scancode here. Scancode often
# fails to parse TOML/lock files correctly and doesn't extract dependencies.

echo "✓ Generated individual SBOM files"
echo

# Step 2: Merge and clean source SBOMs
echo "[2/6] Merging and cleaning source SBOMs..."

python3 << EOF
import json
import sys
import os

VERSION = "$VERSION"
PROJECT_NAME = "$PROJECT_NAME"
PROJECT_TYPE = "$PROJECT_TYPE"

def load_sbom(filename):
    """Load an SBOM file if it exists"""
    if not os.path.exists(filename):
        return None
    with open(filename, 'r') as f:
        return json.load(f)

def clean_sbom_properties(sbom):
    """Remove problematic metadata that violates CycloneDX spec"""
    if 'metadata' in sbom and 'properties' in sbom['metadata']:
        sbom['metadata']['properties'] = [
            p for p in sbom['metadata']['properties']
            if isinstance(p.get('value'), str)
        ]
    return sbom

# Load all individual SBOMs
sbom_files = """$SBOM_FILES""".split()
all_components = []

for filename in sbom_files:
    sbom = load_sbom(filename)
    if not sbom:
        continue

    # Clean the SBOM
    sbom = clean_sbom_properties(sbom)

    # Extract components, filtering out the main project component
    if 'components' in sbom:
        for component in sbom['components']:
            # Skip main project component from scancode - we define it in metadata
            if component.get('name') == PROJECT_NAME.lower():
                continue
            all_components.append(component)

# Create merged source SBOM
merged_sbom = {
    'bomFormat': 'CycloneDX',
    'specVersion': '1.6',
    'version': 1,
    'metadata': {
        'component': {
            'type': PROJECT_TYPE,
            'name': PROJECT_NAME,
            'version': VERSION,
            'licenses': [
                {'license': {'id': 'Apache-2.0'}}
            ]
        }
    },
    'components': all_components
}

# Save merged version
with open('source-sbom.json', 'w') as f:
    json.dump(merged_sbom, f, indent=2)

print(f"Merged {len(sbom_files)} source SBOMs into source-sbom.json")
print(f"Total components: {len(all_components)}")
sys.exit(0)
EOF

echo "✓ Generated source-sbom.json (merged and cleaned)"
echo

# Step 3: Generate dependency SBOM (language-specific)
echo "[3/6] Generating dependency SBOM..."

# For Rust projects with cargo-cyclonedx
if [ -f "Cargo.toml" ]; then
    # Check if cargo cyclonedx is available (it's a cargo subcommand, not a standalone binary)
    if ! cargo cyclonedx --version &> /dev/null; then
        echo "ERROR: cargo-cyclonedx not found. Please install:"
        echo "  cargo install cargo-cyclonedx"
        exit 1
    fi
    echo "  Generating Rust dependencies with cargo cyclonedx..."
    cargo cyclonedx --format json --all
    
    # cargo-cyclonedx creates files in each crate directory
    # For projects with multiple crates, use the main public crate's SBOM
    # (it already includes internal crates as dependencies)
    if [ -f "crates/videostream/videostream.cdx.json" ]; then
        cp "crates/videostream/videostream.cdx.json" deps-sbom.json
        echo "  ✓ Generated Rust dependency SBOM from crates/videostream/videostream.cdx.json"
    else
        # Fallback: find and use any .cdx.json file
        CDX_FILES=$(find . -name "*.cdx.json" -type f)
        if [ -z "$CDX_FILES" ]; then
            echo "  Warning: cargo cyclonedx did not generate any .cdx.json files"
        else
            echo "  Found cargo cyclonedx outputs:"
            for file in $CDX_FILES; do
                echo "    - $file"
            done
            
            # Use the first one as deps-sbom.json
            FIRST_CDX=$(echo "$CDX_FILES" | head -1)
            cp "$FIRST_CDX" deps-sbom.json
            echo "  ✓ Generated Rust dependency SBOM from $FIRST_CDX"
        fi
    fi
fi

# For Python projects (scancode already parsed requirements.txt/pyproject.toml)
# Dependencies are included in source-sbom.json

# For C/C++ projects - manually define system dependencies
if [ ! -f "deps-sbom.json" ]; then
    echo "  Creating empty deps-sbom.json (customize for system dependencies)..."
    cat > deps-sbom.json << 'EOFPYTHON'
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.6",
  "version": 1,
  "components": []
}
EOFPYTHON
fi

echo "✓ Generated deps-sbom.json"
echo

# Step 4: Merge SBOMs using cyclonedx-cli
echo "[4/6] Merging source and dependency SBOMs..."

# Create sbom-deps.json for NOTICE validation (preserves dependency graph)
if [ -f "deps-sbom.json" ]; then
    cp deps-sbom.json sbom-deps.json
    echo "  ✓ Created sbom-deps.json (for NOTICE validation - has dependency graph)"
fi

if ! command -v cyclonedx &> /dev/null; then
    if ! command -v ~/.local/bin/cyclonedx &> /dev/null; then
        echo "Error: cyclonedx CLI not found. Please install from https://github.com/CycloneDX/cyclonedx-cli"
        exit 1
    fi
    CYCLONEDX=~/.local/bin/cyclonedx
else
    CYCLONEDX=cyclonedx
fi

$CYCLONEDX merge \
    --input-files source-sbom.json deps-sbom.json \
    --output-file sbom-temp.json

# Remove duplicate project component from components list
# AND restore bom-ref to metadata.component (cyclonedx merge strips it)
python3 << EOF
import json
import sys

PROJECT_NAME = "$PROJECT_NAME"

with open('sbom-temp.json', 'r') as f:
    sbom = json.load(f)

# Filter out project name from components (it's defined in metadata, not a dependency)
if 'components' in sbom:
    sbom['components'] = [
        c for c in sbom['components']
        if c.get('name') != PROJECT_NAME.lower()
    ]

# Restore bom-ref to metadata.component (required for dependency graph lookup)
# cyclonedx merge strips this field, breaking first-level dependency identification
if 'dependencies' in sbom and sbom['dependencies']:
    # Find the first dependency entry (should be the root)
    first_dep_ref = sbom['dependencies'][0].get('ref')
    if first_dep_ref and 'metadata' in sbom and 'component' in sbom['metadata']:
        sbom['metadata']['component']['bom-ref'] = first_dep_ref
        print(f"Restored bom-ref to metadata.component: {first_dep_ref}", file=sys.stderr)

with open('sbom.json', 'w') as f:
    json.dump(sbom, f, indent=2)
EOF

rm -f sbom-temp.json
echo "✓ Generated sbom.json (merged: source + dependencies - complete license info)"
echo

# Step 5: Check license policy
echo "[5/6] Checking license policy compliance..."
if [ -f ".github/scripts/check_license_policy.py" ]; then
    python3 .github/scripts/check_license_policy.py sbom.json
    POLICY_EXIT=$?
else
    echo "Warning: License policy checker not found, skipping..."
    POLICY_EXIT=0
fi
echo

# Step 6: Validate NOTICE file
echo "[6/6] Validating NOTICE file..."
if [ -f "NOTICE" ] && [ -f ".github/scripts/validate_notice.py" ]; then
    python3 .github/scripts/validate_notice.py NOTICE sbom.json
    NOTICE_EXIT=$?
    if [ $NOTICE_EXIT -ne 0 ]; then
        echo "⚠️  NOTICE file validation failed - please update NOTICE manually"
    else
        echo "✓ NOTICE file validated (matches first-level dependencies)"
    fi
else
    echo "Skipping NOTICE validation (file or validator not found)"
    NOTICE_EXIT=0
fi
echo

# Cleanup temporary files
rm -f $SBOM_FILES source-sbom.json deps-sbom.json

echo "=================================================="
echo "SBOM Generation Complete"
echo "=================================================="
echo "Files generated:"
echo "  - sbom.json (merged SBOM)"
echo
echo "Files validated:"
echo "  - NOTICE (third-party attributions)"
echo

# Exit with error if either check failed
if [ $POLICY_EXIT -ne 0 ] || [ $NOTICE_EXIT -ne 0 ]; then
    exit 1
fi
exit 0
