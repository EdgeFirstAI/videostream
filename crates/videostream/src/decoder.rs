// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! Hardware video decoder for H.264/H.265 streams.
//!
//! This module provides a safe Rust interface to the VideoStream library's
//! hardware decoder functionality, supporting both V4L2 and Hantro backends.
//!
//! # Example
//!
//! ```no_run
//! use videostream::decoder::{Decoder, DecoderCodec, DecodeReturnCode};
//!
//! let decoder = Decoder::create(DecoderCodec::H264, 30)?;
//!
//! // Decode a frame from H.264 NAL unit data
//! let h264_data: &[u8] = &[/* ... */];
//! match decoder.decode_frame(h264_data) {
//!     Ok((code, bytes_used, Some(frame))) => {
//!         println!("Decoded frame: {}x{}", frame.width()?, frame.height()?);
//!     }
//!     Ok((code, bytes_used, None)) => {
//!         println!("No frame yet, consumed {} bytes", bytes_used);
//!     }
//!     Err(e) => eprintln!("Decode error: {}", e),
//! }
//! # Ok::<(), videostream::Error>(())
//! ```

use crate::{encoder::VSLRect, frame::Frame, Error};
use std::{
    ffi::{c_int, c_void},
    io,
    ptr::null_mut,
};
use videostream_sys::{
    self as ffi, vsl_frame, VSLDecoderRetCode_VSL_DEC_ERR, VSLDecoderRetCode_VSL_DEC_FRAME_DEC,
    VSLDecoderRetCode_VSL_DEC_INIT_INFO,
};

/// Hardware video decoder instance.
///
/// The decoder processes H.264 or H.265 NAL units and produces decoded frames.
/// It automatically selects the best available backend (V4L2 or Hantro) unless
/// explicitly specified via [`Decoder::create_ex`].
pub struct Decoder {
    ptr: *mut ffi::VSLDecoder,
}

/// Video codec type for hardware decoder.
///
/// Specifies which video compression standard to use for decoding.
/// Both codecs are supported via hardware acceleration on i.MX8.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum DecoderCodec {
    /// H.264/AVC (Advanced Video Coding) codec.
    ///
    /// Widely supported standard with good compression and compatibility.
    /// Recommended for maximum device compatibility.
    H264 = ffi::VSLDecoderCodec_VSL_DEC_H264,

    /// H.265/HEVC (High Efficiency Video Coding) codec.
    ///
    /// Next-generation standard providing approximately 50% better compression
    /// than H.264 at equivalent quality.
    HEVC = ffi::VSLDecoderCodec_VSL_DEC_HEVC,
}

impl DecoderCodec {
    /// Convert codec enum to fourcc value for create_ex API.
    #[inline]
    fn to_fourcc(self) -> u32 {
        match self {
            DecoderCodec::H264 => fourcc(b"H264"),
            DecoderCodec::HEVC => fourcc(b"HEVC"),
        }
    }
}

/// Create a fourcc value from a 4-byte array.
#[inline]
const fn fourcc(bytes: &[u8; 4]) -> u32 {
    (bytes[0] as u32)
        | ((bytes[1] as u32) << 8)
        | ((bytes[2] as u32) << 16)
        | ((bytes[3] as u32) << 24)
}

/// Codec backend selection for encoder/decoder.
///
/// Allows selection between V4L2 kernel driver and Hantro user-space
/// library (libcodec.so) backends.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u32)]
pub enum CodecBackend {
    /// Auto-detect best available backend (default).
    ///
    /// Selection priority:
    /// 1. Check `VSL_CODEC_BACKEND` environment variable
    /// 2. Prefer V4L2 if device available and has M2M capability
    /// 3. Fall back to Hantro if V4L2 unavailable
    #[default]
    Auto = ffi::VSLCodecBackend_VSL_CODEC_BACKEND_AUTO,

    /// Force Hantro/libcodec.so backend.
    ///
    /// Uses the proprietary VPU wrapper library.
    Hantro = ffi::VSLCodecBackend_VSL_CODEC_BACKEND_HANTRO,

    /// Force V4L2 kernel driver backend.
    ///
    /// Uses the vsi_v4l2 mem2mem driver for better performance.
    V4L2 = ffi::VSLCodecBackend_VSL_CODEC_BACKEND_V4L2,
}

/// Return code from decode operations.
///
/// These codes can be combined (bitfield), but this enum represents
/// the primary status for convenience.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DecodeReturnCode {
    /// Decode succeeded but no frame or initialization info available yet.
    Success,
    /// Decoder has been initialized with stream parameters.
    Initialized,
    /// A decoded frame is available.
    FrameDecoded,
}

// Type alias for backwards compatibility
#[doc(hidden)]
#[deprecated(since = "2.0.0", note = "Use DecoderCodec instead")]
pub type DecoderInputCodec = DecoderCodec;

/// Check if the decoder functionality is available in the loaded library.
///
/// Returns `true` if the library was compiled with VPU decoder support,
/// `false` otherwise. This should be checked before attempting to create
/// a decoder on systems where VPU support may not be available.
///
/// # Example
///
/// ```no_run
/// use videostream::decoder;
///
/// if decoder::is_available().unwrap_or(false) {
///     // Safe to create decoder
/// } else {
///     // Decoder not available, use alternative
/// }
/// ```
pub fn is_available() -> Result<bool, Error> {
    let lib = ffi::init()?;
    Ok(lib.vsl_decoder_create.is_ok())
}

/// Check if explicit backend selection is available.
///
/// Returns `true` if `vsl_decoder_create_ex` is available, allowing
/// the use of [`Decoder::create_ex`] for explicit backend selection.
///
/// Older library versions may not support this function.
pub fn is_backend_selection_available() -> Result<bool, Error> {
    let lib = ffi::init()?;
    Ok(lib.vsl_decoder_create_ex.is_ok())
}

impl Decoder {
    /// Create a new decoder instance with automatic backend selection.
    ///
    /// # Arguments
    ///
    /// * `codec` - The video codec type (H.264 or H.265)
    /// * `fps` - Expected frame rate (used for buffer management)
    ///
    /// # Errors
    ///
    /// Returns `Error::SymbolNotFound` if the library was compiled without VPU support.
    /// Returns `Error::HardwareNotAvailable` if the VPU hardware is not present.
    /// Returns `Error::NullPointer` if the decoder creation fails for other reasons.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::decoder::{Decoder, DecoderCodec};
    ///
    /// let decoder = Decoder::create(DecoderCodec::H264, 30)?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn create(codec: DecoderCodec, fps: c_int) -> Result<Self, Error> {
        let lib = ffi::init()?;

        if lib.vsl_decoder_create.is_err() {
            return Err(Error::SymbolNotFound("vsl_decoder_create"));
        }

        let ptr = unsafe { lib.vsl_decoder_create(codec as ffi::VSLDecoderCodec, fps) };

        if ptr.is_null() {
            Err(Error::HardwareNotAvailable("VPU decoder"))
        } else {
            Ok(Decoder { ptr })
        }
    }

    /// Create a new decoder instance with explicit backend selection.
    ///
    /// # Arguments
    ///
    /// * `codec` - The video codec type (H.264 or H.265)
    /// * `fps` - Expected frame rate (used for buffer management)
    /// * `backend` - Which backend to use (Auto, Hantro, or V4L2)
    ///
    /// # Errors
    ///
    /// Returns `Error::SymbolNotFound` if the library doesn't support backend selection.
    /// Returns `Error::HardwareNotAvailable` if the specified backend is unavailable.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::decoder::{Decoder, DecoderCodec, CodecBackend};
    ///
    /// // Force V4L2 backend for better performance
    /// let decoder = Decoder::create_ex(DecoderCodec::H264, 30, CodecBackend::V4L2)?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn create_ex(
        codec: DecoderCodec,
        fps: c_int,
        backend: CodecBackend,
    ) -> Result<Self, Error> {
        let lib = ffi::init()?;

        if lib.vsl_decoder_create_ex.is_err() {
            return Err(Error::SymbolNotFound("vsl_decoder_create_ex"));
        }

        let ptr = unsafe {
            lib.vsl_decoder_create_ex(codec.to_fourcc(), fps, backend as ffi::VSLCodecBackend)
        };

        if ptr.is_null() {
            Err(Error::HardwareNotAvailable("VPU decoder"))
        } else {
            Ok(Decoder { ptr })
        }
    }

    /// Returns the width of decoded frames in pixels.
    ///
    /// Only valid after decoder initialization (after first [`decode_frame`](Self::decode_frame)).
    pub fn width(&self) -> Result<i32, Error> {
        let lib = ffi::init()?;
        if lib.vsl_decoder_width.is_err() {
            return Err(Error::SymbolNotFound("vsl_decoder_width"));
        }
        Ok(unsafe { lib.vsl_decoder_width(self.ptr) })
    }

    /// Returns the height of decoded frames in pixels.
    ///
    /// Only valid after decoder initialization (after first [`decode_frame`](Self::decode_frame)).
    pub fn height(&self) -> Result<i32, Error> {
        let lib = ffi::init()?;
        if lib.vsl_decoder_height.is_err() {
            return Err(Error::SymbolNotFound("vsl_decoder_height"));
        }
        Ok(unsafe { lib.vsl_decoder_height(self.ptr) })
    }

    /// Returns the crop region for decoded frames.
    ///
    /// The crop region indicates the valid pixel area within the decoded frame,
    /// which may be smaller than the full frame dimensions due to codec alignment
    /// requirements.
    pub fn crop(&self) -> Result<VSLRect, Error> {
        let lib = ffi::init()?;
        // vsl_decoder_crop returns directly, not via Result
        let rect = unsafe { lib.vsl_decoder_crop(self.ptr) };
        Ok(VSLRect { rect })
    }

    /// Decodes a frame from compressed video data.
    ///
    /// # Arguments
    ///
    /// * `data` - H.264/H.265 NAL unit data to decode
    ///
    /// # Returns
    ///
    /// A tuple containing:
    /// * `DecodeReturnCode` - Status of the decode operation
    /// * `usize` - Number of bytes consumed from the input data
    /// * `Option<Frame>` - Decoded frame if available
    ///
    /// # Errors
    ///
    /// Returns an error if the decoder encounters an unrecoverable error.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::decoder::{Decoder, DecoderCodec, DecodeReturnCode};
    ///
    /// let decoder = Decoder::create(DecoderCodec::H264, 30)?;
    /// let h264_data: &[u8] = &[/* NAL unit data */];
    ///
    /// match decoder.decode_frame(h264_data) {
    ///     Ok((DecodeReturnCode::FrameDecoded, _, Some(frame))) => {
    ///         println!("Got frame: {}x{}", frame.width()?, frame.height()?);
    ///     }
    ///     Ok((DecodeReturnCode::FrameDecoded, _, None)) => {
    ///         println!("Frame decoded but not ready for output yet");
    ///     }
    ///     Ok((DecodeReturnCode::Initialized, bytes, _)) => {
    ///         println!("Decoder initialized, consumed {} bytes", bytes);
    ///     }
    ///     Ok((DecodeReturnCode::Success, bytes, _)) => {
    ///         println!("Need more data, consumed {} bytes", bytes);
    ///     }
    ///     Err(e) => eprintln!("Error: {}", e),
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn decode_frame(
        &self,
        data: &[u8],
    ) -> Result<(DecodeReturnCode, usize, Option<Frame>), Error> {
        let lib = ffi::init()?;

        if lib.vsl_decode_frame.is_err() {
            return Err(Error::SymbolNotFound("vsl_decode_frame"));
        }

        let mut output_frame: *mut vsl_frame = null_mut();
        let output_frame_ptr: *mut *mut vsl_frame = &mut output_frame;
        let len = data.len() as u32;
        let mut bytes_used: usize = 0;

        let ret_code = unsafe {
            lib.vsl_decode_frame(
                self.ptr,
                data.as_ptr() as *const c_void,
                len,
                &mut bytes_used,
                output_frame_ptr,
            )
        };

        let output_frame = Frame::wrap(output_frame).ok();
        if ret_code & VSLDecoderRetCode_VSL_DEC_ERR > 0 {
            return Err(Error::Io(io::Error::new(
                io::ErrorKind::Other,
                "Decoder Error",
            )));
        }
        let mut return_msg = DecodeReturnCode::Success;
        if ret_code & VSLDecoderRetCode_VSL_DEC_FRAME_DEC > 0 {
            return_msg = DecodeReturnCode::FrameDecoded;
        }

        if ret_code & VSLDecoderRetCode_VSL_DEC_INIT_INFO > 0 {
            return_msg = DecodeReturnCode::Initialized;
        }

        Ok((return_msg, bytes_used, output_frame))
    }
}

impl Drop for Decoder {
    fn drop(&mut self) {
        if let Ok(lib) = ffi::init() {
            if lib.vsl_decoder_release.is_ok() {
                unsafe {
                    lib.vsl_decoder_release(self.ptr);
                }
            }
        }
    }
}

// Safety: Decoder uses a thread-safe C API
unsafe impl Send for Decoder {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_decoder_codec_values() {
        assert_eq!(DecoderCodec::H264 as u32, 0);
        assert_eq!(DecoderCodec::HEVC as u32, 1);
    }

    #[test]
    fn test_codec_backend_values() {
        assert_eq!(CodecBackend::Auto as u32, 0);
        assert_eq!(CodecBackend::Hantro as u32, 1);
        assert_eq!(CodecBackend::V4L2 as u32, 2);
    }

    #[test]
    fn test_codec_backend_default() {
        let backend = CodecBackend::default();
        assert_eq!(backend, CodecBackend::Auto);
    }

    #[test]
    fn test_decoder_codec_equality() {
        let a = DecoderCodec::H264;
        let b = DecoderCodec::H264;
        let c = DecoderCodec::HEVC;
        assert_eq!(a, b);
        assert_ne!(a, c);
    }

    #[test]
    fn test_decode_return_code_equality() {
        let a = DecodeReturnCode::Success;
        let b = DecodeReturnCode::Success;
        let c = DecodeReturnCode::FrameDecoded;
        assert_eq!(a, b);
        assert_ne!(a, c);
    }

    #[test]
    fn test_decode_return_code_debug() {
        let code = DecodeReturnCode::FrameDecoded;
        let debug_str = format!("{:?}", code);
        assert!(debug_str.contains("FrameDecoded"));
    }

    #[test]
    fn test_decoder_codec_debug() {
        let codec = DecoderCodec::H264;
        let debug_str = format!("{:?}", codec);
        assert!(debug_str.contains("H264"));
    }

    #[test]
    fn test_codec_backend_debug() {
        let backend = CodecBackend::V4L2;
        let debug_str = format!("{:?}", backend);
        assert!(debug_str.contains("V4L2"));
    }

    #[test]
    fn test_decoder_codec_clone() {
        let codec = DecoderCodec::HEVC;
        let cloned = codec;
        assert_eq!(codec, cloned);
    }

    #[test]
    fn test_decode_return_code_clone() {
        let code = DecodeReturnCode::Initialized;
        let cloned = code;
        assert_eq!(code, cloned);
    }

    #[test]
    fn test_decode_return_code_all_variants() {
        // Test all variants can be created and compared
        let success = DecodeReturnCode::Success;
        let initialized = DecodeReturnCode::Initialized;
        let frame_decoded = DecodeReturnCode::FrameDecoded;

        assert_ne!(success, initialized);
        assert_ne!(initialized, frame_decoded);
        assert_ne!(success, frame_decoded);
    }

    /// Test that is_available() returns a valid result without panicking
    /// This test works regardless of whether VPU support is compiled in
    #[test]
    fn test_decoder_is_available_returns_result() {
        // This should not panic - it may return Ok(true), Ok(false), or Err
        // depending on whether the library is available and supports VPU
        let result = is_available();
        // The key assertion is that this doesn't panic
        match result {
            Ok(available) => {
                // Library loaded - result indicates VPU support
                log::debug!("Decoder available: {}", available);
            }
            Err(Error::LibraryNotLoaded(_)) => {
                // Library not installed - acceptable on development hosts
            }
            Err(e) => {
                panic!("Unexpected error from is_available(): {:?}", e);
            }
        }
    }

    /// Test that Decoder::create returns SymbolNotFound when VPU not available
    /// instead of panicking
    #[test]
    fn test_decoder_create_handles_missing_symbols() {
        // Attempt to create decoder - this should not panic
        let result = Decoder::create(DecoderCodec::H264, 30);

        // If VPU is not available, we should get SymbolNotFound or HardwareNotAvailable
        // If VPU is available, we might get Ok or HardwareNotAvailable
        // The key is: this should NEVER panic
        match result {
            Ok(_) => {}                               // VPU available and hardware present
            Err(Error::SymbolNotFound(_)) => {}       // VPU symbols not in library
            Err(Error::HardwareNotAvailable(_)) => {} // VPU symbols present but no hardware
            Err(Error::LibraryNotLoaded(_)) => {}     // Library couldn't be loaded
            Err(e) => panic!("Unexpected error type: {:?}", e),
        }
    }

    // Hardware-dependent tests
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_decoder_create_h264() {
        let decoder = Decoder::create(DecoderCodec::H264, 30);
        assert!(decoder.is_ok());
    }

    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_decoder_create_hevc() {
        let decoder = Decoder::create(DecoderCodec::HEVC, 30);
        assert!(decoder.is_ok());
    }

    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_decoder_create_ex_v4l2() {
        let decoder = Decoder::create_ex(DecoderCodec::H264, 30, CodecBackend::V4L2);
        assert!(decoder.is_ok());
    }

    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_decoder_create_ex_hantro() {
        let decoder = Decoder::create_ex(DecoderCodec::H264, 30, CodecBackend::Hantro);
        assert!(decoder.is_ok());
    }
}
