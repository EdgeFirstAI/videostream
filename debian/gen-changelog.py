#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025 Au-Zone Technologies. All Rights Reserved.
"""
Generate debian/changelog from CHANGELOG.md and version header.

This script extracts the current version and its release notes from CHANGELOG.md,
then generates a debian/changelog file. Historical changelog entries are not
included - users should refer to CHANGELOG.md for full history.

Usage:
    python3 debian/gen-changelog.py
"""

import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import List


def get_version() -> str:
    """Extract version from C header (single source of truth)."""
    header_path = Path(__file__).parent.parent / "include" / "videostream.h"
    try:
        content = header_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"Error reading header file '{header_path}': {exc}", file=sys.stderr)
        sys.exit(1)

    match = re.search(r'#define\s+VSL_VERSION\s+"([^"]+)"', content)
    if not match:
        print("Could not find VSL_VERSION in videostream.h", file=sys.stderr)
        sys.exit(1)
    return match.group(1)


def get_maintainer() -> str:
    """Get maintainer from git config."""
    try:
        name = subprocess.check_output(
            ["git", "config", "user.name"], text=True
        ).strip()
        email = subprocess.check_output(
            ["git", "config", "user.email"], text=True
        ).strip()
        return f"{name} <{email}>"
    except subprocess.CalledProcessError:
        return "Au-Zone Technologies <support@au-zone.com>"


def get_changelog_section(version: str) -> List[str]:
    """Extract release notes for a specific version from CHANGELOG.md."""
    changelog_path = Path(__file__).parent.parent / "CHANGELOG.md"
    try:
        content = changelog_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"Error reading changelog '{changelog_path}': {exc}", file=sys.stderr)
        sys.exit(1)

    # Find the section for this version
    # Pattern: ## [version] - date
    pattern = rf"## \[{re.escape(version)}\][^\n]*\n(.*?)(?=\n## \[|\n---|\Z)"
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        return [f"  * Release {version}"]

    section = match.group(1).strip()
    changes = []

    # Parse the markdown into debian changelog format
    current_category = None
    for line in section.split("\n"):
        line = line.strip()
        if not line:
            continue

        # Category headers like ### Added, ### Fixed, etc.
        if line.startswith("### "):
            current_category = line[4:].strip()
            continue

        # Bullet points
        if line.startswith("- "):
            item = line[2:].strip()
            # Remove markdown formatting
            item = re.sub(r"\*\*([^*]+)\*\*", r"\1", item)  # Bold
            item = re.sub(r"`([^`]+)`", r"\1", item)  # Code
            # Extract just the main description (before detailed explanation)
            # Split on " - " or " (" that introduce details (not hyphenated words)
            main_desc = re.split(r" - |\s+\(", item)[0].strip()
            if current_category:
                changes.append(f"  * {current_category}: {main_desc}")
            else:
                changes.append(f"  * {main_desc}")

    if not changes:
        changes = [f"  * Release {version}"]

    return changes


def format_date() -> str:
    """Format current date in Debian changelog format."""
    # Format: Day, DD Mon YYYY HH:MM:SS +0000
    return datetime.now(timezone.utc).strftime("%a, %d %b %Y %H:%M:%S +0000")


def main():
    """Generate debian/changelog from template."""
    script_dir = Path(__file__).parent
    template_path = script_dir / "changelog.in"
    output_path = script_dir / "changelog"

    version = get_version()
    maintainer = get_maintainer()
    date = format_date()
    changes = get_changelog_section(version)

    try:
        template = template_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"Error reading template '{template_path}': {exc}", file=sys.stderr)
        sys.exit(1)

    changelog = template.replace("@VERSION@", version)
    changelog = changelog.replace("@MAINTAINER@", maintainer)
    changelog = changelog.replace("@DATE@", date)
    changelog = changelog.replace("@CHANGES@", "\n".join(changes))

    try:
        output_path.write_text(changelog, encoding="utf-8")
    except OSError as exc:
        print(f"Error writing changelog '{output_path}': {exc}", file=sys.stderr)
        sys.exit(1)

    print(f"Generated {output_path} for version {version}")


if __name__ == "__main__":
    main()
