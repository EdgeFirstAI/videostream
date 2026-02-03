// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! V4L2 Device Discovery and Enumeration API
//!
//! This module provides portable V4L2 device discovery, capability detection,
//! and format enumeration for Linux video devices. It enables automatic detection
//! of cameras, encoders, and decoders without hardcoded device paths, making video
//! pipelines portable across different hardware platforms.
//!
//! # Features
//!
//! - **Device Enumeration**: Scan all `/dev/video*` devices and query capabilities
//! - **Type Classification**: Automatically classify devices as cameras, encoders,
//!   decoders, or ISPs based on V4L2 capabilities
//! - **Format Discovery**: Query supported pixel formats via `VIDIOC_ENUM_FMT`
//! - **Resolution Discovery**: Query supported resolutions via `VIDIOC_ENUM_FRAMESIZES`
//! - **Memory Detection**: Detect MMAP, USERPTR, and DMABUF support
//! - **Auto-Detection**: Find devices by codec or format (e.g., "find H.264 encoder")
//!
//! # Quick Start
//!
//! ```no_run
//! use videostream::v4l2::{DeviceEnumerator, DeviceType};
//!
//! // List all V4L2 devices
//! let devices = DeviceEnumerator::enumerate()?;
//! for device in &devices {
//!     println!("{}: {} ({:?})", device.path_str(), device.card(), device.device_type());
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! # Device Types
//!
//! V4L2 devices are classified by their capabilities:
//!
//! | Type | Description | Example |
//! |------|-------------|---------|
//! | [`DeviceType::Camera`] | Video capture devices | USB webcam, ISI camera |
//! | [`DeviceType::Encoder`] | Hardware video encoders | Wave6 VPU, JPEG encoder |
//! | [`DeviceType::Decoder`] | Hardware video decoders | Wave6 VPU, JPEG decoder |
//! | [`DeviceType::Isp`] | Image signal processors | NXP ISI, neoisp |
//! | [`DeviceType::M2m`] | Generic memory-to-memory | Scalers, converters |
//!
//! The classification is determined by examining V4L2 capabilities and the types
//! of pixel formats supported on input/output queues.
//!
//! # Memory Types
//!
//! V4L2 supports multiple buffer memory modes:
//!
//! | Mode | Description | Zero-Copy |
//! |------|-------------|-----------|
//! | [`MemoryType::Mmap`] | Kernel allocates, user mmaps | No |
//! | [`MemoryType::UserPtr`] | User allocates, passes pointer | Possible* |
//! | [`MemoryType::DmaBuf`] | User passes dmabuf fd | Yes |
//!
//! *USERPTR can be zero-copy if backed by DMA heap memory. See the C API
//! `vsl_v4l2_alloc_userptr()` for DMA heap-backed USERPTR allocation.
//!
//! # Auto-Detection Examples
//!
//! Find an encoder by output codec:
//!
//! ```no_run
//! use videostream::v4l2::DeviceEnumerator;
//!
//! // Find H.264 encoder
//! if let Some(path) = DeviceEnumerator::find_encoder(b"H264")? {
//!     println!("H.264 encoder: {}", path);
//! }
//!
//! // Find HEVC encoder
//! if let Some(path) = DeviceEnumerator::find_encoder(b"HEVC")? {
//!     println!("HEVC encoder: {}", path);
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! Find a camera by pixel format:
//!
//! ```no_run
//! use videostream::v4l2::DeviceEnumerator;
//!
//! // Find camera supporting NV12
//! if let Some(path) = DeviceEnumerator::find_camera(b"NV12")? {
//!     println!("NV12 camera: {}", path);
//! }
//!
//! // Find camera supporting NV12 at 1080p or higher
//! if let Some(path) = DeviceEnumerator::find_camera_with_resolution(b"NV12", 1920, 1080)? {
//!     println!("1080p NV12 camera: {}", path);
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! # Platform Support
//!
//! Tested on:
//! - NXP i.MX8M Plus (Hantro VPU)
//! - NXP i.MX95 (Wave6 VPU)
//! - Generic x86_64 (development/testing)
//!
//! # See Also
//!
//! - [`DeviceEnumerator`] - Main entry point for device discovery
//! - [`Device`] - Device descriptor with capabilities and formats
//! - [`Format`] - Pixel format with resolutions

mod device;
mod enumerator;

pub use device::{Device, DeviceType, Format, MemoryCapabilities, MemoryType, Resolution};
pub use enumerator::DeviceEnumerator;
