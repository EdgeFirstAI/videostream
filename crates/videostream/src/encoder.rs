// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{frame, Error};
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

/// Codec backend selection for encoder.
///
/// Allows explicit selection between V4L2 kernel driver and Hantro user-space
/// library (libcodec.so) backends for encoding.
///
/// @since 2.0
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u32)]
pub enum CodecBackend {
    /// Auto-detect best available backend (default).
    #[default]
    Auto = ffi::VSLCodecBackend_VSL_CODEC_BACKEND_AUTO,

    /// Force Hantro/libcodec.so backend.
    Hantro = ffi::VSLCodecBackend_VSL_CODEC_BACKEND_HANTRO,

    /// Force V4L2 kernel driver backend.
    V4L2 = ffi::VSLCodecBackend_VSL_CODEC_BACKEND_V4L2,
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

/// Check if the encoder functionality is available in the loaded library.
///
/// Returns `true` if the library was compiled with VPU encoder support,
/// `false` otherwise. This should be checked before attempting to create
/// an encoder on systems where VPU support may not be available.
///
/// # Example
///
/// ```no_run
/// use videostream::encoder;
///
/// if encoder::is_available().unwrap_or(false) {
///     // Safe to create encoder
/// } else {
///     // Encoder not available, use alternative
/// }
/// ```
pub fn is_available() -> Result<bool, Error> {
    let lib = ffi::init()?;
    Ok(lib.vsl_encoder_create.is_ok())
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
    /// Create a new encoder instance.
    ///
    /// # Errors
    ///
    /// Returns `Error::SymbolNotFound` if the library was compiled without VPU support.
    /// Returns `Error::HardwareNotAvailable` if the VPU hardware is not present.
    /// Returns `Error::NullPointer` if the encoder creation fails for other reasons.
    pub fn create(profile: u32, output_fourcc: u32, fps: c_int) -> Result<Self, Error> {
        let lib = ffi::init()?;

        if lib.vsl_encoder_create.is_err() {
            return Err(Error::SymbolNotFound("vsl_encoder_create"));
        }

        let ptr = unsafe { lib.vsl_encoder_create(profile, output_fourcc, fps) };

        if ptr.is_null() {
            Err(Error::HardwareNotAvailable("VPU encoder"))
        } else {
            Ok(Encoder { ptr })
        }
    }

    /// Create a new encoder instance with explicit backend selection.
    ///
    /// This allows choosing between V4L2 and Hantro backends explicitly.
    /// Requires VideoStream 2.0 or later.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::encoder::{Encoder, VSLEncoderProfileEnum, CodecBackend};
    ///
    /// // Force V4L2 backend for encoding
    /// let encoder = Encoder::create_ex(
    ///     VSLEncoderProfileEnum::Kbps25000 as u32,
    ///     u32::from_le_bytes(*b"H264"),
    ///     30,
    ///     CodecBackend::V4L2,
    /// )?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    ///
    /// # Errors
    ///
    /// Returns `Error::SymbolNotFound` if vsl_encoder_create_ex is not available.
    /// Returns `Error::HardwareNotAvailable` if the encoder backend is not present.
    pub fn create_ex(
        profile: u32,
        output_fourcc: u32,
        fps: c_int,
        backend: CodecBackend,
    ) -> Result<Self, Error> {
        let lib = ffi::init()?;

        if lib.vsl_encoder_create_ex.is_err() {
            return Err(Error::SymbolNotFound("vsl_encoder_create_ex"));
        }

        let ptr = unsafe {
            lib.vsl_encoder_create_ex(profile, output_fourcc, fps, backend as ffi::VSLCodecBackend)
        };

        if ptr.is_null() {
            Err(Error::HardwareNotAvailable("VPU encoder"))
        } else {
            Ok(Encoder { ptr })
        }
    }

    pub fn new_output_frame(
        &self,
        width: c_int,
        height: c_int,
        duration: i64,
        pts: i64,
        dts: i64,
    ) -> Result<frame::Frame, Error> {
        let lib = ffi::init()?;

        if lib.vsl_encoder_new_output_frame.is_err() {
            return Err(Error::SymbolNotFound("vsl_encoder_new_output_frame"));
        }

        let frame_ptr = unsafe {
            lib.vsl_encoder_new_output_frame(self.ptr, width, height, duration, pts, dts)
        };

        // Safety: vsl_encoder_new_output_frame transfers ownership of a new
        // frame reference to the caller on success. Null is handled by
        // `ok_or` and indicates failure.
        unsafe { frame::Frame::from_raw(frame_ptr) }.ok_or(Error::NullPointer)
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
        let lib = ffi::init()?;

        if lib.vsl_encode_frame.is_err() {
            return Err(Error::SymbolNotFound("vsl_encode_frame"));
        }

        // Safety: forwarded from the enclosing unsafe fn's contract -
        // `keyframe` is either null or points to a valid `c_int`. The frame
        // pointers are non-null borrows from `source` and `destination`.
        let result = unsafe {
            lib.vsl_encode_frame(
                self.ptr,
                source.as_ptr(),
                destination.as_ptr(),
                &crop_region.rect,
                keyframe,
            )
        };

        Ok(result)
    }
}

impl Drop for Encoder {
    fn drop(&mut self) {
        if let Ok(lib) = ffi::init() {
            if lib.vsl_encoder_release.is_ok() {
                unsafe {
                    lib.vsl_encoder_release(self.ptr);
                }
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

    /// Test that is_available() returns a valid result without panicking
    /// This test works regardless of whether VPU support is compiled in
    #[test]
    fn test_encoder_is_available_returns_result() {
        // This should not panic - it may return Ok(true) or Ok(false)
        // depending on whether the library was compiled with VPU support
        let result = is_available();
        // If library loads, we get Ok(bool), if not we get Err
        assert!(
            result.is_ok(),
            "is_available should always return Ok if the library is loaded correctly"
        );
    }

    /// Test that Encoder::create returns SymbolNotFound when VPU not available
    /// instead of panicking
    #[test]
    fn test_encoder_create_handles_missing_symbols() {
        // Attempt to create encoder - this should not panic
        let result = Encoder::create(
            VSLEncoderProfileEnum::Kbps25000 as u32,
            u32::from_le_bytes(*b"H264"),
            30,
        );

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

    /// Helper for crop tests: allocate a DMA-BUF backed source frame of the
    /// requested format and an encoder destination frame, returning both.
    fn make_crop_test_frames(
        encoder: &Encoder,
        width: i32,
        height: i32,
        fourcc: &str,
    ) -> Result<(crate::frame::Frame, crate::frame::Frame), Error> {
        let source = crate::frame::Frame::new(width as u32, height as u32, 0, fourcc)?;
        source.alloc(None)?;
        let dest = encoder.new_output_frame(width, height, -1, -1, -1)?;
        Ok((source, dest))
    }

    /// Crop region exceeding source dimensions must be rejected by
    /// `latch_init_crop`'s bounds check (`lib/encoder_v4l2.c`).
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_encoder_crop_out_of_bounds_rejected() {
        let encoder = Encoder::create_ex(
            VSLEncoderProfileEnum::Kbps5000 as u32,
            u32::from_le_bytes(*b"H264"),
            30,
            CodecBackend::V4L2,
        )
        .unwrap();

        let (source, dest) = make_crop_test_frames(&encoder, 640, 480, "BGRA").unwrap();

        // Crop region exceeds source — 700x500 doesn't fit in 640x480.
        let bad_crop = VSLRect::new(0, 0, 700, 500);
        let mut keyframe: c_int = 0;
        let result = unsafe {
            encoder
                .frame(&source, &dest, &bad_crop, &mut keyframe)
                .unwrap()
        };
        assert_eq!(result, -1, "encoder must reject out-of-bounds crop region");
    }

    /// YUYV input with an odd `crop.x` is invalid (YUYV pixels come in
    /// 2-pixel macroblocks). `latch_init_crop` must reject it before
    /// configuring `S_FMT`.
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_encoder_crop_yuyv_odd_x_rejected() {
        let encoder = Encoder::create_ex(
            VSLEncoderProfileEnum::Kbps5000 as u32,
            u32::from_le_bytes(*b"H264"),
            30,
            CodecBackend::V4L2,
        )
        .unwrap();

        let (source, dest) = make_crop_test_frames(&encoder, 640, 480, "YUYV").unwrap();

        // Crop.x = 1 is odd — must be rejected for YUYV.
        let bad_crop = VSLRect::new(1, 0, 320, 240);
        let mut keyframe: c_int = 0;
        let result = unsafe {
            encoder
                .frame(&source, &dest, &bad_crop, &mut keyframe)
                .unwrap()
        };
        assert_eq!(result, -1, "encoder must reject odd crop.x for YUYV input");
    }

    /// NV12 (and other planar formats) require per-plane `data_offset`
    /// handling that this code path doesn't yet implement; `latch_init_crop`
    /// must return `EINVAL` rather than silently misaddress the Y/UV planes.
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_encoder_crop_nv12_rejected() {
        let encoder = Encoder::create_ex(
            VSLEncoderProfileEnum::Kbps5000 as u32,
            u32::from_le_bytes(*b"H264"),
            30,
            CodecBackend::V4L2,
        )
        .unwrap();

        let (source, dest) = make_crop_test_frames(&encoder, 640, 480, "NV12").unwrap();

        let crop = VSLRect::new(0, 0, 320, 240);
        let mut keyframe: c_int = 0;
        let result = unsafe { encoder.frame(&source, &dest, &crop, &mut keyframe).unwrap() };
        assert_eq!(
            result, -1,
            "encoder must reject crop_region on NV12 input until \
             per-plane offsets are implemented"
        );
    }

    /// After the encoder has latched its crop dimensions on the first
    /// frame (V4L2 `S_FMT` is one-shot), a subsequent frame with
    /// different crop width/height must be rejected by
    /// `validate_crop_for_frame`. The position (`x`, `y`) can vary —
    /// only the dimensions are locked — so we additionally verify that
    /// changing position alone is accepted.
    #[ignore = "test requires VPU hardware"]
    #[test]
    fn test_encoder_crop_dim_mismatch_after_init() {
        let encoder = Encoder::create_ex(
            VSLEncoderProfileEnum::Kbps5000 as u32,
            u32::from_le_bytes(*b"H264"),
            30,
            CodecBackend::V4L2,
        )
        .unwrap();

        let (source, dest) = make_crop_test_frames(&encoder, 640, 480, "BGRA").unwrap();

        // First frame at 320x240 establishes the locked dims.
        let crop_a = VSLRect::new(0, 0, 320, 240);
        let mut keyframe: c_int = 0;
        let first = unsafe {
            encoder
                .frame(&source, &dest, &crop_a, &mut keyframe)
                .unwrap()
        };
        assert!(
            first >= 0,
            "first frame with valid crop must succeed (got {})",
            first
        );

        // Same dims, different position — must be accepted.
        let crop_a_shifted = VSLRect::new(320, 240, 320, 240);
        let shifted = unsafe {
            encoder
                .frame(&source, &dest, &crop_a_shifted, &mut keyframe)
                .unwrap()
        };
        assert!(
            shifted >= 0,
            "per-call position change with same dims must succeed (got {})",
            shifted
        );

        // Now change the dimensions — must be rejected.
        let crop_b = VSLRect::new(0, 0, 400, 240);
        let mismatched = unsafe {
            encoder
                .frame(&source, &dest, &crop_b, &mut keyframe)
                .unwrap()
        };
        assert_eq!(
            mismatched, -1,
            "subsequent frame with different crop dimensions must be \
             rejected (V4L2 S_FMT is one-shot)"
        );
    }
}
