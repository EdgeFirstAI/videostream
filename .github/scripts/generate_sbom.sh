#!/bin/bash
# Generate complete SBOM for C project using scancode-toolkit
# Adapted from Rust SBOM process for C/CMake projects

set -e  # Exit on error

echo "=================================================="
echo "Generating Complete SBOM for VideoStream"
echo "=================================================="
echo

# Step 1: Generate source code SBOM with scancode (~15-30 seconds)
echo "[1/5] Generating source code SBOM with scancode..."
if [ ! -f "venv/bin/scancode" ]; then
    echo "Error: scancode not found. Please install: python3 -m venv venv && venv/bin/pip install scancode-toolkit"
    exit 1
fi

# Scan source directories for licenses, copyrights, and dependencies
venv/bin/scancode -clpieu \
    --cyclonedx source-sbom-raw.json \
    --only-findings \
    --timeout 300 \
    src/ lib/ include/ gst/ deepview/ CMakeLists.txt LICENSE

echo "✓ Generated source-sbom-raw.json (source code audit)"
echo

# Step 2: Clean scancode output for CycloneDX compatibility
echo "[2/5] Cleaning scancode output..."
python3 << 'EOF'
import json
import sys

# Load scancode output
with open('source-sbom-raw.json', 'r') as f:
    sbom = json.load(f)

# Remove problematic metadata (non-string properties violate CycloneDX spec)
if 'metadata' in sbom and 'properties' in sbom['metadata']:
    sbom['metadata']['properties'] = [
        p for p in sbom['metadata']['properties']
        if isinstance(p.get('value'), str)
    ]

# Add project metadata
if 'metadata' not in sbom:
    sbom['metadata'] = {}

if 'component' not in sbom['metadata']:
    sbom['metadata']['component'] = {
        'type': 'library',
        'name': 'videostream',
        'version': '1.0.0',
        'description': 'Video utility library for embedded Linux',
        'licenses': [
            {'license': {'id': 'Apache-2.0'}}
        ]
    }

# Save cleaned version
with open('source-sbom.json', 'w') as f:
    json.dump(sbom, f, indent=2)

sys.exit(0)
EOF
echo "✓ Generated source-sbom.json (cleaned)"
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
            'version': '1.0.0'
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
rm -f source-sbom-raw.json source-sbom.json deps-sbom.json

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
