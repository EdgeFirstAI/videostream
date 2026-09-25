#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
#
# Build coverage-instrumented Rust test binaries and the CLI for the board, and
# stage them in target/tests/. A board runs tests and never builds, so every
# binary it executes is produced here on the aarch64 hosted runner.

set -euo pipefail

# RUSTFLAGS with -C instrument-coverage and the llvm-cov target directory.
# shellcheck source=/dev/null
source <(cargo llvm-cov show-env --export-prefix)

# -P hardware selects the nextest profile that includes ignored tests.
cargo nextest run --workspace --all-features --cargo-profile profiling -P hardware --no-run

# CLI integration tests spawn the binary, so it needs instrumentation too.
cargo build --profile profiling --bin videostream

mkdir -p target/tests
find target/profiling/deps/ -maxdepth 1 -type f -executable | while read -r bin; do
    [[ "$(basename "$bin")" =~ \.(so|a|d|rlib)$ ]] && continue
    # Test binaries answer --list; other executables do not.
    if "$bin" --list >/dev/null 2>&1; then
        cp "$bin" target/tests/
        echo "staged $(basename "$bin")"
    fi
done

if [[ ! -f target/profiling/videostream ]]; then
    echo "::error::instrumented CLI binary target/profiling/videostream not found"
    exit 1
fi
cp target/profiling/videostream target/tests/
ls -la target/tests/
