#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
#
# Run the staged Rust test binaries on the board, one process per binary and
# including ignored (hardware) tests, writing profraw into build/profraw/ and
# each binary's output into build/rust-test-results/.
#
# Usage: board-rust-tests.sh <tests-dir>

set -uo pipefail

TESTS_DIR="${1:?usage: board-rust-tests.sh <tests-dir>}"

mkdir -p build/rust-test-results
rm -rf build/profraw
mkdir -p build/profraw
LLVM_PROFILE_FILE="$(pwd)/build/profraw/board-%p-%m.profraw"
export LLVM_PROFILE_FILE

# libgcov merges counters into any existing .gcda, and a stale file from the
# pytest run can corrupt the heap at thread exit. Start from a clean slate.
find build -name '*.gcda' -delete 2>/dev/null || true

# Abort at the first heap inconsistency with a backtrace, rather than at an
# unrelated later point, and make use-after-free reads recognisable.
export MALLOC_CHECK_=3
export MALLOC_PERTURB_=42

failed=0
for bin in "$TESTS_DIR"/*; do
    [[ -f "$bin" && -x "$bin" ]] || continue
    name="$(basename "$bin")"
    if ! "$bin" --list >/dev/null 2>&1; then
        echo "skip non-test binary $name"
        continue
    fi
    echo "=== $name ==="
    if ! "$bin" --include-ignored --test-threads=1 2>&1 | tee "build/rust-test-results/${name}.txt"; then
        echo "FAILED: $name"
        failed=1
    fi
done

echo "profraw files: $(find build/profraw -name '*.profraw' | wc -l)"
exit "$failed"
