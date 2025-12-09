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

static LIBRARY: OnceLock<VideoStreamLibrary> = OnceLock::new();
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

    LIBRARY.set(lib).ok().expect("Failed to initialize library");

    Ok(LIBRARY.get().unwrap())
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
    LIBRARY.get()
}
