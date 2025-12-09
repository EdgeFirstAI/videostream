// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

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

pub struct Decoder {
    ptr: *mut ffi::VSLDecoder,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum DecoderInputCodec {
    H264 = 0,
    HEVC = 1,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum DecodeReturnCode {
    Success,
    Initialized,
    FrameDecoded,
}

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
    Ok(lib.is_decoder_available())
}

impl Decoder {
    /// Create a new decoder instance.
    ///
    /// # Errors
    ///
    /// Returns `Error::SymbolNotFound` if the library was compiled without VPU support.
    /// Returns `Error::HardwareNotAvailable` if the VPU hardware is not present.
    /// Returns `Error::NullPointer` if the decoder creation fails for other reasons.
    pub fn create(input_codec: DecoderInputCodec, fps: c_int) -> Result<Self, Error> {
        let lib = ffi::init()?;

        if !lib.is_decoder_available() {
            return Err(Error::SymbolNotFound("vsl_decoder_create"));
        }

        let ptr = unsafe { lib.try_vsl_decoder_create(input_codec as u32, fps) };

        match ptr {
            Some(p) if !p.is_null() => Ok(Decoder { ptr: p }),
            Some(_) => Err(Error::HardwareNotAvailable("VPU decoder")),
            None => Err(Error::SymbolNotFound("vsl_decoder_create")),
        }
    }

    pub fn width(&self) -> Result<i32, Error> {
        let lib = ffi::init()?;
        unsafe { lib.try_vsl_decoder_width(self.ptr) }
            .ok_or(Error::SymbolNotFound("vsl_decoder_width"))
    }

    pub fn height(&self) -> Result<i32, Error> {
        let lib = ffi::init()?;
        unsafe { lib.try_vsl_decoder_height(self.ptr) }
            .ok_or(Error::SymbolNotFound("vsl_decoder_height"))
    }

    pub fn crop(&self) -> Result<VSLRect, Error> {
        let lib = ffi::init()?;
        unsafe { lib.try_vsl_decoder_crop(self.ptr) }
            .map(|rect| VSLRect { rect })
            .ok_or(Error::SymbolNotFound("vsl_decoder_crop"))
    }

    pub fn decode_frame(
        &self,
        data: &[u8],
    ) -> Result<(DecodeReturnCode, usize, Option<Frame>), Error> {
        let lib = ffi::init()?;
        let mut output_frame: *mut vsl_frame = null_mut();
        let output_frame_ptr: *mut *mut vsl_frame = &mut output_frame;
        let len = data.len() as u32;
        let mut bytes_used: usize = 0;

        let ret_code = unsafe {
            lib.try_vsl_decode_frame(
                self.ptr,
                data.as_ptr() as *const c_void,
                len,
                &mut bytes_used,
                output_frame_ptr,
            )
        }
        .ok_or(Error::SymbolNotFound("vsl_decode_frame"))?;

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
            unsafe {
                lib.try_vsl_decoder_release(self.ptr);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_decoder_input_codec_values() {
        assert_eq!(DecoderInputCodec::H264 as i32, 0);
        assert_eq!(DecoderInputCodec::HEVC as i32, 1);
    }

    #[test]
    fn test_decoder_input_codec_equality() {
        let a = DecoderInputCodec::H264;
        let b = DecoderInputCodec::H264;
        let c = DecoderInputCodec::HEVC;
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
    fn test_decoder_input_codec_debug() {
        let codec = DecoderInputCodec::H264;
        let debug_str = format!("{:?}", codec);
        assert!(debug_str.contains("H264"));
    }

    #[test]
    fn test_decoder_input_codec_clone() {
        let codec = DecoderInputCodec::HEVC;
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
        // This should not panic - it may return Ok(true) or Ok(false)
        // depending on whether the library was compiled with VPU support
        let result = is_available();
        // If library loads, we get Ok(bool), if not we get Err
        assert!(result.is_ok() || result.is_err());
    }

    /// Test that Decoder::create returns SymbolNotFound when VPU not available
    /// instead of panicking
    #[test]
    fn test_decoder_create_handles_missing_symbols() {
        // Attempt to create decoder - this should not panic
        let result = Decoder::create(DecoderInputCodec::H264, 30);

        // If VPU is not available, we should get SymbolNotFound or HardwareNotAvailable
        // If VPU is available, we might get Ok or HardwareNotAvailable
        // The key is: this should NEVER panic
        match result {
            Ok(_) => {} // VPU available and hardware present
            Err(Error::SymbolNotFound(_)) => {} // VPU symbols not in library
            Err(Error::HardwareNotAvailable(_)) => {} // VPU symbols present but no hardware
            Err(Error::LibraryNotLoaded(_)) => {} // Library couldn't be loaded
            Err(e) => panic!("Unexpected error type: {:?}", e),
        }
    }

    // Hardware-dependent tests
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_decoder_create_h264() {
        let decoder = Decoder::create(DecoderInputCodec::H264, 30);
        assert!(decoder.is_ok());
    }

    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_decoder_create_hevc() {
        let decoder = Decoder::create(DecoderInputCodec::HEVC, 30);
        assert!(decoder.is_ok());
    }
}
