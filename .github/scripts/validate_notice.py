#!/usr/bin/env python3
"""
Validate NOTICE file against SBOM (CycloneDX format)
Ensures NOTICE file lists all first-level dependencies requiring attribution

LICENSE POLICY SOURCE:
This script implements NOTICE file validation per the Au-Zone Software Process Specification.
Last synchronized with policy version: 2.0 (2025-11-22)

IMPORTANT: The NOTICE file is manually curated by developers and describes:
  1. First-level dependencies requiring attribution (MIT, Apache-2.0, BSD, etc.)
  2. Proprietary licenses with special terms (e.g., NXP Proprietary)
  3. License exceptions approved by VP R&D

The NOTICE file is NOT auto-generated. This validator only checks that first-level
dependencies match between NOTICE and sbom.json, and warns about discrepancies.

CUSTOMIZATION:
- Update ATTRIBUTION_REQUIRED_LICENSES to match your attribution requirements
- Update PROPRIETARY_LICENSES to match your approved proprietary licenses
"""

import json
import sys
import re
from typing import Set, List, Dict, Tuple

# Licenses that require attribution in NOTICE file
ATTRIBUTION_REQUIRED_LICENSES: Set[str] = {
    "Apache-2.0",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "BSD-4-Clause",
    "MIT",
    "ISC",
    "IJG",
}

# Proprietary licenses that should always be in NOTICE
PROPRIETARY_LICENSES: Set[str] = {
    "NXP-Proprietary",
    "NXP Proprietary",
    "Proprietary",
}


def extract_license_from_component(component: Dict) -> Set[str]:
    """Extract all license identifiers from a component."""
    licenses: Set[str] = set()

    if "licenses" not in component:
        return licenses

    for lic_entry in component["licenses"]:
        # Check for direct license ID
        if "license" in lic_entry:
            if "id" in lic_entry["license"]:
                licenses.add(lic_entry["license"]["id"])
            # Also check for license name (for proprietary/non-SPDX licenses)
            elif "name" in lic_entry["license"]:
                licenses.add(lic_entry["license"]["name"])

        # Check for SPDX expression
        if "expression" in lic_entry:
            expr = lic_entry["expression"]
            # Parse expression (simplified - splits on OR/AND/WITH)
            parts = expr.replace("(", "").replace(")", "")
            parts = parts.replace(" OR ", " ").replace(" AND ", " ").replace(" WITH ", " ")
            for part in parts.split():
                if part and not part.isspace():
                    licenses.add(part)

    return licenses


def get_first_level_dependencies(sbom: Dict) -> Set[str]:
    """
    Extract first-level (direct) dependencies from SBOM.

    First-level dependencies are those directly required by the project,
    not transitive dependencies of dependencies.
    """
    first_level: Set[str] = set()

    # Get the metadata component (the project itself)
    metadata = sbom.get("metadata", {})
    project_component = metadata.get("component", {})
    project_ref = project_component.get("bom-ref", "")

    # Get dependencies graph
    dependencies = sbom.get("dependencies", [])

    # Find the project's direct dependencies
    first_level_refs = []
    for dep_entry in dependencies:
        if dep_entry.get("ref") == project_ref:
            # These are the direct dependencies of the project
            first_level_refs = dep_entry.get("dependsOn", [])
            break
    else:
        # If no dependency graph, assume all components are first-level
        # (fallback for SBOMs without dependency graph)
        components = sbom.get("components", [])
        return {f"{c.get('name', 'unknown')} {c.get('version', 'unknown')}"
                for c in components}

    # Map bom-ref to component name+version
    components = sbom.get("components", [])
    ref_to_component = {c.get("bom-ref"): c for c in components}

    for ref in first_level_refs:
        if ref in ref_to_component:
            component = ref_to_component[ref]
            name = component.get("name", "unknown")
            version = component.get("version", "unknown")
            
            # For first-level dependencies, include all (regardless of license)
            # This ensures NOTICE tracks direct project dependencies
            first_level.add(f"{name} {version}")

    return first_level


def parse_notice_file(notice_path: str) -> Set[str]:
    """
    Parse NOTICE file to extract listed dependencies.

    Looks for lines like:
      * package-name 1.2.3 (MIT)
      * NXP BSP - NXP Proprietary
    """
    listed_deps: Set[str] = set()

    try:
        with open(notice_path, 'r') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading NOTICE file: {e}", file=sys.stderr)
        sys.exit(1)

    # Match patterns like: "  * package-name 1.2.3 (License)"
    pattern = r'^\s*\*\s+(\S+)\s+([\d.]+(?:-[\w.]+)?)\s+\(.*?\)'

    # Also match proprietary entries: "  * NXP BSP - NXP Proprietary"
    proprietary_pattern = r'^\s*\*\s+(.+?)\s+-\s+.*Proprietary'

    for line in content.split('\n'):
        match = re.match(pattern, line)
        if match:
            name = match.group(1)
            version = match.group(2)
            listed_deps.add(f"{name} {version}")
        else:
            prop_match = re.match(proprietary_pattern, line)
            if prop_match:
                # For proprietary entries, just use the name
                listed_deps.add(prop_match.group(1))

    return listed_deps


def validate_notice(notice_path: str, sbom_path: str) -> Tuple[bool, List[str], List[str]]:
    """
    Validate NOTICE file against SBOM.

    Returns:
        (passed, missing_deps, extra_deps)
    """
    try:
        with open(sbom_path, 'r') as f:
            sbom = json.load(f)
    except Exception as e:
        print(f"Error reading SBOM: {e}", file=sys.stderr)
        sys.exit(1)

    # Validate SBOM is not empty
    components = sbom.get("components", [])
    if len(components) == 0:
        print("ERROR: SBOM has no components!", file=sys.stderr)
        print("This likely means SBOM generation failed or no dependencies were found.", file=sys.stderr)
        print("Cannot validate NOTICE file against an empty SBOM.", file=sys.stderr)
        sys.exit(1)

    # Get first-level dependencies from SBOM
    sbom_first_level = get_first_level_dependencies(sbom)

    # Get listed dependencies from NOTICE
    notice_deps = parse_notice_file(notice_path)

    # Find missing and extra dependencies
    missing_deps = list(sbom_first_level - notice_deps)
    extra_deps = list(notice_deps - sbom_first_level)

    missing_deps.sort()
    extra_deps.sort()

    passed = len(missing_deps) == 0 and len(extra_deps) == 0

    return passed, missing_deps, extra_deps


def main():
    if len(sys.argv) != 3:
        print("Usage: validate_notice.py <NOTICE> <sbom.json>", file=sys.stderr)
        sys.exit(1)

    notice_path = sys.argv[1]
    sbom_path = sys.argv[2]

    passed, missing_deps, extra_deps = validate_notice(notice_path, sbom_path)

    print("=" * 80)
    print("NOTICE File Validation")
    print("=" * 80)
    print()

    if missing_deps:
        print("MISSING FROM NOTICE FILE:")
        print("-" * 80)
        print("The following first-level dependencies require attribution but are")
        print("missing from the NOTICE file:")
        print()
        for dep in missing_deps:
            print(f"  • {dep}")
        print()

    if extra_deps:
        print("EXTRA IN NOTICE FILE:")
        print("-" * 80)
        print("The following dependencies are listed in NOTICE but are not")
        print("first-level dependencies in sbom.json:")
        print()
        for dep in extra_deps:
            print(f"  • {dep}")
        print()

    if passed:
        print("✓ NOTICE file validation PASSED!")
        print("  All first-level dependencies are properly documented.")
        print()
        sys.exit(0)
    else:
        print("✗ NOTICE file validation FAILED!")
        print()
        print("ACTION REQUIRED:")
        print("  1. Review the missing/extra dependencies above")
        print("  2. Manually update the NOTICE file to match first-level dependencies")
        print("  3. Re-run this validation script")
        print()
        sys.exit(1)


if __name__ == "__main__":
    main()
