#!/bin/bash
# Generate complete SBOM for C project using scancode-toolkit
# Adapted from Rust SBOM process for C/CMake projects

set -e  # Exit on error

echo "=================================================="
echo "Generating Complete SBOM for VideoStream"
echo "=================================================="
echo

# Step 1: Generate source code SBOM with scancode (scan directories separately)
echo "[1/5] Generating source code SBOM with scancode..."
if [ ! -f "venv/bin/scancode" ]; then
    echo "Error: scancode not found. Please install: python3 -m venv venv && venv/bin/pip install scancode-toolkit"
    exit 1
fi

# Scan each source directory separately to avoid scancode bugs with large directory trees
# This is MUCH faster than scanning everything at once
echo "  Scanning src/..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-src.json \
    --only-findings \
    --timeout 300 \
    src/

echo "  Scanning lib/..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-lib.json \
    --only-findings \
    --timeout 300 \
    lib/

echo "  Scanning include/..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-include.json \
    --only-findings \
    --timeout 300 \
    include/

echo "  Scanning gst/..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-gst.json \
    --only-findings \
    --timeout 300 \
    gst/

echo "  Scanning deepview/..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-deepview.json \
    --only-findings \
    --timeout 300 \
    deepview/

echo "  Scanning ext/ (third-party dependencies)..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-ext.json \
    --only-findings \
    --timeout 300 \
    ext/

echo "  Scanning project root files..."
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-root.json \
    --only-findings \
    --timeout 300 \
    ./CMakeLists.txt ./LICENSE ./README.md

echo "✓ Generated individual SBOM files for each directory"
echo

# Step 2: Merge individual source SBOMs and clean for CycloneDX compatibility
echo "[2/5] Merging and cleaning source SBOMs..."

# Extract version from include/videostream.h (single source of truth)
VERSION=$(grep '#define VSL_VERSION' include/videostream.h | cut -d'"' -f2)
echo "  Detected version: $VERSION"

python3 << EOF
import json
import sys
import os

VERSION = "$VERSION"

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
sbom_files = [
    'source-sbom-src.json',
    'source-sbom-lib.json',
    'source-sbom-include.json',
    'source-sbom-gst.json',
    'source-sbom-deepview.json',
    'source-sbom-ext.json',
    'source-sbom-root.json'
]

# Merge components from all SBOMs
all_components = []
all_licenses = set()

for filename in sbom_files:
    sbom = load_sbom(filename)
    if not sbom:
        continue
    
    # Clean the SBOM
    sbom = clean_sbom_properties(sbom)
    
    # Extract components
    if 'components' in sbom:
        all_components.extend(sbom['components'])
    
    # Extract licenses
    if 'metadata' in sbom and 'licenses' in sbom['metadata']:
        for lic in sbom['metadata']['licenses']:
            if 'license' in lic and 'id' in lic['license']:
                all_licenses.add(lic['license']['id'])

# Create merged source SBOM
merged_sbom = {
    'bomFormat': 'CycloneDX',
    'specVersion': '1.3',
    'version': 1,
    'metadata': {
        'component': {
            'type': 'library',
            'name': 'videostream',
            'version': VERSION,
            'description': 'Video utility library for embedded Linux',
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

# Step 3: Generate system dependencies manifest
echo "[3/5] Generating system dependencies manifest..."
python3 << 'EOF'
import json
import subprocess
import sys

def get_pkg_config_info(package):
    """Get version and license info from pkg-config"""
    try:
        version = subprocess.check_output(
            ['pkg-config', '--modversion', package],
            stderr=subprocess.DEVNULL
        ).decode().strip()
        return version
    except:
        return 'unknown'

# System dependencies from CMakeLists.txt
dependencies = [
    {'name': 'gstreamer-1.0', 'type': 'library'},
    {'name': 'gstreamer-video-1.0', 'type': 'library'},
    {'name': 'gstreamer-app-1.0', 'type': 'library'},
    {'name': 'gstreamer-allocators-1.0', 'type': 'library'},
    {'name': 'glib-2.0', 'type': 'library'},
]

components = []
for dep in dependencies:
    version = get_pkg_config_info(dep['name'])
    component = {
        'type': dep['type'],
        'name': dep['name'],
        'version': version,
    }

    # Add known licenses for common dependencies
    if 'gstreamer' in dep['name']:
        component['licenses'] = [{'license': {'id': 'LGPL-2.1-or-later'}}]
    elif 'glib' in dep['name']:
        component['licenses'] = [{'license': {'id': 'LGPL-2.1-or-later'}}]

    components.append(component)

# Create CycloneDX SBOM for dependencies
deps_sbom = {
    'bomFormat': 'CycloneDX',
    'specVersion': '1.3',
    'version': 1,
    'metadata': {
        'component': {
            'type': 'library',
            'name': 'videostream',
            'version': VERSION
        }
    },
    'components': components
}

with open('deps-sbom.json', 'w') as f:
    json.dump(deps_sbom, f, indent=2)

print(f"Found {len(components)} system dependencies")
sys.exit(0)
EOF
echo "✓ Generated deps-sbom.json (system dependencies)"
echo

# Step 4: Merge SBOMs using cyclonedx-cli
echo "[4/5] Merging source and dependency SBOMs..."
if ! command -v cyclonedx &> /dev/null; then
    echo "Error: cyclonedx CLI not found. Please install from https://github.com/CycloneDX/cyclonedx-cli"
    exit 1
fi

cyclonedx merge \
    --input-files source-sbom.json deps-sbom.json \
    --output-file sbom.json

echo "✓ Generated sbom.json (merged: source + dependencies)"
echo

# Step 5: Check license policy
echo "[5/5] Checking license policy compliance..."
if [ -f ".github/scripts/check_license_policy.py" ]; then
    python3 .github/scripts/check_license_policy.py sbom.json
    POLICY_EXIT=$?
else
    echo "Warning: License policy checker not found, skipping..."
    POLICY_EXIT=0
fi
echo

# Step 6: Generate NOTICE file
echo "[6/6] Generating NOTICE file..."
if [ -f ".github/scripts/generate_notice.py" ]; then
    python3 .github/scripts/generate_notice.py sbom.json > NOTICE.generated
    echo "✓ Generated NOTICE.generated (third-party attributions)"
else
    echo "Warning: NOTICE generator not found, skipping..."
fi
echo

# Cleanup temporary files
rm -f source-sbom-src.json source-sbom-lib.json source-sbom-include.json \
      source-sbom-gst.json source-sbom-deepview.json source-sbom-ext.json \
      source-sbom-root.json source-sbom.json deps-sbom.json

echo "=================================================="
echo "SBOM Generation Complete"
echo "=================================================="
echo "Files generated:"
echo "  - sbom.json (merged SBOM)"
if [ -f "NOTICE.generated" ]; then
    echo "  - NOTICE.generated (third-party attributions)"
fi
echo

# Exit with license policy check result
exit $POLICY_EXIT
