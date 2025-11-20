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
VERSION=$(grep '#define VSL_VERSION "' include/videostream.h | cut -d'"' -f2)
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
    
    # Extract components, filtering out the main videostream component
    # (we define it in metadata, don't want scancode's duplicate)
    if 'components' in sbom:
        for component in sbom['components']:
            # Skip videostream components from scancode - we define the main one in metadata
            if component.get('name') == 'videostream':
                continue
            all_components.append(component)
    
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

# Read NXP license text from file (escape for JSON)
NXP_LICENSE_TEXT=$(cat ext/vpu_wrapper/LICENSE)

python3 << EOF
import json
import subprocess
import sys
import os

VERSION = "$VERSION"

# Read NXP license from environment
NXP_LICENSE_TEXT = """$NXP_LICENSE_TEXT"""

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

# Add third-party embedded dependencies from ext/
third_party = [
    {
        'type': 'library',
        'name': 'stb_image_write',
        'version': 'unknown',
        'description': 'STB single-file public domain image writer library',
        'licenses': [
            {'license': {'id': 'MIT'}},
            {'license': {'name': 'Public Domain'}}
        ],
        'copyright': 'Copyright (c) 2017 Sean Barrett',
        'purl': 'pkg:github/nothings/stb'
    },
    {
        'type': 'library',
        'name': 'vpu_wrapper',
        'version': 'ccaf10a',
        'description': 'Modified version of NXP imx-vpuwrap library for Hantro VPU',
        'licenses': [{
            'license': {
                'name': 'NXP Proprietary',
                'text': {
                    'contentType': 'text/plain',
                    'content': NXP_LICENSE_TEXT
                }
            }
        }],
        'copyright': 'NXP Semiconductors',
        'purl': 'pkg:github/NXP/imx-vpuwrap@ccaf10a0dae7c0d7d204bd64282598bc0e3bd661',
        'externalReferences': [
            {
                'type': 'vcs',
                'url': 'https://github.com/NXP/imx-vpuwrap'
            }
        ]
    }
]

components.extend(third_party)

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

print(f"Found {len(dependencies)} system dependencies + {len(third_party)} third-party components")
sys.exit(0)
EOF
echo "✓ Generated deps-sbom.json (system dependencies + third-party components)"
echo

# Step 4: Merge SBOMs using cyclonedx-cli
echo "[4/5] Merging source and dependency SBOMs..."
if ! command -v cyclonedx &> /dev/null; then
    echo "Error: cyclonedx CLI not found. Please install from https://github.com/CycloneDX/cyclonedx-cli"
    exit 1
fi

cyclonedx merge \
    --input-files source-sbom.json deps-sbom.json \
    --output-file sbom-temp.json

# Remove duplicate videostream component from components list
# (cyclonedx merge sometimes adds metadata.component to components)
python3 << EOF
import json

with open('sbom-temp.json', 'r') as f:
    sbom = json.load(f)

# Filter out videostream from components (it's defined in metadata, not a dependency)
if 'components' in sbom:
    sbom['components'] = [
        c for c in sbom['components']
        if c.get('name') != 'videostream'
    ]

with open('sbom.json', 'w') as f:
    json.dump(sbom, f, indent=2)
EOF

rm -f sbom-temp.json
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

# Cleanup temporary files

# Cleanup temporary files
rm -f source-sbom-src.json source-sbom-lib.json source-sbom-include.json \
      source-sbom-gst.json source-sbom-deepview.json source-sbom-ext.json \
      source-sbom-root.json source-sbom.json deps-sbom.json

echo "=================================================="
echo "SBOM Generation Complete"
echo "=================================================="
echo "Files generated:"
echo "  - sbom.json (merged SBOM)"
echo

# Exit with license policy check result
exit $POLICY_EXIT
