#!/bin/sh

# Generate FFI bindings with dynamic loading support
# Note: After generation, manually fix the trait bound from:
#   P: AsRef<::std::ffi::OsStr>
# To:
#   P: ::libloading::AsFilename
#
# This is due to a change in libloading 0.9+ that requires AsFilename

bindgen --dynamic-loading VideoStreamLibrary \
    --allowlist-function 'vsl_.*' \
    --allowlist-type 'vsl_.*' \
    --allowlist-var 'VSL.*' \
    ../../include/videostream.h > src/ffi.rs

# Fix the trait bound for libloading 0.9+
sed -i 's/P: AsRef<::std::ffi::OsStr>/P: ::libloading::AsFilename/' src/ffi.rs

# Add copyright header
sed -i '1s/^/\/\/ SPDX-License-Identifier: Apache-2.0\n\/\/ Copyright 2025 Au-Zone Technologies\n\/\/\n/' src/ffi.rs

echo "FFI bindings generated. Manual additions may be needed for helper methods."
