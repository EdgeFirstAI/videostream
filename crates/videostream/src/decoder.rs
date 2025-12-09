// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{encoder::VSLRect, frame::Frame, vsl, Error};
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

impl Decoder {
    pub fn create(input_codec: DecoderInputCodec, fps: c_int) -> Result<Self, Error> {
        Ok(Decoder {
            ptr: vsl!(vsl_decoder_create(input_codec as u32, fps)),
        })
    }

    pub fn width(&self) -> Result<i32, Error> {
        Ok(vsl!(vsl_decoder_width(self.ptr)))
    }

    pub fn height(&self) -> Result<i32, Error> {
        Ok(vsl!(vsl_decoder_height(self.ptr)))
    }

    pub fn crop(&self) -> Result<VSLRect, Error> {
        Ok(VSLRect {
            rect: vsl!(vsl_decoder_crop(self.ptr)),
        })
    }

    pub fn decode_frame(
        &self,
        data: &[u8],
    ) -> Result<(DecodeReturnCode, usize, Option<Frame>), Error> {
        let mut output_frame: *mut vsl_frame = null_mut();
        let output_frame_ptr: *mut *mut vsl_frame = &mut output_frame;
        let len = data.len() as u32;
        let mut bytes_used: usize = 0;
        let ret_code = vsl!(vsl_decode_frame(
            self.ptr,
            data.as_ptr() as *const c_void,
            len,
            &mut bytes_used,
            output_frame_ptr
        ));
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
                lib.vsl_decoder_release(self.ptr);
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
