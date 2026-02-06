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

        if frame_ptr.is_null() {
            Err(Error::NullPointer)
        } else {
            frame_ptr.try_into().map_err(|()| Error::NullPointer)
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
        let lib = ffi::init()?;

        if lib.vsl_encode_frame.is_err() {
            return Err(Error::SymbolNotFound("vsl_encode_frame"));
        }

        let result = lib.vsl_encode_frame(
            self.ptr,
            source.get_ptr(),
            destination.get_ptr(),
            &crop_region.rect,
            keyframe,
        );

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
}
