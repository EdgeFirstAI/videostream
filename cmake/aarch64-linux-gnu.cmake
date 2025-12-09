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

# Disable GStreamer for cross-compilation (requires target GStreamer dev packages)
set(ENABLE_GSTREAMER OFF CACHE BOOL "Disable GStreamer for cross-compilation" FORCE)

# VPU and G2D can be enabled because they use dlopen to load libraries at runtime.
# The Hantro libraries (libhantro.so.1, libhantro_vc8000e.so.1, libcodec.so.1) are
# not needed at compile time - they're loaded dynamically on the target device.
# Default to ON for aarch64 targets which typically have VPU/G2D hardware.
if(NOT DEFINED ENABLE_VPU)
    set(ENABLE_VPU ON CACHE BOOL "Enable VPU for cross-compilation (uses dlopen)" FORCE)
endif()
if(NOT DEFINED ENABLE_G2D)
    set(ENABLE_G2D ON CACHE BOOL "Enable G2D for cross-compilation (uses dlopen)" FORCE)
endif()
