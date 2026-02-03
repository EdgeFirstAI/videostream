// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies
//
// V4L2 Device Discovery Tests
//
// TESTING LAYERS:
//
// Layer 1 (Unit Tests - No hardware required):
//   - Rust type conversions (DeviceType, MemoryType, etc.)
//   - Display trait implementations
//   - FourCC utility functions via FFI
//   - Graceful handling of missing devices
//
// Layer 3 (Hardware Integration - Requires V4L2 devices):
//   - Device enumeration
//   - Device property validation
//   - Auto-detection functions (find_encoder, find_decoder, find_camera)
//
// RUN LAYER 1:
//   cargo test --test v4l2_discovery
//
// RUN LAYER 3 (on hardware):
//   cargo test --test v4l2_discovery -- --ignored --nocapture

use videostream::fourcc::FourCC;
use videostream::v4l2::{DeviceEnumerator, DeviceType, MemoryCapabilities, MemoryType, Resolution};
use videostream_sys as ffi;

// =============================================================================
// Layer 1: Unit Tests (No Hardware Required)
// =============================================================================

// -----------------------------------------------------------------------------
// DeviceType Tests
// -----------------------------------------------------------------------------

#[test]
fn test_device_type_from_raw_camera() {
    let dt = DeviceType::from_raw(ffi::VSLDeviceType_VSL_V4L2_TYPE_CAMERA);
    assert_eq!(dt, Some(DeviceType::Camera));
}

#[test]
fn test_device_type_from_raw_encoder() {
    let dt = DeviceType::from_raw(ffi::VSLDeviceType_VSL_V4L2_TYPE_ENCODER);
    assert_eq!(dt, Some(DeviceType::Encoder));
}

#[test]
fn test_device_type_from_raw_decoder() {
    let dt = DeviceType::from_raw(ffi::VSLDeviceType_VSL_V4L2_TYPE_DECODER);
    assert_eq!(dt, Some(DeviceType::Decoder));
}

#[test]
fn test_device_type_from_raw_output() {
    let dt = DeviceType::from_raw(ffi::VSLDeviceType_VSL_V4L2_TYPE_OUTPUT);
    assert_eq!(dt, Some(DeviceType::Output));
}

#[test]
fn test_device_type_from_raw_isp() {
    let dt = DeviceType::from_raw(ffi::VSLDeviceType_VSL_V4L2_TYPE_ISP);
    assert_eq!(dt, Some(DeviceType::Isp));
}

#[test]
fn test_device_type_from_raw_m2m() {
    let dt = DeviceType::from_raw(ffi::VSLDeviceType_VSL_V4L2_TYPE_M2M);
    assert_eq!(dt, Some(DeviceType::M2m));
}

#[test]
fn test_device_type_from_raw_unknown() {
    // Values outside the enum range should map to Unknown
    let dt = DeviceType::from_raw(9999);
    assert_eq!(dt, Some(DeviceType::Unknown));
}

#[test]
fn test_device_type_name() {
    assert_eq!(DeviceType::Camera.name(), "Camera");
    assert_eq!(DeviceType::Encoder.name(), "Encoder");
    assert_eq!(DeviceType::Decoder.name(), "Decoder");
    assert_eq!(DeviceType::Output.name(), "Output");
    assert_eq!(DeviceType::Isp.name(), "ISP");
    assert_eq!(DeviceType::M2m.name(), "M2M");
    assert_eq!(DeviceType::Unknown.name(), "Unknown");
}

#[test]
fn test_device_type_display() {
    assert_eq!(format!("{}", DeviceType::Camera), "Camera");
    assert_eq!(format!("{}", DeviceType::Encoder), "Encoder");
    assert_eq!(format!("{}", DeviceType::Decoder), "Decoder");
}

// -----------------------------------------------------------------------------
// MemoryType Tests
// -----------------------------------------------------------------------------

#[test]
fn test_memory_type_from_mask_empty() {
    let types = MemoryType::from_mask(0);
    assert!(types.is_empty());
}

#[test]
fn test_memory_type_from_mask_single_mmap() {
    let types = MemoryType::from_mask(ffi::VSLMemoryType_VSL_V4L2_MEM_MMAP);
    assert_eq!(types.len(), 1);
    assert!(types.contains(&MemoryType::Mmap));
}

#[test]
fn test_memory_type_from_mask_single_userptr() {
    let types = MemoryType::from_mask(ffi::VSLMemoryType_VSL_V4L2_MEM_USERPTR);
    assert_eq!(types.len(), 1);
    assert!(types.contains(&MemoryType::UserPtr));
}

#[test]
fn test_memory_type_from_mask_single_dmabuf() {
    let types = MemoryType::from_mask(ffi::VSLMemoryType_VSL_V4L2_MEM_DMABUF);
    assert_eq!(types.len(), 1);
    assert!(types.contains(&MemoryType::DmaBuf));
}

#[test]
fn test_memory_type_from_mask_multiple() {
    // Combine MMAP and DMABUF
    let mask = ffi::VSLMemoryType_VSL_V4L2_MEM_MMAP | ffi::VSLMemoryType_VSL_V4L2_MEM_DMABUF;
    let types = MemoryType::from_mask(mask);
    assert_eq!(types.len(), 2);
    assert!(types.contains(&MemoryType::Mmap));
    assert!(types.contains(&MemoryType::DmaBuf));
    assert!(!types.contains(&MemoryType::UserPtr));
}

#[test]
fn test_memory_type_from_mask_all() {
    let mask = ffi::VSLMemoryType_VSL_V4L2_MEM_MMAP
        | ffi::VSLMemoryType_VSL_V4L2_MEM_USERPTR
        | ffi::VSLMemoryType_VSL_V4L2_MEM_DMABUF;
    let types = MemoryType::from_mask(mask);
    assert_eq!(types.len(), 3);
    assert!(types.contains(&MemoryType::Mmap));
    assert!(types.contains(&MemoryType::UserPtr));
    assert!(types.contains(&MemoryType::DmaBuf));
}

#[test]
fn test_memory_type_display() {
    assert_eq!(format!("{}", MemoryType::Mmap), "MMAP");
    assert_eq!(format!("{}", MemoryType::UserPtr), "USERPTR");
    assert_eq!(format!("{}", MemoryType::DmaBuf), "DMABUF");
}

// -----------------------------------------------------------------------------
// MemoryCapabilities Tests
// -----------------------------------------------------------------------------

#[test]
fn test_memory_capabilities_from_types_empty() {
    let caps = MemoryCapabilities::from_types(&[]);
    assert!(!caps.mmap);
    assert!(!caps.userptr);
    assert!(!caps.dmabuf);
}

#[test]
fn test_memory_capabilities_from_types_mmap() {
    let caps = MemoryCapabilities::from_types(&[MemoryType::Mmap]);
    assert!(caps.mmap);
    assert!(!caps.userptr);
    assert!(!caps.dmabuf);
}

#[test]
fn test_memory_capabilities_from_types_dmabuf() {
    let caps = MemoryCapabilities::from_types(&[MemoryType::DmaBuf]);
    assert!(!caps.mmap);
    assert!(!caps.userptr);
    assert!(caps.dmabuf);
}

#[test]
fn test_memory_capabilities_from_types_all() {
    let caps = MemoryCapabilities::from_types(&[
        MemoryType::Mmap,
        MemoryType::UserPtr,
        MemoryType::DmaBuf,
    ]);
    assert!(caps.mmap);
    assert!(caps.userptr);
    assert!(caps.dmabuf);
}

#[test]
fn test_memory_capabilities_default() {
    let caps = MemoryCapabilities::default();
    assert!(!caps.mmap);
    assert!(!caps.userptr);
    assert!(!caps.dmabuf);
}

// -----------------------------------------------------------------------------
// Resolution Tests
// -----------------------------------------------------------------------------

#[test]
fn test_resolution_new() {
    let res = Resolution::new(1920, 1080);
    assert_eq!(res.width, 1920);
    assert_eq!(res.height, 1080);
}

#[test]
fn test_resolution_display() {
    let res = Resolution::new(1920, 1080);
    assert_eq!(format!("{}", res), "1920x1080");
}

#[test]
fn test_resolution_display_4k() {
    let res = Resolution::new(3840, 2160);
    assert_eq!(format!("{}", res), "3840x2160");
}

#[test]
fn test_resolution_display_vga() {
    let res = Resolution::new(640, 480);
    assert_eq!(format!("{}", res), "640x480");
}

#[test]
fn test_resolution_equality() {
    let a = Resolution::new(1920, 1080);
    let b = Resolution::new(1920, 1080);
    let c = Resolution::new(1280, 720);
    assert_eq!(a, b);
    assert_ne!(a, c);
}

// -----------------------------------------------------------------------------
// FourCC Utility Tests (via FFI)
// These tests require the C library to be loaded, so they're marked as hardware
// tests. On CI without the library, they're skipped gracefully.
// -----------------------------------------------------------------------------

#[test]
fn test_is_compressed_format_h264() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(is_compressed) = lib.vsl_v4l2_is_compressed_format.as_ref() {
        let fourcc = FourCC(*b"H264").as_u32();
        let result = unsafe { is_compressed(fourcc) };
        assert!(result, "H264 should be compressed");
    }
}

#[test]
fn test_is_compressed_format_hevc() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(is_compressed) = lib.vsl_v4l2_is_compressed_format.as_ref() {
        let fourcc = FourCC(*b"HEVC").as_u32();
        let result = unsafe { is_compressed(fourcc) };
        assert!(result, "HEVC should be compressed");
    }
}

#[test]
fn test_is_compressed_format_mjpg() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(is_compressed) = lib.vsl_v4l2_is_compressed_format.as_ref() {
        let fourcc = FourCC(*b"MJPG").as_u32();
        let result = unsafe { is_compressed(fourcc) };
        assert!(result, "MJPG should be compressed");
    }
}

#[test]
fn test_is_compressed_format_nv12() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(is_compressed) = lib.vsl_v4l2_is_compressed_format.as_ref() {
        let fourcc = FourCC(*b"NV12").as_u32();
        let result = unsafe { is_compressed(fourcc) };
        assert!(!result, "NV12 should NOT be compressed (raw format)");
    }
}

#[test]
fn test_is_compressed_format_yuyv() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(is_compressed) = lib.vsl_v4l2_is_compressed_format.as_ref() {
        let fourcc = FourCC(*b"YUYV").as_u32();
        let result = unsafe { is_compressed(fourcc) };
        assert!(!result, "YUYV should NOT be compressed (raw format)");
    }
}

#[test]
fn test_fourcc_to_string_nv12() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(fourcc_to_string) = lib.vsl_v4l2_fourcc_to_string.as_ref() {
        let fourcc = FourCC(*b"NV12").as_u32();
        let mut buf = [0u8; 5];
        let result = unsafe { fourcc_to_string(fourcc, buf.as_mut_ptr() as *mut i8) };
        assert!(!result.is_null());
        let s = unsafe { std::ffi::CStr::from_ptr(result) }
            .to_string_lossy()
            .into_owned();
        assert_eq!(s, "NV12");
    }
}

#[test]
fn test_fourcc_to_string_h264() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(fourcc_to_string) = lib.vsl_v4l2_fourcc_to_string.as_ref() {
        let fourcc = FourCC(*b"H264").as_u32();
        let mut buf = [0u8; 5];
        let result = unsafe { fourcc_to_string(fourcc, buf.as_mut_ptr() as *mut i8) };
        assert!(!result.is_null());
        let s = unsafe { std::ffi::CStr::from_ptr(result) }
            .to_string_lossy()
            .into_owned();
        assert_eq!(s, "H264");
    }
}

#[test]
fn test_fourcc_to_string_roundtrip() {
    let Ok(lib) = ffi::init() else {
        eprintln!("Skipping: Library not available in this environment");
        return;
    };
    if let Ok(fourcc_to_string) = lib.vsl_v4l2_fourcc_to_string.as_ref() {
        // Test various formats
        for format in &[b"NV12", b"YUYV", b"H264", b"HEVC", b"MJPG", b"RGB3"] {
            let fourcc = FourCC(**format).as_u32();
            let mut buf = [0u8; 5];
            let result = unsafe { fourcc_to_string(fourcc, buf.as_mut_ptr() as *mut i8) };
            assert!(!result.is_null());
            let s = unsafe { std::ffi::CStr::from_ptr(result) }
                .to_string_lossy()
                .into_owned();
            let expected = std::str::from_utf8(*format).unwrap();
            assert_eq!(s, expected, "FourCC roundtrip failed for {}", expected);
        }
    }
}

// =============================================================================
// Layer 3: Hardware Integration Tests (Requires V4L2 Devices)
// =============================================================================

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_enumerate_finds_devices() {
    let _ = env_logger::builder().is_test(true).try_init();

    let devices = DeviceEnumerator::enumerate().expect("enumerate should succeed");
    println!("Found {} V4L2 devices", devices.len());

    // Most Linux systems with V4L2 support should have at least one device
    // On embedded systems, we expect multiple devices
    assert!(
        !devices.is_empty(),
        "At least one V4L2 device should be present"
    );

    for device in &devices {
        println!(
            "  {}: {} ({}) - {:?}",
            device.path_str(),
            device.card(),
            device.driver(),
            device.device_type()
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_enumerate_type_cameras() {
    let _ = env_logger::builder().is_test(true).try_init();

    let cameras = DeviceEnumerator::enumerate_type(DeviceType::Camera)
        .expect("enumerate cameras should succeed");

    println!("Found {} cameras", cameras.len());
    for cam in &cameras {
        println!("  Camera: {} at {}", cam.card(), cam.path_str());
        assert!(cam.is_camera());
        assert_eq!(cam.device_type(), DeviceType::Camera);
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_enumerate_type_encoders() {
    let _ = env_logger::builder().is_test(true).try_init();

    let encoders = DeviceEnumerator::enumerate_type(DeviceType::Encoder)
        .expect("enumerate encoders should succeed");

    println!("Found {} encoders", encoders.len());
    for enc in &encoders {
        println!("  Encoder: {} at {}", enc.card(), enc.path_str());
        assert!(enc.is_encoder());
        assert_eq!(enc.device_type(), DeviceType::Encoder);

        // Encoders should have compressed capture formats
        let has_compressed = enc.capture_formats().iter().any(|f| f.compressed);
        assert!(
            has_compressed || enc.capture_formats().is_empty(),
            "Encoder {} should have compressed capture formats",
            enc.card()
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_enumerate_type_decoders() {
    let _ = env_logger::builder().is_test(true).try_init();

    let decoders = DeviceEnumerator::enumerate_type(DeviceType::Decoder)
        .expect("enumerate decoders should succeed");

    println!("Found {} decoders", decoders.len());
    for dec in &decoders {
        println!("  Decoder: {} at {}", dec.card(), dec.path_str());
        assert!(dec.is_decoder());
        assert_eq!(dec.device_type(), DeviceType::Decoder);

        // Decoders should have compressed output formats
        let has_compressed = dec.output_formats().iter().any(|f| f.compressed);
        assert!(
            has_compressed || dec.output_formats().is_empty(),
            "Decoder {} should have compressed output formats",
            dec.card()
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_device_has_valid_path() {
    let devices = DeviceEnumerator::enumerate().expect("enumerate should succeed");

    for device in &devices {
        let path = device.path_str();
        assert!(
            path.starts_with("/dev/video"),
            "Path should start with /dev/video: {}",
            path
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_device_has_driver_name() {
    let devices = DeviceEnumerator::enumerate().expect("enumerate should succeed");

    for device in &devices {
        let driver = device.driver();
        assert!(
            !driver.is_empty(),
            "Device {} should have a driver name",
            device.path_str()
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_device_has_card_name() {
    let devices = DeviceEnumerator::enumerate().expect("enumerate should succeed");

    for device in &devices {
        let card = device.card();
        assert!(
            !card.is_empty(),
            "Device {} should have a card name",
            device.path_str()
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_device_has_bus_info() {
    let devices = DeviceEnumerator::enumerate().expect("enumerate should succeed");

    for device in &devices {
        let bus = device.bus_info();
        assert!(
            !bus.is_empty(),
            "Device {} should have bus info",
            device.path_str()
        );
    }
}

#[test]
#[ignore = "requires i.MX VPU (run with --ignored on i.MX hardware)"]
fn test_find_h264_encoder() {
    let _ = env_logger::builder().is_test(true).try_init();

    let result = DeviceEnumerator::find_encoder(b"H264").expect("find_encoder should succeed");

    match result {
        Some(path) => {
            println!("Found H.264 encoder: {}", path);
            assert!(
                path.starts_with("/dev/video"),
                "Encoder path should be /dev/videoX"
            );
        }
        None => {
            println!("No H.264 encoder found (this may be expected on some platforms)");
        }
    }
}

#[test]
#[ignore = "requires i.MX VPU (run with --ignored on i.MX hardware)"]
fn test_find_hevc_encoder() {
    let _ = env_logger::builder().is_test(true).try_init();

    let result = DeviceEnumerator::find_encoder(b"HEVC").expect("find_encoder should succeed");

    match result {
        Some(path) => {
            println!("Found HEVC encoder: {}", path);
            assert!(
                path.starts_with("/dev/video"),
                "Encoder path should be /dev/videoX"
            );
        }
        None => {
            println!("No HEVC encoder found (this may be expected on some platforms)");
        }
    }
}

#[test]
#[ignore = "requires i.MX VPU (run with --ignored on i.MX hardware)"]
fn test_find_h264_decoder() {
    let _ = env_logger::builder().is_test(true).try_init();

    let result = DeviceEnumerator::find_decoder(b"H264").expect("find_decoder should succeed");

    match result {
        Some(path) => {
            println!("Found H.264 decoder: {}", path);
            assert!(
                path.starts_with("/dev/video"),
                "Decoder path should be /dev/videoX"
            );
        }
        None => {
            println!("No H.264 decoder found");
        }
    }
}

#[test]
#[ignore = "requires i.MX VPU (run with --ignored on i.MX hardware)"]
fn test_find_hevc_decoder() {
    let _ = env_logger::builder().is_test(true).try_init();

    let result = DeviceEnumerator::find_decoder(b"HEVC").expect("find_decoder should succeed");

    match result {
        Some(path) => {
            println!("Found HEVC decoder: {}", path);
            assert!(
                path.starts_with("/dev/video"),
                "Decoder path should be /dev/videoX"
            );
        }
        None => {
            println!("No HEVC decoder found");
        }
    }
}

#[test]
#[ignore = "requires camera (run with --ignored on hardware)"]
fn test_find_camera_nv12() {
    let _ = env_logger::builder().is_test(true).try_init();

    let result = DeviceEnumerator::find_camera(b"NV12").expect("find_camera should succeed");

    match result {
        Some(path) => {
            println!("Found NV12 camera: {}", path);
            assert!(
                path.starts_with("/dev/video"),
                "Camera path should be /dev/videoX"
            );
        }
        None => {
            println!("No NV12 camera found");
        }
    }
}

#[test]
#[ignore = "requires camera (run with --ignored on hardware)"]
fn test_find_camera_with_resolution_1080p() {
    let _ = env_logger::builder().is_test(true).try_init();

    let result = DeviceEnumerator::find_camera_with_resolution(b"NV12", 1920, 1080)
        .expect("find_camera_with_resolution should succeed");

    match result {
        Some(path) => {
            println!("Found 1080p NV12 camera: {}", path);
            assert!(
                path.starts_with("/dev/video"),
                "Camera path should be /dev/videoX"
            );
        }
        None => {
            println!("No 1080p NV12 camera found");
        }
    }
}

#[test]
#[ignore = "requires encoder with DMABUF support (run with --ignored on hardware)"]
fn test_encoder_supports_dmabuf() {
    let _ = env_logger::builder().is_test(true).try_init();

    let encoders = DeviceEnumerator::enumerate_type(DeviceType::Encoder)
        .expect("enumerate encoders should succeed");

    let mut found_dmabuf = false;
    for enc in &encoders {
        if enc.supports_output_dmabuf() {
            println!(
                "Encoder {} supports DMABUF input (zero-copy)",
                enc.path_str()
            );
            found_dmabuf = true;
        }
        if enc.supports_capture_dmabuf() {
            println!("Encoder {} supports DMABUF output", enc.path_str());
        }
    }

    if !found_dmabuf && !encoders.is_empty() {
        println!("No encoder with DMABUF support found (this is expected on some platforms)");
    }
}

#[test]
#[ignore = "requires camera (run with --ignored on hardware)"]
fn test_camera_memory_types() {
    let _ = env_logger::builder().is_test(true).try_init();

    let cameras = DeviceEnumerator::enumerate_type(DeviceType::Camera)
        .expect("enumerate cameras should succeed");

    for cam in &cameras {
        let caps = cam.capture_memory();
        println!(
            "Camera {}: MMAP={}, USERPTR={}, DMABUF={}",
            cam.path_str(),
            caps.mmap,
            caps.userptr,
            caps.dmabuf
        );

        // Most cameras support at least MMAP
        assert!(
            caps.mmap || caps.userptr || caps.dmabuf,
            "Camera {} should support at least one memory type",
            cam.path_str()
        );
    }
}

#[test]
#[ignore = "requires camera (run with --ignored on hardware)"]
fn test_camera_has_capture_formats() {
    let _ = env_logger::builder().is_test(true).try_init();

    let cameras = DeviceEnumerator::enumerate_type(DeviceType::Camera)
        .expect("enumerate cameras should succeed");

    for cam in &cameras {
        let formats = cam.capture_formats();
        println!("Camera {} formats:", cam.path_str());
        for fmt in formats {
            println!("  {} - {}", fmt.fourcc, fmt.description);
        }

        // Cameras should have at least one capture format
        assert!(
            !formats.is_empty(),
            "Camera {} should have capture formats",
            cam.path_str()
        );
    }
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_device_display_trait() {
    let devices = DeviceEnumerator::enumerate().expect("enumerate should succeed");

    for device in &devices {
        let display = format!("{}", device);
        // Display should include path, card, driver, and type
        assert!(
            display.contains(device.path_str()),
            "Display should contain path"
        );
        assert!(
            display.contains(device.card()),
            "Display should contain card name"
        );
    }
}
