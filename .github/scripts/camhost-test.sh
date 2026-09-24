#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
#
# vsl-camhost capture at several resolutions: start camhost on the camera,
# receive frames with vsl-test-client, and check the frames carry the
# requested size. A sensor may not support every resolution, so the run fails
# only when none of them works.
#
# Usage: camhost-test.sh <build-dir>
# Environment: VSL_CAMERA_DEVICE (default /dev/video3)

set -euo pipefail

BUILD_DIR="${1:?usage: camhost-test.sh <build-dir>}"
CAMERA_DEVICE="${VSL_CAMERA_DEVICE:-/dev/video3}"
RESOLUTIONS="640x480 1280x720 1920x1080"
NUM_FRAMES=10

LD_LIBRARY_PATH="$(pwd)/${BUILD_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

if [[ ! -e "$CAMERA_DEVICE" ]]; then
    echo "::error::camera device $CAMERA_DEVICE not found"
    exit 1
fi

passed=0
failed=0
for res in $RESOLUTIONS; do
    socket="/tmp/camhost_test_${res}_$$"
    log="$(mktemp)"
    echo "=== ${res} on ${CAMERA_DEVICE} ==="

    "$BUILD_DIR/src/vsl-camhost" -d "$CAMERA_DEVICE" -r "$res" -p "$socket" -V &
    camhost_pid=$!
    sleep 3

    if ! kill -0 "$camhost_pid" 2>/dev/null || [[ ! -S "$socket" ]]; then
        echo "vsl-camhost did not start at ${res}"
        failed=$((failed + 1))
    elif timeout 30 "$BUILD_DIR/src/vsl-test-client" "$socket" "$NUM_FRAMES" 2>&1 | tee "$log" \
        && grep -q "Size:.*${res}" "$log"; then
        echo "ok  ${res}"
        passed=$((passed + 1))
    else
        echo "FAILED  ${res}"
        failed=$((failed + 1))
    fi

    kill "$camhost_pid" 2>/dev/null || true
    wait "$camhost_pid" 2>/dev/null || true
    rm -f "$socket" "$log"
    sleep 1
done

echo "camhost: ${passed} passed, ${failed} failed"
if [[ "$passed" -eq 0 ]]; then
    echo "::error::no resolution captured successfully"
    exit 1
fi
