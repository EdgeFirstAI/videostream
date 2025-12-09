// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{frame, vsl, Error};
use std::os::raw::c_int;
use videostream_sys as ffi;

pub struct Encoder {
    ptr: *mut ffi::VSLEncoder,
}

pub struct VSLEncoderProfile {
    _profile: ffi::VSLEncoderProfile,
}

pub struct VSLRect {
    pub(crate) rect: ffi::vsl_rect,
}

#[repr(u32)]
#[derive(Clone, Debug, PartialEq, Copy)]
pub enum VSLEncoderProfileEnum {
    Auto = ffi::vsl_encode_profile_VSL_ENCODE_PROFILE_AUTO,
    Kbps5000 = ffi::vsl_encode_profile_VSL_ENCODE_PROFILE_5000_KBPS,
    Kbps25000 = ffi::vsl_encode_profile_VSL_ENCODE_PROFILE_25000_KBPS,
    Kbps50000 = ffi::vsl_encode_profile_VSL_ENCODE_PROFILE_50000_KBPS,
    Kbps100000 = ffi::vsl_encode_profile_VSL_ENCODE_PROFILE_100000_KBPS,
}

impl VSLRect {
    pub fn new(x: c_int, y: c_int, width: c_int, height: c_int) -> Self {
        VSLRect {
            rect: ffi::vsl_rect {
                x,
                y,
                width,
                height,
            },
        }
    }

    pub fn width(&self) -> c_int {
        (self.rect).width
    }

    pub fn height(&self) -> c_int {
        (self.rect).height
    }

    pub fn x(&self) -> c_int {
        (self.rect).x
    }

    pub fn y(&self) -> c_int {
        (self.rect).y
    }
}

impl Encoder {
    pub fn create(profile: u32, output_fourcc: u32, fps: c_int) -> Result<Self, Error> {
        Ok(Encoder {
            ptr: vsl!(vsl_encoder_create(profile, output_fourcc, fps)),
        })
    }

    pub fn new_output_frame(
        &self,
        width: c_int,
        height: c_int,
        duration: i64,
        pts: i64,
        dts: i64,
    ) -> Result<frame::Frame, Error> {
        let frame_ptr = vsl!(vsl_encoder_new_output_frame(
            self.ptr, width, height, duration, pts, dts
        ));
        if frame_ptr.is_null() {
            return Err(Error::NullPointer);
        }
        match frame_ptr.try_into() {
            Ok(frame) => Ok(frame),
            Err(()) => Err(Error::NullPointer),
        }
    }

    /// # Safety
    /// The caller must ensure that `keyframe` is either null or points to a
    /// valid `c_int`.
    pub unsafe fn frame(
        &self,
        source: &frame::Frame,
        destination: &frame::Frame,
        crop_region: &VSLRect,
        keyframe: *mut c_int,
    ) -> Result<i32, Error> {
        Ok(vsl!(vsl_encode_frame(
            self.ptr,
            source.get_ptr(),
            destination.get_ptr(),
            &crop_region.rect,
            keyframe
        )))
    }
}

impl Drop for Encoder {
    fn drop(&mut self) {
        if let Ok(lib) = ffi::init() {
            unsafe {
                lib.vsl_encoder_release(self.ptr);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vsl_rect_new() {
        let rect = VSLRect::new(10, 20, 640, 480);
        assert_eq!(rect.x(), 10);
        assert_eq!(rect.y(), 20);
        assert_eq!(rect.width(), 640);
        assert_eq!(rect.height(), 480);
    }

    #[test]
    fn test_vsl_rect_zero() {
        let rect = VSLRect::new(0, 0, 0, 0);
        assert_eq!(rect.x(), 0);
        assert_eq!(rect.y(), 0);
        assert_eq!(rect.width(), 0);
        assert_eq!(rect.height(), 0);
    }

    #[test]
    fn test_vsl_rect_negative() {
        // Negative values should be allowed (for crop offsets)
        let rect = VSLRect::new(-10, -20, 640, 480);
        assert_eq!(rect.x(), -10);
        assert_eq!(rect.y(), -20);
    }

    #[test]
    fn test_encoder_profile_enum_values() {
        // Verify enum values match expected constants
        assert_eq!(VSLEncoderProfileEnum::Auto as u32, 0);
        assert_eq!(VSLEncoderProfileEnum::Kbps5000 as u32, 1);
        assert_eq!(VSLEncoderProfileEnum::Kbps25000 as u32, 2);
        assert_eq!(VSLEncoderProfileEnum::Kbps50000 as u32, 3);
        assert_eq!(VSLEncoderProfileEnum::Kbps100000 as u32, 4);
    }

    #[test]
    fn test_encoder_profile_enum_clone() {
        let profile = VSLEncoderProfileEnum::Kbps25000;
        let cloned = profile;
        assert_eq!(profile, cloned);
    }

    #[test]
    fn test_encoder_profile_enum_debug() {
        let profile = VSLEncoderProfileEnum::Kbps25000;
        let debug_str = format!("{:?}", profile);
        assert!(debug_str.contains("Kbps25000"));
    }

    #[test]
    fn test_encoder_profile_enum_copy() {
        let profile = VSLEncoderProfileEnum::Kbps50000;
        let copied = profile;
        assert_eq!(profile, copied);
    }

    #[test]
    fn test_encoder_profile_enum_equality() {
        let a = VSLEncoderProfileEnum::Auto;
        let b = VSLEncoderProfileEnum::Auto;
        let c = VSLEncoderProfileEnum::Kbps5000;
        assert_eq!(a, b);
        assert_ne!(a, c);
    }

    #[test]
    fn test_vsl_rect_large_values() {
        // Test with 4K resolution values
        let rect = VSLRect::new(0, 0, 3840, 2160);
        assert_eq!(rect.width(), 3840);
        assert_eq!(rect.height(), 2160);
    }

    // Hardware-dependent tests (marked with ignore)
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_encoder_create_h264() {
        let encoder = Encoder::create(
            VSLEncoderProfileEnum::Kbps25000 as u32,
            u32::from_le_bytes(*b"H264"),
            30,
        );
        assert!(encoder.is_ok());
    }

    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_encoder_create_hevc() {
        let encoder = Encoder::create(
            VSLEncoderProfileEnum::Kbps25000 as u32,
            u32::from_le_bytes(*b"HEVC"),
            30,
        );
        assert!(encoder.is_ok());
    }
}
