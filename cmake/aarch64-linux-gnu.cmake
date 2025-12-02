# SPDX-License-Identifier: Apache-2.0
# Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

# CMake toolchain file for cross-compiling to aarch64 Linux (e.g., NXP i.MX8M Plus)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross compilers
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Search paths for target (don't search host paths)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Disable features that require target-specific libraries not available on host
set(ENABLE_GSTREAMER OFF CACHE BOOL "Disable GStreamer for cross-compilation" FORCE)
set(ENABLE_VPU OFF CACHE BOOL "Disable VPU for cross-compilation" FORCE)
set(ENABLE_G2D OFF CACHE BOOL "Disable G2D for cross-compilation" FORCE)
