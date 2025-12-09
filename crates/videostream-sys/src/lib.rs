// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(clippy::type_complexity)]
#![allow(clippy::missing_safety_doc)]
#![allow(clippy::too_many_arguments)]

include!("ffi.rs");

// Re-export libloading for error handling
pub use libloading;

use std::sync::{Mutex, OnceLock};

// Store a leaked (never-freed) reference to the library to prevent dlclose() at program exit.
// This prevents segfaults when the C library or GStreamer plugins have global destructors
// or atexit() handlers that would run after the library is unloaded.
// Using a leaked Box ensures the library pointer remains valid for the entire program lifetime.
static LIBRARY: OnceLock<&'static VideoStreamLibrary> = OnceLock::new();
static INIT_LOCK: Mutex<()> = Mutex::new(());

/// Initialize the VideoStream library by loading libvideostream.so
///
/// This must be called before using any other VideoStream functions.
/// Returns an error if the library cannot be loaded.
///
/// The environment variable `VIDEOSTREAM_LIBRARY` can be used to specify
/// a custom path to the library. If not set, searches standard system paths.
pub fn init() -> Result<&'static VideoStreamLibrary, libloading::Error> {
    if let Some(lib) = LIBRARY.get() {
        return Ok(lib);
    }

    let _guard = INIT_LOCK.lock().unwrap();

    // Double-check after acquiring lock
    if let Some(lib) = LIBRARY.get() {
        return Ok(lib);
    }

    // Check for VIDEOSTREAM_LIBRARY environment variable
    let lib_path = std::env::var("VIDEOSTREAM_LIBRARY")
        .ok()
        .unwrap_or_else(|| "libvideostream.so".to_string());

    let lib = unsafe { VideoStreamLibrary::new(lib_path.as_str())? };

    // Leak the library to prevent dlclose() at program exit.
    // This intentionally leaks memory but prevents segfaults from cleanup code
    // trying to access unloaded library code.
    let leaked_lib: &'static VideoStreamLibrary = Box::leak(Box::new(lib));

    LIBRARY.set(leaked_lib).ok().expect("Failed to initialize library");

    Ok(*LIBRARY.get().unwrap())
}

/// Get a reference to the loaded library
///
/// Panics if init() has not been called successfully.
pub fn library() -> &'static VideoStreamLibrary {
    LIBRARY
        .get()
        .expect("VideoStream library not initialized - call videostream_sys::init() first")
}

/// Try to get a reference to the loaded library without panicking
pub fn try_library() -> Option<&'static VideoStreamLibrary> {
    LIBRARY.get().copied()
}
