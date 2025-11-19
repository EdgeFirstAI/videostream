// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

// Phase 2: Dynamic loading via libloading
// No compile-time linking required - library loaded at runtime

fn main() {
    // Using libloading for runtime dynamic library loading
    // No rustc-link-lib directive needed - prevents compile-time linking requirement

    // Tell Cargo to rerun this build script if the header changes
    // (useful if regenerating bindings with update.sh)
    println!("cargo:rerun-if-changed=../../include/videostream.h");
}
