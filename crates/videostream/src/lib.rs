// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! VideoStream Library for Rust
//!
//! Safe Rust bindings for the VideoStream Library, providing zero-copy video
//! frame management and distribution across processes and containers.
//!
//! The VideoStream Library enables efficient frame sharing through DMA buffers
//! or shared-memory with signaling over UNIX Domain Sockets, optimized for
//! edge AI and computer vision applications on resource-constrained embedded
//! devices.
//!
//! # Quick Start
//!
//! ## Publishing Frames (Host)
//!
//! ```no_run
//! use videostream::host::Host;
//! use videostream::frame::Frame;
//!
//! let host = Host::new("/tmp/video.sock")?;
//! let frame = Frame::new(1920, 1080, 1920 * 2, "YUYV")?;
//! frame.alloc(None)?;
//! // Register and publish frame to clients
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## Subscribing to Frames (Client)
//!
//! ```no_run
//! use videostream::client::Client;
//! use videostream::frame::Frame;
//!
//! let client = Client::new("/tmp/video.sock", true)?;
//! let frame = Frame::wait(&client, 1000)?;
//! // Process the frame here
//! println!("Received frame: {}x{}", frame.width()?, frame.height()?);
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! # Features
//!
//! - Zero-copy frame sharing across process boundaries
//! - DMA buffer support for hardware-accelerated access
//! - Hardware video encoding/decoding (H.264, H.265)
//! - V4L2 camera capture integration
//! - Multi-subscriber support (one publisher, many subscribers)
//!
//! # Support
//!
//! For questions and support:
//! - Documentation: <https://docs.rs/videostream>
//! - Repository: <https://github.com/EdgeFirstAI/videostream>
//! - Professional support: support@au-zone.com

use std::{
    error,
    ffi::{CStr, NulError},
    fmt, io,
    num::TryFromIntError,
    str,
};
use videostream_sys as ffi;

/// Error type for VideoStream library operations
#[derive(Debug)]
pub enum Error {
    /// The VideoStream library (libvideostream.so) could not be loaded at runtime
    LibraryNotLoaded(ffi::libloading::Error),

    /// I/O error from underlying system calls (errno-based errors from C library)
    Io(io::Error),

    /// UTF-8 conversion error when converting C strings to Rust strings
    Utf8(str::Utf8Error),

    /// CString creation error (null byte found in string)
    CString(NulError),

    /// Integer conversion error (try_from failed)
    TryFromInt(TryFromIntError),

    /// Null pointer returned from C library where valid pointer expected
    NullPointer,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::LibraryNotLoaded(err) => {
                write!(f, "VideoStream library could not be loaded: {}", err)
            }
            Error::Io(err) => write!(f, "I/O error: {}", err),
            Error::Utf8(err) => write!(f, "UTF-8 conversion error: {}", err),
            Error::CString(err) => write!(f, "CString creation error: {}", err),
            Error::TryFromInt(err) => write!(f, "Integer conversion error: {}", err),
            Error::NullPointer => write!(f, "Null pointer returned from VideoStream library"),
        }
    }
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::LibraryNotLoaded(err) => Some(err),
            Error::Io(err) => Some(err),
            Error::Utf8(err) => Some(err),
            Error::CString(err) => Some(err),
            Error::TryFromInt(err) => Some(err),
            Error::NullPointer => None,
        }
    }
}

impl From<ffi::libloading::Error> for Error {
    fn from(err: ffi::libloading::Error) -> Self {
        Error::LibraryNotLoaded(err)
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        Error::Io(err)
    }
}

impl From<str::Utf8Error> for Error {
    fn from(err: str::Utf8Error) -> Self {
        Error::Utf8(err)
    }
}

impl From<NulError> for Error {
    fn from(err: NulError) -> Self {
        Error::CString(err)
    }
}

impl From<TryFromIntError> for Error {
    fn from(err: TryFromIntError) -> Self {
        Error::TryFromInt(err)
    }
}

/// Helper macro for modules to get library reference and call functions
/// All functions must return Result<T, Error> to use this macro
#[macro_export]
macro_rules! vsl {
    ($fn_name:ident($($args:expr),*)) => {
        {
            #[allow(clippy::macro_metavars_in_unsafe)]
            let result = {
                let lib = videostream_sys::init()?;
                unsafe { lib.$fn_name($($args),*) }
            };
            result
        }
    };
}

/// The frame module provides the common frame handling functionality.
pub mod frame;

/// The client module provides the frame subscription functionality.
pub mod client;

/// The host module provides the frame sharing functionality.
pub mod host;

/// The encoder module provides accelerated video encoding to h.264 and h.265
pub mod encoder;

/// The encoder module provides accelerated video decoding from h.264 and h.265
pub mod decoder;

/// The camera module provides camera capture capabilities.
pub mod camera;

/// The fourcc module provides portable handling of fourcc codes.
pub mod fourcc;

/// Get the VideoStream library version string
///
/// Returns an error if the library is not loaded.
pub fn version() -> Result<String, Error> {
    let lib = ffi::init()?;
    let cstr = unsafe { CStr::from_ptr(lib.vsl_version()) };
    Ok(cstr.to_str()?.to_string())
}

/// Get the current timestamp in nanoseconds
///
/// Returns an error if the library is not loaded.
pub fn timestamp() -> Result<i64, Error> {
    let lib = ffi::init()?;
    Ok(unsafe { lib.vsl_timestamp() })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version() {
        match version() {
            Ok(ver) => println!("VideoStream Library version: {}", ver),
            Err(e) => println!("Failed to get version: {}", e),
        }
    }
}
