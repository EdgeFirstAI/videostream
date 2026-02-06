// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! V4L2 Device Enumerator
//!
//! This module provides the [`DeviceEnumerator`] type for discovering V4L2 video
//! devices on Linux systems.

use std::ffi::CStr;

use crate::fourcc::FourCC;
use crate::Error;
use videostream_sys as ffi;

use super::device::{Device, DeviceType};

/// V4L2 Device Enumerator
///
/// Provides static methods for discovering and querying V4L2 video devices on the system.
/// This is the main entry point for the V4L2 device discovery API.
///
/// # Features
///
/// - Enumerate all V4L2 devices with capabilities and formats
/// - Filter by device type (camera, encoder, decoder, ISP)
/// - Auto-detect devices by codec (H.264, HEVC) or pixel format (NV12, YUYV)
/// - Find cameras with specific resolution requirements
///
/// # Example: List All Devices
///
/// ```no_run
/// use videostream::v4l2::{DeviceEnumerator, DeviceType};
///
/// let devices = DeviceEnumerator::enumerate()?;
/// println!("Found {} V4L2 devices:", devices.len());
///
/// for device in &devices {
///     println!("  {}: {} ({:?})",
///         device.path_str(), device.card(), device.device_type());
/// }
/// # Ok::<(), videostream::Error>(())
/// ```
///
/// # Example: Find Encoders and Decoders
///
/// ```no_run
/// use videostream::v4l2::{DeviceEnumerator, DeviceType};
///
/// // List all encoders
/// let encoders = DeviceEnumerator::enumerate_type(DeviceType::Encoder)?;
/// for enc in encoders {
///     println!("Encoder: {} ({})", enc.card(), enc.path_str());
///     for fmt in enc.capture_formats() {
///         println!("  Output: {}", fmt.fourcc);
///     }
/// }
///
/// // List all decoders
/// let decoders = DeviceEnumerator::enumerate_type(DeviceType::Decoder)?;
/// for dec in decoders {
///     println!("Decoder: {} ({})", dec.card(), dec.path_str());
/// }
/// # Ok::<(), videostream::Error>(())
/// ```
///
/// # Example: Auto-Detect Devices
///
/// ```no_run
/// use videostream::v4l2::DeviceEnumerator;
///
/// // Find H.264 encoder (for recording)
/// if let Some(h264_enc) = DeviceEnumerator::find_encoder(b"H264")? {
///     println!("H.264 encoder: {}", h264_enc);
/// }
///
/// // Find HEVC encoder (higher quality)
/// if let Some(hevc_enc) = DeviceEnumerator::find_encoder(b"HEVC")? {
///     println!("HEVC encoder: {}", hevc_enc);
/// }
///
/// // Find H.264 decoder (for playback)
/// if let Some(h264_dec) = DeviceEnumerator::find_decoder(b"H264")? {
///     println!("H.264 decoder: {}", h264_dec);
/// }
///
/// // Find NV12 camera
/// if let Some(camera) = DeviceEnumerator::find_camera(b"NV12")? {
///     println!("NV12 camera: {}", camera);
/// }
///
/// // Find 1080p camera
/// if let Some(camera) = DeviceEnumerator::find_camera_with_resolution(
///     b"NV12", 1920, 1080)?
/// {
///     println!("1080p camera: {}", camera);
/// }
/// # Ok::<(), videostream::Error>(())
/// ```
///
/// # Platform Support
///
/// The device enumerator works on any Linux system with V4L2 support. It has been
/// tested on:
///
/// - NXP i.MX8M Plus (Hantro VPU for encode/decode)
/// - NXP i.MX95 (Wave6 VPU for encode/decode)
/// - Generic x86_64 (USB webcams, software codecs)
///
/// # Errors
///
/// Most methods return `Result<_, Error>` where errors can occur if:
///
/// - The VideoStream library is not loaded ([`Error::LibraryNotLoaded`])
/// - A required symbol is not found ([`Error::SymbolNotFound`])
/// - The underlying C function fails (check errno)
pub struct DeviceEnumerator;

impl DeviceEnumerator {
    /// Enumerate all V4L2 video devices on the system.
    ///
    /// Scans `/dev/video*` and queries each device's capabilities using
    /// `VIDIOC_QUERYCAP`. Devices are classified by type based on their
    /// capabilities and supported formats.
    ///
    /// # Returns
    ///
    /// A vector of [`Device`] descriptors for all accessible V4L2 devices.
    /// The vector may be empty if no devices are found or accessible.
    ///
    /// # Errors
    ///
    /// Returns an error if the VideoStream library cannot be loaded or the
    /// enumeration function is not available.
    ///
    /// # Notes
    ///
    /// - Devices that are busy (`EBUSY`) or inaccessible are silently skipped
    /// - Format enumeration is performed automatically
    /// - The returned devices are sorted by device number
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::v4l2::DeviceEnumerator;
    ///
    /// let devices = DeviceEnumerator::enumerate()?;
    /// println!("Found {} devices", devices.len());
    ///
    /// for device in devices {
    ///     println!("{}: {} - {:?}",
    ///         device.path_str(),
    ///         device.card(),
    ///         device.device_type());
    ///
    ///     // Check memory capabilities
    ///     let caps = device.capture_memory();
    ///     println!("  Memory: MMAP={}, DMABUF={}", caps.mmap, caps.dmabuf);
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn enumerate() -> Result<Vec<Device>, Error> {
        let lib = ffi::init()?;

        let enumerate_fn = lib
            .vsl_v4l2_enumerate
            .as_ref()
            .map_err(|_| Error::SymbolNotFound("vsl_v4l2_enumerate"))?;

        let list_ptr = unsafe { enumerate_fn() };
        if list_ptr.is_null() {
            return Ok(Vec::new());
        }

        let result = Self::parse_device_list(list_ptr);

        // Free the list
        if let Ok(free_fn) = lib.vsl_v4l2_device_list_free.as_ref() {
            unsafe { free_fn(list_ptr) };
        }

        result
    }

    /// Enumerate V4L2 devices filtered by type.
    ///
    /// Same as [`enumerate()`](Self::enumerate) but only returns devices matching
    /// the specified type.
    ///
    /// # Arguments
    ///
    /// * `device_type` - Type of devices to enumerate
    ///
    /// # Returns
    ///
    /// A vector of [`Device`] descriptors matching the requested type.
    ///
    /// # Errors
    ///
    /// Returns an error if the VideoStream library cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::v4l2::{DeviceEnumerator, DeviceType};
    ///
    /// // Get only cameras
    /// let cameras = DeviceEnumerator::enumerate_type(DeviceType::Camera)?;
    /// println!("Found {} cameras", cameras.len());
    ///
    /// // Get only encoders
    /// let encoders = DeviceEnumerator::enumerate_type(DeviceType::Encoder)?;
    /// for enc in encoders {
    ///     println!("Encoder: {} at {}", enc.card(), enc.path_str());
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn enumerate_type(device_type: DeviceType) -> Result<Vec<Device>, Error> {
        let lib = ffi::init()?;

        let enumerate_fn = lib
            .vsl_v4l2_enumerate_type
            .as_ref()
            .map_err(|_| Error::SymbolNotFound("vsl_v4l2_enumerate_type"))?;

        let list_ptr = unsafe { enumerate_fn(device_type as u32) };
        if list_ptr.is_null() {
            return Ok(Vec::new());
        }

        let result = Self::parse_device_list(list_ptr);

        // Free the list
        if let Ok(free_fn) = lib.vsl_v4l2_device_list_free.as_ref() {
            unsafe { free_fn(list_ptr) };
        }

        result
    }

    /// Find an encoder device that supports a specific output codec.
    ///
    /// Searches for a hardware encoder that can produce the specified compressed
    /// format. This is useful for applications that need to encode video without
    /// knowing the specific device path.
    ///
    /// # Arguments
    ///
    /// * `codec` - Four-character code for the output codec:
    ///   - `b"H264"` - H.264/AVC
    ///   - `b"HEVC"` - H.265/HEVC
    ///   - `b"MJPG"` - Motion JPEG
    ///   - `b"VP80"` - VP8
    ///   - `b"VP90"` - VP9
    ///
    /// # Returns
    ///
    /// The device path (e.g., `"/dev/video10"`) if an encoder is found,
    /// or `None` if no matching encoder exists on the system.
    ///
    /// # Errors
    ///
    /// Returns an error if the VideoStream library cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::v4l2::DeviceEnumerator;
    ///
    /// // Try to find H.264 encoder first, fall back to HEVC
    /// let encoder_path = DeviceEnumerator::find_encoder(b"H264")?
    ///     .or(DeviceEnumerator::find_encoder(b"HEVC")?);
    ///
    /// match encoder_path {
    ///     Some(path) => println!("Using encoder: {}", path),
    ///     None => println!("No hardware encoder found"),
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn find_encoder(codec: &[u8; 4]) -> Result<Option<String>, Error> {
        let lib = ffi::init()?;

        let find_fn = lib
            .vsl_v4l2_find_encoder
            .as_ref()
            .map_err(|_| Error::SymbolNotFound("vsl_v4l2_find_encoder"))?;

        let fourcc = FourCC(*codec).as_u32();
        let ptr = unsafe { find_fn(fourcc) };

        if ptr.is_null() {
            Ok(None)
        } else {
            let path = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
            Ok(Some(path))
        }
    }

    /// Find a decoder device that supports a specific input codec.
    ///
    /// Searches for a hardware decoder that can accept the specified compressed
    /// format as input.
    ///
    /// # Arguments
    ///
    /// * `codec` - Four-character code for the input codec (same values as
    ///   [`find_encoder`](Self::find_encoder))
    ///
    /// # Returns
    ///
    /// The device path if a decoder is found, or `None` if no matching decoder exists.
    ///
    /// # Errors
    ///
    /// Returns an error if the VideoStream library cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::v4l2::DeviceEnumerator;
    ///
    /// // Find decoder for H.264 playback
    /// if let Some(path) = DeviceEnumerator::find_decoder(b"H264")? {
    ///     println!("H.264 decoder: {}", path);
    /// } else {
    ///     println!("No H.264 decoder available, using software decode");
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn find_decoder(codec: &[u8; 4]) -> Result<Option<String>, Error> {
        let lib = ffi::init()?;

        let find_fn = lib
            .vsl_v4l2_find_decoder
            .as_ref()
            .map_err(|_| Error::SymbolNotFound("vsl_v4l2_find_decoder"))?;

        let fourcc = FourCC(*codec).as_u32();
        let ptr = unsafe { find_fn(fourcc) };

        if ptr.is_null() {
            Ok(None)
        } else {
            let path = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
            Ok(Some(path))
        }
    }

    /// Find a camera device that supports a specific pixel format.
    ///
    /// Searches for a camera (video capture device) that can produce frames
    /// in the specified pixel format.
    ///
    /// # Arguments
    ///
    /// * `format` - Four-character code for the pixel format:
    ///   - `b"NV12"` - YUV 4:2:0 semi-planar (most common for hardware pipelines)
    ///   - `b"YUYV"` - YUV 4:2:2 packed
    ///   - `b"MJPG"` - Motion JPEG (compressed)
    ///   - `b"BA24"` - RGB24/BGR3
    ///
    /// # Returns
    ///
    /// The device path if a camera is found, or `None` if no matching camera exists.
    ///
    /// # Errors
    ///
    /// Returns an error if the VideoStream library cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::v4l2::DeviceEnumerator;
    ///
    /// // Find camera with NV12 support (best for hardware encode pipeline)
    /// if let Some(path) = DeviceEnumerator::find_camera(b"NV12")? {
    ///     println!("NV12 camera: {}", path);
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn find_camera(format: &[u8; 4]) -> Result<Option<String>, Error> {
        let lib = ffi::init()?;

        let find_fn = lib
            .vsl_v4l2_find_camera
            .as_ref()
            .map_err(|_| Error::SymbolNotFound("vsl_v4l2_find_camera"))?;

        let fourcc = FourCC(*format).as_u32();
        let ptr = unsafe { find_fn(fourcc) };

        if ptr.is_null() {
            Ok(None)
        } else {
            let path = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
            Ok(Some(path))
        }
    }

    /// Find a camera device that supports a specific format and minimum resolution.
    ///
    /// Searches for a camera that supports the specified pixel format at the
    /// given resolution or higher.
    ///
    /// # Arguments
    ///
    /// * `format` - Four-character code for the pixel format
    /// * `width` - Minimum width in pixels (0 for any width)
    /// * `height` - Minimum height in pixels (0 for any height)
    ///
    /// # Returns
    ///
    /// The device path if a camera is found, or `None` if no matching camera exists.
    ///
    /// # Errors
    ///
    /// Returns an error if the VideoStream library cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::v4l2::DeviceEnumerator;
    ///
    /// // Find 1080p camera
    /// if let Some(path) = DeviceEnumerator::find_camera_with_resolution(
    ///     b"NV12", 1920, 1080)?
    /// {
    ///     println!("1080p NV12 camera: {}", path);
    /// }
    ///
    /// // Find 4K camera
    /// if let Some(path) = DeviceEnumerator::find_camera_with_resolution(
    ///     b"NV12", 3840, 2160)?
    /// {
    ///     println!("4K NV12 camera: {}", path);
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn find_camera_with_resolution(
        format: &[u8; 4],
        width: u32,
        height: u32,
    ) -> Result<Option<String>, Error> {
        let lib = ffi::init()?;

        let find_fn = lib
            .vsl_v4l2_find_camera_with_resolution
            .as_ref()
            .map_err(|_| Error::SymbolNotFound("vsl_v4l2_find_camera_with_resolution"))?;

        let fourcc = FourCC(*format).as_u32();
        let ptr = unsafe { find_fn(fourcc, width, height) };

        if ptr.is_null() {
            Ok(None)
        } else {
            let path = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
            Ok(Some(path))
        }
    }

    /// Parse a VSLDeviceList into a Vec<Device>
    fn parse_device_list(list_ptr: *mut ffi::VSLDeviceList) -> Result<Vec<Device>, Error> {
        let list = unsafe { &*list_ptr };
        let mut devices = Vec::with_capacity(list.count);

        if !list.devices.is_null() && list.count > 0 {
            let device_slice = unsafe { std::slice::from_raw_parts(list.devices, list.count) };
            for ffi_dev in device_slice {
                if let Ok(device) = Device::from_ffi(ffi_dev) {
                    devices.push(device);
                }
            }
        }

        Ok(devices)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_device_type_name() {
        assert_eq!(DeviceType::Camera.name(), "Camera");
        assert_eq!(DeviceType::Encoder.name(), "Encoder");
        assert_eq!(DeviceType::Decoder.name(), "Decoder");
    }

    #[test]
    fn test_device_type_display() {
        assert_eq!(format!("{}", DeviceType::Camera), "Camera");
        assert_eq!(format!("{}", DeviceType::Encoder), "Encoder");
    }
}
