#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

# Test script for VideoStream host/client communication
# Runs test_host and test_client together to verify frame sharing

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
SOCKET_PATH="/tmp/videostream_test_$$"  # Use PID for unique socket
NUM_FRAMES=30  # Receive 30 frames (~1 second at 30fps)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}ERROR: Build directory not found: $BUILD_DIR${NC}"
    echo "Please build the project first:"
    echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build build -j\$(nproc)"
    exit 1
fi

# Check if executables exist
if [ ! -x "$BUILD_DIR/src/vsl-test-host" ]; then
    echo -e "${RED}ERROR: test_host executable not found${NC}"
    echo "Please build the project first."
    exit 1
fi

if [ ! -x "$BUILD_DIR/src/vsl-test-client" ]; then
    echo -e "${RED}ERROR: test_client executable not found${NC}"
    echo "Please build the project first."
    exit 1
fi

echo "==============================================================================="
echo "VideoStream Host/Client Integration Test"
echo "==============================================================================="
echo "Socket path: $SOCKET_PATH"
echo "Test frames: $NUM_FRAMES"
echo ""
echo "NOTE: This test requires DMA heap access or will use POSIX shared memory"
echo "      If you see permission errors, try running with sudo:"
echo "      sudo ./scripts/test_host_client.sh"
echo "==============================================================================="
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    
    # Kill host process if still running
    if [ ! -z "$HOST_PID" ]; then
        if kill -0 $HOST_PID 2>/dev/null; then
            echo "Stopping host (PID: $HOST_PID)..."
            kill $HOST_PID 2>/dev/null || true
            wait $HOST_PID 2>/dev/null || true
        fi
    fi
    
    # Remove socket file if it exists
    if [ -S "$SOCKET_PATH" ]; then
        rm -f "$SOCKET_PATH"
    fi
    
    echo "Done."
}

# Set up trap for cleanup on exit
trap cleanup EXIT INT TERM

# Start host in background
echo "Starting host..."
echo "-------------------------------------------------------------------------------"
"$BUILD_DIR/src/vsl-test-host" "$SOCKET_PATH" &
HOST_PID=$!

# Give host time to start up
sleep 1

# Check if host is still running
if ! kill -0 $HOST_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Host failed to start${NC}"
    wait $HOST_PID
    exit 1
fi

echo ""
echo "✓ Host started (PID: $HOST_PID)"
echo ""

# Wait a bit more to ensure socket is created
sleep 1

# Start client (runs in foreground)
echo "Starting client..."
echo "-------------------------------------------------------------------------------"
if "$BUILD_DIR/src/vsl-test-client" "$SOCKET_PATH" "$NUM_FRAMES"; then
    CLIENT_EXIT=$?
else
    CLIENT_EXIT=$?
fi

echo ""
echo "==============================================================================="

if [ $CLIENT_EXIT -eq 0 ]; then
    echo -e "${GREEN}✓ TEST PASSED${NC}"
    echo "  Host and client communicated successfully"
    echo "  $NUM_FRAMES frames transferred"
    exit 0
else
    echo -e "${RED}✗ TEST FAILED${NC}"
    echo "  Client exited with code: $CLIENT_EXIT"
    exit 1
fi
