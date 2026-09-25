#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
#
# Turn the board's raw coverage into reports, on an aarch64 runner that has the
# toolchain the board lacks:
#   - Rust: merge profraw against the instrumented objects -> LCOV
#   - C: place each .gcda beside its .gcno and run gcovr -> SonarQube XML
#
# Expects, relative to the repository root:
#   build/                  aarch64 build artifact (.gcno files)
#   target/profiling/       instrumented Rust objects from the same build
#   board-coverage/         the board's profraw and gcda files
# Writes build/coverage_rust_board.lcov and build/coverage_c_board_sonar.xml.

set -euo pipefail

profraw=$(find board-coverage -name '*.profraw' -type f)
if [[ -z "$profraw" ]]; then
    echo "::error::no profraw files from the board"
    exit 1
fi
echo "profraw files: $(wc -l <<< "$profraw")"

sysroot="$(rustc --print sysroot)"
llvm_profdata="$(find "$sysroot" -name llvm-profdata -type f | head -1)"
llvm_cov="$(find "$sysroot" -name llvm-cov -type f | head -1)"
if [[ -z "$llvm_profdata" || -z "$llvm_cov" ]]; then
    echo "::error::llvm-tools-preview is not installed on the toolchain"
    exit 1
fi

mkdir -p build
# shellcheck disable=SC2086  # one argument per profraw file
"$llvm_profdata" merge -sparse $profraw -o build/board.profdata

# Artifact downloads drop the executable bit, so ELF objects are found by
# content rather than by permission.
mapfile -t candidates < <(find target/profiling/deps -maxdepth 1 -type f \
    ! -name '*.d' ! -name '*.rlib' ! -name '*.rmeta')
candidates+=(target/profiling/videostream)
objects=()
for obj in "${candidates[@]}"; do
    if [[ ! -f "$obj" ]] || ! file "$obj" | grep -q ELF; then
        continue
    fi
    if [[ ${#objects[@]} -eq 0 ]]; then
        objects+=("$obj")
    else
        objects+=("--object=$obj")
    fi
done
if [[ ${#objects[@]} -eq 0 ]]; then
    echo "::error::no instrumented ELF objects in target/profiling/"
    exit 1
fi

"$llvm_cov" export \
    --format=lcov \
    --instr-profile=build/board.profdata \
    --ignore-filename-regex='/.cargo/registry|/rustc/' \
    "${objects[@]}" > build/coverage_rust_board.lcov
if [[ ! -s build/coverage_rust_board.lcov ]]; then
    echo "::error::empty Rust coverage report"
    exit 1
fi

find board-coverage -name '*.gcda' -type f | while read -r gcda; do
    gcno="$(find build -name "$(basename "${gcda%.gcda}").gcno" -type f | head -1)"
    if [[ -n "$gcno" ]]; then
        cp "$gcda" "$(dirname "$gcno")/"
    else
        echo "no .gcno for $(basename "$gcda")"
    fi
done
gcovr -r . --sonarqube -o build/coverage_c_board_sonar.xml build/
