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
            return Err(Error::Io(io::Error::new(io::ErrorKind::Other, "Decoder Error")));
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
