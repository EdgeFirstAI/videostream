#!/usr/bin/env python3
"""Fix coverage.py XML report paths for SonarCloud compatibility.

The issue: coverage.py generates relative file paths like 'client.py'
instead of 'videostream/client.py', causing SonarCloud to fail resolving.

This script post-processes the Cobertura XML to prefix filenames with the
source package directory, making them resolvable by SonarCloud.
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def fix_coverage_xml(xml_file: str,
                     source_package: str = "videostream") -> None:
    """Fix file paths in coverage XML report.
    
    Args:
        xml_file: Path to the coverage XML file to fix
        source_package: The source package name to prefix filenames with
    """
    tree = ET.parse(xml_file)
    root = tree.getroot()
    
    # Track how many files were fixed
    fixed_count = 0
    
    # Find all class elements and fix their filename attributes
    for class_elem in root.findall(".//class"):
        filename = class_elem.get("filename")
        if filename and not filename.startswith(source_package + "/"):
            # Add the package prefix if not already present
            class_elem.set("filename", f"{source_package}/{filename}")
            fixed_count += 1
    
    # Write the fixed XML back
    tree.write(xml_file, encoding="utf-8", xml_declaration=True)
    
    print(f"✓ Fixed {fixed_count} file paths in {xml_file}")
    print(f"  All filenames now properly prefixed with '{source_package}/'")


def main() -> int:
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: fix_coverage_paths.py <xml_file> [source_package]")
        print("  xml_file: Path to coverage XML report")
        print("  source_package: Package name to prefix")
        print("                  (default: videostream)")
        return 1
    
    xml_file = sys.argv[1]
    source_package = sys.argv[2] if len(sys.argv) > 2 else "videostream"
    
    if not Path(xml_file).exists():
        print(f"ERROR: File not found: {xml_file}")
        return 1
    
    try:
        fix_coverage_xml(xml_file, source_package)
        return 0
    except Exception as e:
        print(f"ERROR: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
