#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
#
# CI setup hook: install the host packages the C library needs, build it into
# build/, and export the paths the Rust and Python tests load it from.
#
# Called as `pre-command` by the shared rust-quick lane and directly by the
# repository's own jobs. Runs from the repository root.
#
# Environment:
#   SKIP_PACKAGES=1      do not install system packages
#   VSL_CMAKE_ARGS       extra CMake configure arguments
#   VSL_BUILD_DIR        build directory (default: build)
#   VSL_BUILD_WRAPPER    command prefixed to the build step (SonarCloud
#                        build-wrapper)

set -euo pipefail

BUILD_DIR="${VSL_BUILD_DIR:-build}"

if [[ "${SKIP_PACKAGES:-0}" != 1 ]]; then
    sudo apt-get update
    # The trailing `-` removes libunwind-14-dev, which the hosted 22.04 image
    # ships with LLVM 14 and which Breaks the libunwind-dev that
    # libgstreamer1.0-dev depends on. It is a no-op where not installed.
    sudo apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        gstreamer1.0-tools \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        python3-dev python3-venv \
        lcov gcovr \
        libunwind-14-dev-
fi

# shellcheck disable=SC2086  # VSL_CMAKE_ARGS is a list of arguments
cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${VSL_CMAKE_ARGS:-}
# shellcheck disable=SC2086  # VSL_BUILD_WRAPPER is a command and its arguments
${VSL_BUILD_WRAPPER:-} cmake --build "$BUILD_DIR" --parallel

lib_dir="$(pwd)/$BUILD_DIR"
if [[ -n "${GITHUB_ENV:-}" ]]; then
    {
        echo "LD_LIBRARY_PATH=${lib_dir}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        echo "VIDEOSTREAM_LIBRARY=${lib_dir}/libvideostream.so"
    } >> "$GITHUB_ENV"
fi
