// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! V4L2 Device types and structures
//!
//! This module defines the core types used for V4L2 device discovery:
//!
//! - [`DeviceType`] - Classification of V4L2 devices (camera, encoder, decoder, etc.)
//! - [`MemoryType`] - Buffer memory types supported by devices (MMAP, USERPTR, DMABUF)
//! - [`MemoryCapabilities`] - Boolean flags for memory type support
//! - [`Format`] - Pixel format descriptor with fourcc and resolutions
//! - [`Resolution`] - Width × height descriptor
//! - [`Device`] - Complete device descriptor with all capabilities

use std::ffi::CStr;
use std::fmt;
use std::path::PathBuf;

use crate::fourcc::FourCC;
use crate::Error;
use videostream_sys as ffi;

/// V4L2 device type classification
///
/// Devices are classified based on their V4L2 capabilities and the types of
/// pixel formats they support on input/output queues:
///
/// - **Camera**: Has `VIDEO_CAPTURE` capability, produces video frames
/// - **Encoder**: M2M device that accepts raw formats and produces compressed output
/// - **Decoder**: M2M device that accepts compressed input and produces raw frames
/// - **ISP**: M2M device that processes raw frames (scaling, color conversion)
///
/// # Example
///
/// ```no_run
/// use videostream::v4l2::{DeviceEnumerator, DeviceType};
///
/// let devices = DeviceEnumerator::enumerate()?;
/// for device in devices {
///     match device.device_type() {
///         DeviceType::Camera => println!("Camera: {}", device.card()),
///         DeviceType::Encoder => println!("Encoder: {}", device.card()),
///         DeviceType::Decoder => println!("Decoder: {}", device.card()),
///         _ => {}
///     }
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u32)]
pub enum DeviceType {
    /// Video capture device (camera, frame grabber)
    ///
    /// Has `V4L2_CAP_VIDEO_CAPTURE` or `V4L2_CAP_VIDEO_CAPTURE_MPLANE` capability.
    /// Produces video frames that can be captured by applications.
    Camera = ffi::VSLDeviceType_VSL_V4L2_TYPE_CAMERA,

    /// Video output device (display, transmitter)
    ///
    /// Has `V4L2_CAP_VIDEO_OUTPUT` capability.
    /// Accepts video frames for display or transmission.
    Output = ffi::VSLDeviceType_VSL_V4L2_TYPE_OUTPUT,

    /// Video encoder (M2M: raw input → compressed output)
    ///
    /// Memory-to-memory device that accepts raw pixel formats (NV12, YUYV, etc.)
    /// on its output queue and produces compressed bitstream (H.264, HEVC, etc.)
    /// on its capture queue.
    Encoder = ffi::VSLDeviceType_VSL_V4L2_TYPE_ENCODER,

    /// Video decoder (M2M: compressed input → raw output)
    ///
    /// Memory-to-memory device that accepts compressed bitstream (H.264, HEVC, etc.)
    /// on its output queue and produces raw pixel formats on its capture queue.
    Decoder = ffi::VSLDeviceType_VSL_V4L2_TYPE_DECODER,

    /// Image/video processor (M2M: raw input → raw output)
    ///
    /// Memory-to-memory device for image processing operations like scaling,
    /// rotation, color space conversion, etc. Both input and output are raw formats.
    Isp = ffi::VSLDeviceType_VSL_V4L2_TYPE_ISP,

    /// Memory-to-memory device (generic)
    ///
    /// Generic M2M device that doesn't fit into encoder, decoder, or ISP categories.
    M2m = ffi::VSLDeviceType_VSL_V4L2_TYPE_M2M,

    /// Unknown device type
    ///
    /// Device type could not be determined from capabilities.
    Unknown = 0xFF,
}

impl DeviceType {
    /// Convert from raw FFI value
    ///
    /// # Arguments
    ///
    /// * `raw` - Raw VSLDeviceType value from C API
    ///
    /// # Returns
    ///
    /// The corresponding [`DeviceType`], or [`DeviceType::Unknown`] for unrecognized values.
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            ffi::VSLDeviceType_VSL_V4L2_TYPE_CAMERA => Some(DeviceType::Camera),
            ffi::VSLDeviceType_VSL_V4L2_TYPE_OUTPUT => Some(DeviceType::Output),
            ffi::VSLDeviceType_VSL_V4L2_TYPE_ENCODER => Some(DeviceType::Encoder),
            ffi::VSLDeviceType_VSL_V4L2_TYPE_DECODER => Some(DeviceType::Decoder),
            ffi::VSLDeviceType_VSL_V4L2_TYPE_ISP => Some(DeviceType::Isp),
            ffi::VSLDeviceType_VSL_V4L2_TYPE_M2M => Some(DeviceType::M2m),
            _ => Some(DeviceType::Unknown),
        }
    }

    /// Get human-readable name for this device type
    ///
    /// # Returns
    ///
    /// A static string: "Camera", "Output", "Encoder", "Decoder", "ISP", "M2M", or "Unknown"
    pub fn name(&self) -> &'static str {
        match self {
            DeviceType::Camera => "Camera",
            DeviceType::Output => "Output",
            DeviceType::Encoder => "Encoder",
            DeviceType::Decoder => "Decoder",
            DeviceType::Isp => "ISP",
            DeviceType::M2m => "M2M",
            DeviceType::Unknown => "Unknown",
        }
    }
}

impl fmt::Display for DeviceType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name())
    }
}

/// V4L2 memory type capabilities
///
/// Indicates which buffer memory types a device supports for streaming I/O.
/// Different devices support different combinations of memory types.
///
/// | Memory Type | Allocation | Zero-Copy Potential |
/// |-------------|------------|---------------------|
/// | [`MemoryType::Mmap`] | Kernel | No (copy required) |
/// | [`MemoryType::UserPtr`] | User | Yes (if DMA-capable) |
/// | [`MemoryType::DmaBuf`] | User | Yes |
///
/// For best performance, prefer [`MemoryType::DmaBuf`] when available for zero-copy
/// pipeline between camera, encoder, and display.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u32)]
pub enum MemoryType {
    /// Memory-mapped buffers
    ///
    /// Kernel allocates buffers, user mmaps them. This is the most compatible
    /// mode but requires a copy if passing buffers between devices.
    Mmap = ffi::VSLMemoryType_VSL_V4L2_MEM_MMAP,

    /// User pointer buffers
    ///
    /// User allocates buffers and passes pointers. Can be zero-copy if the
    /// buffer is allocated from DMA-capable memory (see `vsl_v4l2_alloc_userptr`).
    UserPtr = ffi::VSLMemoryType_VSL_V4L2_MEM_USERPTR,

    /// DMA buffer file descriptors
    ///
    /// User passes dmabuf file descriptors. This enables zero-copy buffer
    /// sharing between hardware components (camera → encoder → display).
    DmaBuf = ffi::VSLMemoryType_VSL_V4L2_MEM_DMABUF,
}

impl MemoryType {
    /// Convert from raw FFI bitmask
    ///
    /// The C API represents memory capabilities as a bitmask of `VSLMemoryType` values.
    /// This method extracts the individual types into a vector.
    ///
    /// # Arguments
    ///
    /// * `mask` - Bitmask of VSLMemoryType values
    ///
    /// # Returns
    ///
    /// Vector of memory types that are supported (flags set in mask).
    pub fn from_mask(mask: u32) -> Vec<Self> {
        let mut types = Vec::new();
        if mask & ffi::VSLMemoryType_VSL_V4L2_MEM_MMAP != 0 {
            types.push(MemoryType::Mmap);
        }
        if mask & ffi::VSLMemoryType_VSL_V4L2_MEM_USERPTR != 0 {
            types.push(MemoryType::UserPtr);
        }
        if mask & ffi::VSLMemoryType_VSL_V4L2_MEM_DMABUF != 0 {
            types.push(MemoryType::DmaBuf);
        }
        types
    }
}

impl fmt::Display for MemoryType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            MemoryType::Mmap => write!(f, "MMAP"),
            MemoryType::UserPtr => write!(f, "USERPTR"),
            MemoryType::DmaBuf => write!(f, "DMABUF"),
        }
    }
}

/// Memory capability flags for a V4L2 queue
///
/// This struct provides boolean flags for each memory type, which is often
/// more convenient than working with [`MemoryType`] slices.
///
/// # Example
///
/// ```no_run
/// use videostream::v4l2::DeviceEnumerator;
///
/// let devices = DeviceEnumerator::enumerate()?;
/// for device in devices {
///     let caps = device.capture_memory();
///     if caps.dmabuf {
///         println!("{}: supports zero-copy DMABUF", device.card());
///     }
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
#[derive(Debug, Clone, Copy, Default)]
pub struct MemoryCapabilities {
    /// Supports memory-mapped buffers
    pub mmap: bool,
    /// Supports user pointer buffers
    pub userptr: bool,
    /// Supports DMA buffer file descriptors
    pub dmabuf: bool,
}

impl MemoryCapabilities {
    /// Create from memory type slice
    ///
    /// # Arguments
    ///
    /// * `types` - Slice of supported memory types
    pub fn from_types(types: &[MemoryType]) -> Self {
        Self {
            mmap: types.contains(&MemoryType::Mmap),
            userptr: types.contains(&MemoryType::UserPtr),
            dmabuf: types.contains(&MemoryType::DmaBuf),
        }
    }
}

/// Video resolution
///
/// Represents a supported video resolution (width × height in pixels).
///
/// # Example
///
/// ```
/// use videostream::v4l2::Resolution;
///
/// let res = Resolution::new(1920, 1080);
/// println!("{}", res);  // "1920x1080"
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Resolution {
    /// Width in pixels
    pub width: u32,
    /// Height in pixels
    pub height: u32,
}

impl Resolution {
    /// Create a new resolution
    ///
    /// # Arguments
    ///
    /// * `width` - Width in pixels
    /// * `height` - Height in pixels
    pub fn new(width: u32, height: u32) -> Self {
        Self { width, height }
    }
}

impl fmt::Display for Resolution {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}x{}", self.width, self.height)
    }
}

/// Video format descriptor
///
/// Describes a pixel format supported by a V4L2 device, including its fourcc code,
/// human-readable description, and optionally the supported resolutions.
///
/// # Common Formats
///
/// | FourCC | Description | Type |
/// |--------|-------------|------|
/// | `NV12` | YUV 4:2:0 semi-planar | Raw |
/// | `YUYV` | YUV 4:2:2 packed | Raw |
/// | `H264` | H.264/AVC | Compressed |
/// | `HEVC` | H.265/HEVC | Compressed |
/// | `MJPG` | Motion JPEG | Compressed |
#[derive(Debug, Clone)]
pub struct Format {
    /// Four-character code (e.g., "NV12", "H264")
    pub fourcc: FourCC,
    /// Human-readable description from driver
    pub description: String,
    /// Whether this is a compressed format
    pub compressed: bool,
    /// Supported resolutions (if enumerated)
    pub resolutions: Vec<Resolution>,
}

impl Format {
    /// Create from FFI VSLFormat
    pub(crate) fn from_ffi(ffi_fmt: &ffi::VSLFormat) -> Self {
        let description = unsafe {
            CStr::from_ptr(ffi_fmt.description.as_ptr())
                .to_string_lossy()
                .into_owned()
        };

        // Parse resolutions if available
        let mut resolutions = Vec::new();
        if !ffi_fmt.resolutions.is_null() && ffi_fmt.num_resolutions > 0 {
            let res_slice =
                unsafe { std::slice::from_raw_parts(ffi_fmt.resolutions, ffi_fmt.num_resolutions) };
            for res in res_slice {
                resolutions.push(Resolution::new(res.width, res.height));
            }
        }

        Format {
            fourcc: FourCC::from_u32(ffi_fmt.fourcc),
            description,
            compressed: ffi_fmt.compressed,
            resolutions,
        }
    }
}

impl fmt::Display for Format {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.fourcc)?;
        if self.compressed {
            write!(f, " (compressed)")?;
        }
        Ok(())
    }
}

/// V4L2 device descriptor
///
/// Contains all information about a discovered V4L2 device including its path,
/// driver information, capabilities, supported formats, and memory types.
///
/// Devices are obtained via [`DeviceEnumerator::enumerate()`] or the `find_*` methods.
///
/// # Example
///
/// ```no_run
/// use videostream::v4l2::{DeviceEnumerator, DeviceType};
///
/// let devices = DeviceEnumerator::enumerate()?;
/// for device in devices {
///     println!("Path: {}", device.path_str());
///     println!("Driver: {}", device.driver());
///     println!("Card: {}", device.card());
///     println!("Type: {:?}", device.device_type());
///     println!("Multiplanar: {}", device.is_multiplanar());
///
///     // Check memory capabilities
///     if device.supports_capture_dmabuf() {
///         println!("  Supports zero-copy capture via DMABUF");
///     }
///
///     // List capture formats
///     for fmt in device.capture_formats() {
///         println!("  Format: {} - {}", fmt.fourcc, fmt.description);
///     }
/// }
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
#[derive(Debug, Clone)]
pub struct Device {
    /// Device path (e.g., "/dev/video0")
    path: PathBuf,
    /// Driver name
    driver: String,
    /// Card/device name
    card: String,
    /// Bus information
    bus_info: String,
    /// Device type classification
    device_type: DeviceType,
    /// Whether device uses multiplanar API
    multiplanar: bool,
    /// Supported capture memory types
    capture_memory: Vec<MemoryType>,
    /// Supported output memory types
    output_memory: Vec<MemoryType>,
    /// Capture formats (populated by enumerate_formats)
    capture_formats: Vec<Format>,
    /// Output formats (populated by enumerate_formats)
    output_formats: Vec<Format>,
}

impl Device {
    /// Create from FFI VSLDevice
    pub(crate) fn from_ffi(ffi_dev: &ffi::VSLDevice) -> Result<Self, Error> {
        let path = unsafe {
            CStr::from_ptr(ffi_dev.path.as_ptr())
                .to_string_lossy()
                .into_owned()
        };
        let driver = unsafe {
            CStr::from_ptr(ffi_dev.driver.as_ptr())
                .to_string_lossy()
                .into_owned()
        };
        let card = unsafe {
            CStr::from_ptr(ffi_dev.card.as_ptr())
                .to_string_lossy()
                .into_owned()
        };
        let bus_info = unsafe {
            CStr::from_ptr(ffi_dev.bus_info.as_ptr())
                .to_string_lossy()
                .into_owned()
        };

        let device_type = DeviceType::from_raw(ffi_dev.device_type).unwrap_or(DeviceType::M2m);

        // Parse capture formats
        let mut capture_formats = Vec::new();
        if !ffi_dev.capture_formats.is_null() && ffi_dev.num_capture_formats > 0 {
            let fmt_slice = unsafe {
                std::slice::from_raw_parts(ffi_dev.capture_formats, ffi_dev.num_capture_formats)
            };
            for fmt in fmt_slice {
                capture_formats.push(Format::from_ffi(fmt));
            }
        }

        // Parse output formats
        let mut output_formats = Vec::new();
        if !ffi_dev.output_formats.is_null() && ffi_dev.num_output_formats > 0 {
            let fmt_slice = unsafe {
                std::slice::from_raw_parts(ffi_dev.output_formats, ffi_dev.num_output_formats)
            };
            for fmt in fmt_slice {
                output_formats.push(Format::from_ffi(fmt));
            }
        }

        Ok(Device {
            path: PathBuf::from(path),
            driver,
            card,
            bus_info,
            device_type,
            multiplanar: ffi_dev.multiplanar,
            capture_memory: MemoryType::from_mask(ffi_dev.capture_mem),
            output_memory: MemoryType::from_mask(ffi_dev.output_mem),
            capture_formats,
            output_formats,
        })
    }

    /// Device path (e.g., "/dev/video0")
    ///
    /// Returns the filesystem path to the device node.
    pub fn path(&self) -> &std::path::Path {
        &self.path
    }

    /// Device path as string
    ///
    /// Convenience method that returns the path as a `&str`.
    /// Returns empty string if the path is not valid UTF-8.
    pub fn path_str(&self) -> &str {
        self.path.to_str().unwrap_or("")
    }

    /// Driver name (e.g., "wave6-enc", "mxc-isi")
    ///
    /// The kernel driver handling this device.
    pub fn driver(&self) -> &str {
        &self.driver
    }

    /// Card/device name (e.g., "wave6-enc", "mxc-isi-cap")
    ///
    /// Human-readable device name from the driver.
    pub fn card(&self) -> &str {
        &self.card
    }

    /// Bus information (e.g., "platform:wave6-enc")
    ///
    /// Identifies the bus the device is connected to. Useful for grouping
    /// related devices (e.g., multiple channels on the same ISI controller).
    pub fn bus_info(&self) -> &str {
        &self.bus_info
    }

    /// Bus information (alias for [`bus_info`](Self::bus_info))
    pub fn bus(&self) -> &str {
        &self.bus_info
    }

    /// Device type classification
    ///
    /// Returns how the device has been classified based on its capabilities.
    pub fn device_type(&self) -> DeviceType {
        self.device_type
    }

    /// Whether device uses multiplanar API
    ///
    /// Multiplanar devices use `V4L2_BUF_TYPE_VIDEO_*_MPLANE` buffer types
    /// and can handle formats with multiple separate memory planes.
    pub fn is_multiplanar(&self) -> bool {
        self.multiplanar
    }

    /// Supported capture memory types (as capability flags)
    ///
    /// Returns a [`MemoryCapabilities`] struct with boolean flags for each type.
    pub fn capture_memory(&self) -> MemoryCapabilities {
        MemoryCapabilities::from_types(&self.capture_memory)
    }

    /// Supported capture memory types (as slice)
    ///
    /// Returns the raw slice of supported memory types.
    pub fn capture_memory_types(&self) -> &[MemoryType] {
        &self.capture_memory
    }

    /// Supported output memory types (as capability flags)
    ///
    /// Returns a [`MemoryCapabilities`] struct with boolean flags for each type.
    pub fn output_memory(&self) -> MemoryCapabilities {
        MemoryCapabilities::from_types(&self.output_memory)
    }

    /// Supported output memory types (as slice)
    ///
    /// Returns the raw slice of supported memory types.
    pub fn output_memory_types(&self) -> &[MemoryType] {
        &self.output_memory
    }

    /// Capture formats (encoder output, decoder output, camera output)
    ///
    /// For cameras, these are the formats the camera can produce.
    /// For encoders, these are the compressed formats the encoder can produce.
    /// For decoders, these are the raw formats the decoder can produce.
    pub fn capture_formats(&self) -> &[Format] {
        &self.capture_formats
    }

    /// Output formats (encoder input, decoder input)
    ///
    /// For encoders, these are the raw formats the encoder can accept.
    /// For decoders, these are the compressed formats the decoder can accept.
    pub fn output_formats(&self) -> &[Format] {
        &self.output_formats
    }

    /// Check if device supports DMABUF on capture side
    ///
    /// Returns true if the device can export captured buffers as dmabuf
    /// file descriptors for zero-copy sharing.
    pub fn supports_capture_dmabuf(&self) -> bool {
        self.capture_memory.contains(&MemoryType::DmaBuf)
    }

    /// Check if device supports DMABUF on output side
    ///
    /// Returns true if the device can import dmabuf file descriptors
    /// for zero-copy input.
    pub fn supports_output_dmabuf(&self) -> bool {
        self.output_memory.contains(&MemoryType::DmaBuf)
    }

    /// Check if device is an encoder
    ///
    /// Equivalent to `device.device_type() == DeviceType::Encoder`.
    pub fn is_encoder(&self) -> bool {
        self.device_type == DeviceType::Encoder
    }

    /// Check if device is a decoder
    ///
    /// Equivalent to `device.device_type() == DeviceType::Decoder`.
    pub fn is_decoder(&self) -> bool {
        self.device_type == DeviceType::Decoder
    }

    /// Check if device is a camera
    ///
    /// Equivalent to `device.device_type() == DeviceType::Camera`.
    pub fn is_camera(&self) -> bool {
        self.device_type == DeviceType::Camera
    }
}

impl fmt::Display for Device {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}: {} ({}) - {}",
            self.path.display(),
            self.card,
            self.driver,
            self.device_type
        )
    }
}
