#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
"""Convert libtest output captured on the board into JUnit XML.

Usage: rust-junit.py <results-dir> <output.xml>

Each <results-dir>/<binary>.txt holds one test binary's stdout. Individual
test times are not in libtest's plain output, so every case records 0.
"""

import pathlib
import re
import sys
from xml.sax.saxutils import quoteattr

CASE = re.compile(r"^test ([\w:]+) \.\.\. (ok|FAILED|ignored)$", re.M)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    results = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])

    cases = []
    for log in sorted(results.glob("*.txt")):
        text = log.read_text(encoding="utf-8", errors="replace")
        cases += [(log.stem, name, status) for name, status in CASE.findall(text)]

    failures = sum(1 for _, _, status in cases if status == "FAILED")
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<testsuites name="rust-board" tests="{len(cases)}" failures="{failures}">',
        f'  <testsuite name="rust-board" tests="{len(cases)}" failures="{failures}">',
    ]
    for binary, name, status in cases:
        attrs = f"name={quoteattr(name)} classname={quoteattr(binary)} time=\"0\""
        if status == "FAILED":
            lines.append(f'    <testcase {attrs}><failure message="failed"/></testcase>')
        elif status == "ignored":
            lines.append(f"    <testcase {attrs}><skipped/></testcase>")
        else:
            lines.append(f"    <testcase {attrs}/>")
    lines += ["  </testsuite>", "</testsuites>"]
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{output}: {len(cases)} tests, {failures} failed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
