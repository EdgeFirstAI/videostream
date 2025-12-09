// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! VideoStream Library for Rust
//!
//! Safe Rust bindings for the VideoStream Library, providing zero-copy video
//! frame management and distribution across processes and containers on embedded Linux.
//!
//! The VideoStream Library enables efficient frame sharing through DMA buffers
//! or shared-memory with signaling over UNIX Domain Sockets, optimized for
//! edge AI and computer vision applications on resource-constrained embedded
//! devices like NXP i.MX8M Plus.
//!
//! # Architecture
//!
//! VideoStream uses a **Host/Client** pattern for inter-process communication:
//!
//! - **Host**: Publishes video frames to a UNIX socket
//! - **Clients**: Subscribe to frames by connecting to the socket
//! - **Frames**: Zero-copy shared memory (DmaBuf or POSIX shm) with metadata
//!
//! # Quick Start
//!
//! ## Publishing Frames (Host)
//!
//! ```no_run
//! use videostream::{host::Host, frame::Frame, Error};
//!
//! fn publish_frames() -> Result<(), Error> {
//!     // Create host on UNIX socket
//!     let host = Host::new("/tmp/video.sock")?;
//!     
//!     // Create and allocate a frame
//!     let frame = Frame::new(1920, 1080, 1920 * 2, "YUYV")?;
//!     frame.alloc(None)?; // DmaBuf or shared memory
//!     
//!     // Frame is now ready to be posted to clients
//!     // (Posting requires additional host.post() - see host module)
//!     Ok(())
//! }
//! # publish_frames().ok();
//! ```
//!
//! ## Subscribing to Frames (Client)
//!
//! ```no_run
//! use videostream::{client::{Client, Reconnect}, frame::Frame, Error};
//!
//! fn subscribe_frames() -> Result<(), Error> {
//!     // Connect to host socket (auto-reconnect on disconnect)
//!     let client = Client::new("/tmp/video.sock", Reconnect::Yes)?;
//!     
//!     // Wait for next frame (blocking)
//!     let frame = Frame::wait(&client, 0)?;
//!     
//!     // Lock frame before accessing
//!     frame.trylock()?;
//!     println!("Frame: {}x{}", frame.width()?, frame.height()?);
//!     frame.unlock()?;
//!     
//!     Ok(())
//! }
//! # subscribe_frames().ok();
//! ```
//!
//! ## Camera Capture
//!
//! ```no_run
//! use videostream::{camera::create_camera, fourcc::FourCC, Error};
//!
//! fn capture_camera() -> Result<(), Error> {
//!     // Configure and open camera
//!     let cam = create_camera()
//!         .with_device("/dev/video0")
//!         .with_resolution(1920, 1080)
//!         .with_format(FourCC(*b"YUYV"))
//!         .open()?;
//!     
//!     cam.start()?;
//!     let buffer = cam.read()?;
//!     println!("Captured: {}", buffer);
//!     
//!     Ok(())
//! }
//! # capture_camera().ok();
//! ```
//!
//! ## Hardware Encoding
//!
//! ```no_run
//! use videostream::{encoder::{Encoder, VSLEncoderProfileEnum}, Error};
//!
//! fn encode_video() -> Result<(), Error> {
//!     // Create H.264 encoder (Hantro VPU on i.MX8)
//!     let encoder = Encoder::create(
//!         VSLEncoderProfileEnum::Kbps25000 as u32,
//!         u32::from_le_bytes(*b"H264"),
//!         30 // fps
//!     )?;
//!     
//!     // Source and destination frames required for encoding
//!     // (See encoder module for complete example)
//!     Ok(())
//! }
//! # encode_video().ok();
//! ```
//!
//! # Features
//!
//! - **Zero-copy sharing**: DmaBuf or POSIX shared memory for minimal overhead
//! - **Hardware acceleration**: G2D format conversion, VPU encoding/decoding
//! - **Multi-subscriber**: One host can serve many clients simultaneously
//! - **V4L2 camera**: Native Linux camera capture with DmaBuf export
//! - **Cross-process**: UNIX sockets enable containerized applications
//!
//! # Platform Support
//!
//! - **Primary**: NXP i.MX8M Plus (full hardware acceleration)
//! - **Compatible**: Any Linux system with V4L2 (software fallback)
//! - **Kernel**: Linux 4.14+ (5.6+ recommended for DmaBuf heap)
//!
//! # Error Handling
//!
//! All fallible operations return [`Result<T, Error>`]. The [`Error`] enum provides
//! detailed error information including I/O errors (with errno), library loading
//! failures, and type conversion errors.
//!
//! # Safety
//!
//! This crate wraps unsafe C FFI calls with safe Rust abstractions. All public
//! APIs are safe to use. Unsafe blocks are carefully isolated in the FFI layer.
//!
//! # Support
//!
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
#[non_exhaustive]
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

    /// Required symbol not found in the library (e.g., encoder/decoder when VPU not compiled)
    SymbolNotFound(&'static str),

    /// Hardware not available (e.g., VPU hardware not present on the system)
    HardwareNotAvailable(&'static str),
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
            Error::SymbolNotFound(symbol) => {
                write!(
                    f,
                    "Symbol '{}' not found in library (VPU support may not be compiled)",
                    symbol
                )
            }
            Error::HardwareNotAvailable(hw) => {
                write!(f, "Hardware '{}' not available on this system", hw)
            }
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
            Error::SymbolNotFound(_) => None,
            Error::HardwareNotAvailable(_) => None,
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

/// Helper macro for calling C library functions safely.
///
/// This macro handles library initialization and wraps unsafe FFI calls.
/// All functions must return `Result<T, Error>` to propagate library loading errors.
///
/// # Internal Use Only
///
/// This macro is exported for use by submodules but is not part of the public API.
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

/// Frame management for video data.
///
/// Provides the [`Frame`](frame::Frame) type for creating, allocating, and
/// manipulating video frames. Frames can be free-standing or shared via Host/Client.
pub mod frame;

/// Client API for subscribing to video frames.
///
/// Provides the [`Client`](client::Client) type for connecting to a
/// [`Host`](host::Host) and receiving published frames.
pub mod client;

/// Host API for publishing video frames.
///
/// Provides the [`Host`](host::Host) type for managing a UNIX socket server
/// that publishes frames to connected clients.
pub mod host;

/// Hardware-accelerated video encoding (H.264/H.265).
///
/// Provides the [`Encoder`](encoder::Encoder) type for compressing video frames
/// using the Hantro VPU on i.MX8 platforms.
pub mod encoder;

/// Hardware-accelerated video decoding (H.264/H.265).
///
/// Provides the [`Decoder`](decoder::Decoder) type for decompressing video streams
/// using the Hantro VPU on i.MX8 platforms.
pub mod decoder;

/// V4L2 camera capture with DmaBuf support.
///
/// Provides the [`Camera`](camera::Camera) and [`CameraReader`](camera::CameraReader)
/// types for capturing frames from Linux V4L2 video devices.
pub mod camera;

/// FOURCC pixel format codes.
///
/// Provides the [`FourCC`](fourcc::FourCC) type for portable handling of
/// four-character-code pixel formats (e.g., "YUYV", "NV12").
pub mod fourcc;

/// Returns the VideoStream Library version string.
///
/// The version follows semantic versioning (MAJOR.MINOR.PATCH).
///
/// # Errors
///
/// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
///
/// # Example
///
/// ```no_run
/// use videostream::version;
///
/// let ver = version().expect("Failed to get version");
/// println!("VideoStream version: {}", ver);
/// ```
pub fn version() -> Result<String, Error> {
    let lib = ffi::init()?;
    let cstr = unsafe { CStr::from_ptr(lib.vsl_version()) };
    Ok(cstr.to_str()?.to_string())
}

/// Returns the current monotonic timestamp in nanoseconds.
///
/// Uses `CLOCK_MONOTONIC` for consistent timing across the system.
/// This timestamp is used internally for frame timing and synchronization.
///
/// # Errors
///
/// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
///
/// # Example
///
/// ```no_run
/// use videostream::timestamp;
///
/// let ts = timestamp().expect("Failed to get timestamp");
/// println!("Current time: {} ns", ts);
/// ```
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

    #[test]
    fn test_timestamp() {
        let result = timestamp();
        assert!(result.is_ok(), "timestamp() should succeed");
        let ts = result.unwrap();
        assert!(ts >= 0, "timestamp should be non-negative");
    }

    #[test]
    fn test_error_display_io() {
        let io_err = Error::Io(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            "test error",
        ));
        let display = format!("{}", io_err);
        assert!(
            display.contains("test error") || display.contains("I/O error"),
            "Display should contain error message"
        );
    }

    #[test]
    fn test_error_display_null_pointer() {
        let err = Error::NullPointer;
        let display = format!("{}", err);
        assert!(
            display.contains("Null pointer"),
            "Display should mention null pointer"
        );
    }

    #[test]
    fn test_error_from_io() {
        let io_err = std::io::Error::new(std::io::ErrorKind::PermissionDenied, "no access");
        let err: Error = io_err.into();
        assert!(matches!(err, Error::Io(_)));
    }

    #[test]
    fn test_error_from_nul() {
        // Create a NulError by trying to create a CString with embedded null
        let nul_result = std::ffi::CString::new("test\0string");
        assert!(nul_result.is_err());
        let nul_err = nul_result.unwrap_err();
        let err: Error = nul_err.into();
        assert!(matches!(err, Error::CString(_)));
    }

    #[test]
    fn test_error_from_utf8() {
        // Create a Utf8Error by converting invalid UTF-8
        let invalid_utf8 = vec![0xff, 0xfe];
        let utf8_result = std::str::from_utf8(&invalid_utf8);
        assert!(utf8_result.is_err());
        let utf8_err = utf8_result.unwrap_err();
        let err: Error = utf8_err.into();
        assert!(matches!(err, Error::Utf8(_)));
    }

    #[test]
    fn test_error_from_try_from_int() {
        // Create a TryFromIntError by converting an out-of-range value
        let result: Result<u8, _> = (-1i32).try_into();
        assert!(result.is_err());
        let int_err = result.unwrap_err();
        let err: Error = int_err.into();
        assert!(matches!(err, Error::TryFromInt(_)));
    }

    #[test]
    fn test_error_debug() {
        let err = Error::NullPointer;
        let debug_str = format!("{:?}", err);
        assert!(debug_str.contains("NullPointer"));
    }

    #[test]
    fn test_error_source() {
        use std::error::Error as StdError;

        // NullPointer should have no source
        let null_err = Error::NullPointer;
        assert!(null_err.source().is_none());

        // Io error should have a source
        let io_err = Error::Io(std::io::Error::new(std::io::ErrorKind::Other, "test"));
        assert!(io_err.source().is_some());
    }

    #[test]
    fn test_error_display_utf8() {
        let invalid_utf8 = vec![0xff, 0xfe];
        let utf8_err = std::str::from_utf8(&invalid_utf8).unwrap_err();
        let err: Error = utf8_err.into();
        let display = format!("{}", err);
        assert!(
            display.contains("UTF-8"),
            "Display should mention UTF-8 error"
        );
    }

    #[test]
    fn test_error_display_cstring() {
        let nul_err = std::ffi::CString::new("test\0string").unwrap_err();
        let err: Error = nul_err.into();
        let display = format!("{}", err);
        assert!(
            display.contains("CString"),
            "Display should mention CString error"
        );
    }

    #[test]
    fn test_error_display_try_from_int() {
        let result: Result<u8, _> = (-1i32).try_into();
        let int_err = result.unwrap_err();
        let err: Error = int_err.into();
        let display = format!("{}", err);
        assert!(
            display.contains("Integer") || display.contains("conversion"),
            "Display should mention integer conversion error"
        );
    }

    #[test]
    fn test_error_display_symbol_not_found() {
        let err = Error::SymbolNotFound("vsl_encoder_create");
        let display = format!("{}", err);
        assert!(display.contains("vsl_encoder_create"));
        assert!(display.contains("Symbol"));
    }

    #[test]
    fn test_error_display_hardware_not_available() {
        let err = Error::HardwareNotAvailable("VPU encoder");
        let display = format!("{}", err);
        assert!(display.contains("VPU encoder"));
        assert!(display.contains("Hardware"));
    }
}
