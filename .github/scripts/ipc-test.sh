#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Au-Zone Technologies
#
# Host/client IPC round trip: vsl-test-host publishes frames on a UNIX socket
# and vsl-test-client must receive them. Frames use a DMA heap where one is
# available and POSIX shared memory otherwise.
#
# Usage: ipc-test.sh <build-dir> [frames]

set -euo pipefail

BUILD_DIR="${1:?usage: ipc-test.sh <build-dir> [frames]}"
NUM_FRAMES="${2:-30}"
SOCKET_PATH="/tmp/videostream_ci_ipc_$$"

LD_LIBRARY_PATH="$(pwd)/${BUILD_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

"$BUILD_DIR/src/vsl-test-host" "$SOCKET_PATH" &
host_pid=$!
trap 'kill "$host_pid" 2>/dev/null || true; wait "$host_pid" 2>/dev/null || true; rm -f "$SOCKET_PATH"' EXIT

sleep 2
if ! kill -0 "$host_pid" 2>/dev/null; then
    echo "::error::vsl-test-host failed to start"
    exit 1
fi

"$BUILD_DIR/src/vsl-test-client" "$SOCKET_PATH" "$NUM_FRAMES"
echo "Host/client IPC: $NUM_FRAMES frames received"
