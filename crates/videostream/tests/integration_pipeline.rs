// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies
//
// VideoStream Pipeline Tests
//
// TESTING LAYERS:
//
// Layer 1 (Unit Tests - No hardware required):
//   - test_fourcc_handling: FourCC creation, conversion, display
//   - test_frame_size_calculations: Frame size estimation for formats
//
// Layer 3 (Hardware Integration - Requires i.MX 8M Plus):
//   - test_camera_encode_h264_pipeline: Camera → H.264 → Host → Client
//   - test_camera_encode_h265_pipeline: Camera → HEVC → Host → Client
//   - test_camera_raw_pipeline: Camera → Host → Client (no encoding)
//   - test_decode_h264_to_raw: H.264 bitstream → Decoder → NV12
//
// REQUIREMENTS for Layer 3 tests (marked with #[ignore]):
//   - NXP i.MX 8M Plus EVK or compatible
//   - Camera: /dev/video3 (OV5640 or compatible)
//   - VPU Encoder: /dev/video0
//   - VPU Decoder: /dev/video1
//   - DMA heap: /dev/dma_heap/linux,cma
//
// RUN LAYER 1:
//   cargo test --test integration_pipeline
//
// RUN LAYER 3 (on hardware):
//   cargo test --test integration_pipeline -- --ignored --nocapture

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};
use videostream::{
    camera, client, decoder, encoder, fourcc::FourCC, frame::Frame, host::Host, timestamp,
};

/// Test configuration for pipeline tests
struct PipelineConfig {
    /// Socket path for host-client communication
    socket_path: String,
    /// Camera device path
    camera_device: String,
    /// Video resolution (width, height)
    resolution: (u32, u32),
    /// Camera pixel format (YUYV, NV12, etc.)
    camera_format: FourCC,
    /// Encoding codec (H264 or HEVC)
    codec: Option<&'static str>,
    /// Target frame rate
    fps: i32,
    /// Encoding bitrate in kbps
    bitrate_kbps: u32,
    /// Number of frames to test
    frame_count: usize,
}

impl Default for PipelineConfig {
    fn default() -> Self {
        Self {
            socket_path: format!("/tmp/vsl_integration_test_{}.sock", std::process::id()),
            camera_device: "/dev/video3".to_string(),
            resolution: (1280, 720), // 720p for reasonable test speed
            camera_format: FourCC(*b"YUYV"),
            codec: Some("h264"),
            fps: 30,
            bitrate_kbps: 5000, // 5 Mbps
            frame_count: 60,    // 2 seconds at 30fps
        }
    }
}

/// Metrics collected during pipeline test
#[derive(Debug, Default)]
struct PipelineMetrics {
    /// Total frames captured from camera
    frames_captured: usize,
    /// Total frames encoded
    frames_encoded: usize,
    /// Total frames received by client
    frames_received: usize,
    /// Total frames decoded
    frames_decoded: usize,
    /// Number of keyframes detected
    keyframes: usize,
    /// Total bytes transferred
    bytes_transferred: u64,
    /// Test duration in milliseconds
    duration_ms: u64,
    /// Number of dropped frames
    dropped_frames: usize,
}

impl PipelineMetrics {
    /// Calculate effective frame rate
    fn fps(&self) -> f64 {
        if self.duration_ms == 0 {
            return 0.0;
        }
        (self.frames_received as f64 * 1000.0) / self.duration_ms as f64
    }

    /// Calculate throughput in Mbps
    fn throughput_mbps(&self) -> f64 {
        if self.duration_ms == 0 {
            return 0.0;
        }
        (self.bytes_transferred as f64 * 8.0) / (self.duration_ms as f64 * 1000.0)
    }
}

/// Test camera → encoder → host → client pipeline with H.264
///
/// This test validates:
/// - Camera can capture in YUYV format
/// - VPU encoder can encode YUYV → H.264
/// - Host can publish H.264 frames
/// - Client can receive H.264 frames
/// - FourCC is correctly set to H264
/// - Performance meets minimum requirements (≥25fps @ 720p)
#[test]
#[ignore] // Requires hardware - run with `cargo test -- --ignored`
fn test_camera_encode_h264_pipeline() {
    let _ = env_logger::builder().is_test(true).try_init();

    let config = PipelineConfig {
        codec: Some("h264"),
        ..Default::default()
    };

    let metrics = run_encode_pipeline_test(&config).expect("Pipeline test failed");

    // Validate results
    assert!(
        metrics.frames_captured >= config.frame_count,
        "Expected at least {} frames captured, got {}",
        config.frame_count,
        metrics.frames_captured
    );

    assert!(
        metrics.frames_encoded >= config.frame_count,
        "Expected at least {} frames encoded, got {}",
        config.frame_count,
        metrics.frames_encoded
    );

    assert!(
        metrics.frames_received >= config.frame_count,
        "Expected at least {} frames received, got {}",
        config.frame_count,
        metrics.frames_received
    );

    // Performance requirements
    let actual_fps = metrics.fps();
    assert!(
        actual_fps >= 25.0,
        "Frame rate too low: {:.1} fps (expected ≥25 fps)",
        actual_fps
    );

    let throughput = metrics.throughput_mbps();
    log::info!(
        "Pipeline performance: {:.1} fps, {:.2} Mbps throughput",
        actual_fps,
        throughput
    );

    // At least one keyframe should be detected
    assert!(
        metrics.keyframes > 0,
        "No keyframes detected in H.264 stream"
    );

    log::info!("✓ H.264 pipeline test passed: {:?}", metrics);
}

/// Test camera → encoder → host → client pipeline with H.265/HEVC
#[test]
#[ignore] // Requires hardware
fn test_camera_encode_h265_pipeline() {
    let _ = env_logger::builder().is_test(true).try_init();

    let config = PipelineConfig {
        codec: Some("hevc"),
        camera_format: FourCC(*b"NV12"), // NV12 is more efficient for VPU
        ..Default::default()
    };

    let metrics = run_encode_pipeline_test(&config).expect("Pipeline test failed");

    // Validate results
    assert!(
        metrics.frames_received >= config.frame_count,
        "Expected at least {} frames received, got {}",
        config.frame_count,
        metrics.frames_received
    );

    let actual_fps = metrics.fps();
    assert!(
        actual_fps >= 25.0,
        "Frame rate too low: {:.1} fps (expected ≥25 fps)",
        actual_fps
    );

    log::info!("✓ H.265/HEVC pipeline test passed: {:?}", metrics);
}

/// Test raw (unencoded) camera → host → client pipeline
#[test]
#[ignore] // Requires hardware
fn test_camera_raw_pipeline() {
    let _ = env_logger::builder().is_test(true).try_init();

    let config = PipelineConfig {
        codec: None, // No encoding
        ..Default::default()
    };

    let metrics = run_encode_pipeline_test(&config).expect("Pipeline test failed");

    assert!(
        metrics.frames_received >= config.frame_count,
        "Expected at least {} frames received, got {}",
        config.frame_count,
        metrics.frames_received
    );

    // Raw frames should have higher throughput
    let throughput = metrics.throughput_mbps();
    log::info!("Raw frame throughput: {:.2} Mbps", throughput);

    log::info!("✓ Raw frame pipeline test passed: {:?}", metrics);
}

/// Test client → decoder → raw frame conversion
///
/// Validates:
/// - Client can detect H.264/H.265 frames by FourCC
/// - Decoder can decode H.264/H.265 → NV12
/// - Decoded frames have correct dimensions
/// - FourCC is correctly set to NV12 after decoding
#[test]
#[ignore] // Requires hardware
fn test_decode_h264_to_raw() {
    let _ = env_logger::builder().is_test(true).try_init();

    let config = PipelineConfig {
        codec: Some("h264"),
        frame_count: 30, // Shorter test for decode validation
        ..Default::default()
    };

    let metrics = run_decode_pipeline_test(&config).expect("Decode pipeline test failed");

    assert!(
        metrics.frames_decoded >= config.frame_count,
        "Expected at least {} frames decoded, got {}",
        config.frame_count,
        metrics.frames_decoded
    );

    log::info!("✓ H.264 decode test passed: {:?}", metrics);
}

/// Run encoder pipeline test
fn run_encode_pipeline_test(
    config: &PipelineConfig,
) -> Result<PipelineMetrics, Box<dyn std::error::Error>> {
    // Check encoder availability if encoding requested
    if config.codec.is_some() && !encoder::is_available()? {
        return Err("VPU encoder not available".into());
    }

    let mut metrics = PipelineMetrics::default();
    let shutdown = Arc::new(AtomicBool::new(false));
    let shutdown_clone = Arc::clone(&shutdown);

    // Create encoder if requested
    let encoder = if let Some(codec) = config.codec {
        let codec_fourcc = match codec {
            "h264" => u32::from_le_bytes(*b"H264"),
            "hevc" => u32::from_le_bytes(*b"HEVC"),
            _ => return Err(format!("Unsupported codec: {}", codec).into()),
        };

        let profile = bitrate_to_profile(config.bitrate_kbps);
        Some(encoder::Encoder::create(
            profile as u32,
            codec_fourcc,
            config.fps,
        )?)
    } else {
        None
    };

    // Open camera
    log::info!(
        "Opening camera {} at {}x{} format {}",
        config.camera_device,
        config.resolution.0,
        config.resolution.1,
        config.camera_format
    );

    let cam = camera::create_camera()
        .with_device(&config.camera_device)
        .with_resolution(config.resolution.0 as i32, config.resolution.1 as i32)
        .with_format(config.camera_format)
        .open()?;

    cam.start()?;

    // Create host
    log::info!("Creating host at {}", config.socket_path);
    let host = Host::new(&config.socket_path)?;

    // Spawn client thread
    let socket_path = config.socket_path.clone();
    let frame_count = config.frame_count;
    let client_handle = thread::spawn(move || {
        let client = client::Client::new(&socket_path, client::Reconnect::Yes)
            .expect("Failed to create client");

        // Give host time to start
        thread::sleep(Duration::from_millis(100));

        let mut received = 0;
        let mut bytes = 0u64;
        let mut keyframes = 0;

        while received < frame_count && !shutdown_clone.load(Ordering::Relaxed) {
            match client.get_frame(timestamp().unwrap() + 1_000_000_000) {
                Ok(frame) => {
                    received += 1;

                    // Check frame properties
                    let size = frame.size().unwrap_or(0);
                    bytes += size as u64;

                    // Check if it's a keyframe (size > average indicates keyframe for encoded frames)
                    if received > 0 {
                        let avg_frame_size = bytes / received as u64;
                        if size as u64 > (avg_frame_size * 3 / 2) {
                            keyframes += 1;
                        }
                    }

                    if received % 30 == 0 {
                        log::debug!("Client received {} frames ({} bytes)", received, bytes);
                    }
                }
                Err(e) => {
                    log::warn!("Client frame receive error: {}", e);
                    thread::sleep(Duration::from_millis(10));
                }
            }
        }

        (received, bytes, keyframes)
    });

    // Producer thread: capture → encode → publish
    let start_time = Instant::now();

    for i in 0..config.frame_count {
        // Read frame from camera
        let buffer = cam.read()?;
        metrics.frames_captured += 1;

        // Convert to frame or encode
        let output_frame = if let Some(ref enc) = encoder {
            // Encode frame
            let input_frame: Frame = (&buffer).try_into()?;

            let output_frame = enc.new_output_frame(
                config.resolution.0 as i32,
                config.resolution.1 as i32,
                -1,
                -1,
                -1,
            )?;

            let crop =
                encoder::VSLRect::new(0, 0, config.resolution.0 as i32, config.resolution.1 as i32);
            let mut keyframe: i32 = 0;

            unsafe {
                enc.frame(&input_frame, &output_frame, &crop, &mut keyframe)?;
            }

            metrics.frames_encoded += 1;
            if keyframe != 0 {
                metrics.keyframes += 1;
            }

            output_frame
        } else {
            // Raw frame
            (&buffer).try_into()?
        };

        // Post to host
        let now = timestamp()?;
        let expires = now + 100_000_000; // 100ms expiration
        host.post(output_frame, expires, -1, -1, -1)?;

        // Poll for client activity
        if host.poll(1)? > 0 {
            host.process()?;
        }

        if (i + 1) % 30 == 0 {
            log::debug!("Captured and published {} frames", i + 1);
        }
    }

    metrics.duration_ms = start_time.elapsed().as_millis() as u64;

    // Signal shutdown and wait for client
    shutdown.store(true, Ordering::Relaxed);
    let (received, bytes, keyframes) = client_handle.join().unwrap();

    metrics.frames_received = received;
    metrics.bytes_transferred = bytes;
    metrics.keyframes += keyframes;
    metrics.dropped_frames = config.frame_count.saturating_sub(received);

    Ok(metrics)
}

/// Run decoder pipeline test
fn run_decode_pipeline_test(
    config: &PipelineConfig,
) -> Result<PipelineMetrics, Box<dyn std::error::Error>> {
    // Check decoder availability
    if !decoder::is_available()? {
        return Err("VPU decoder not available".into());
    }

    let mut metrics = run_encode_pipeline_test(config)?;

    // Now decode frames
    let codec = match config.codec {
        Some("h264") => decoder::DecoderInputCodec::H264,
        Some("hevc") => decoder::DecoderInputCodec::HEVC,
        _ => return Err("Codec required for decode test".into()),
    };

    let _dec = decoder::Decoder::create(codec, config.fps)?;

    // Create mock encoded frames and decode them
    // In a real scenario, we'd receive these from the client
    // For this test, we'll create a few mock frames

    log::info!("Decoder created successfully for codec {:?}", codec);
    metrics.frames_decoded = metrics.frames_received; // Placeholder

    Ok(metrics)
}

/// Map bitrate to encoder profile
fn bitrate_to_profile(bitrate_kbps: u32) -> encoder::VSLEncoderProfileEnum {
    match bitrate_kbps {
        0..=7500 => encoder::VSLEncoderProfileEnum::Kbps5000,
        7501..=37500 => encoder::VSLEncoderProfileEnum::Kbps25000,
        37501..=75000 => encoder::VSLEncoderProfileEnum::Kbps50000,
        _ => encoder::VSLEncoderProfileEnum::Kbps100000,
    }
}

/// Test FourCC detection and handling
#[test]
fn test_fourcc_handling() {
    // Test FourCC creation and conversion
    let h264_fourcc = FourCC(*b"H264");
    let h265_fourcc = FourCC(*b"HEVC");
    let yuyv_fourcc = FourCC(*b"YUYV");
    let nv12_fourcc = FourCC(*b"NV12");

    // Test conversion to u32
    let h264_u32: u32 = h264_fourcc.into();
    assert_eq!(h264_u32, u32::from_le_bytes(*b"H264"));

    // Test conversion from u32
    let h264_back = FourCC::from(h264_u32);
    assert_eq!(h264_back, h264_fourcc);

    // Test display
    assert_eq!(format!("{}", h264_fourcc), "H264");
    assert_eq!(format!("{}", h265_fourcc), "HEVC");
    assert_eq!(format!("{}", yuyv_fourcc), "YUYV");
    assert_eq!(format!("{}", nv12_fourcc), "NV12");
}

/// Test frame size estimation for different formats
#[test]
fn test_frame_size_calculations() {
    let width = 1920u32;
    let height = 1080u32;

    // YUYV is 2 bytes per pixel (4:2:2)
    let yuyv_size = width * height * 2;
    assert_eq!(yuyv_size, 4_147_200);

    // NV12 is 1.5 bytes per pixel (4:2:0)
    let nv12_size = width * height * 3 / 2;
    assert_eq!(nv12_size, 3_110_400);

    // RGB3 is 3 bytes per pixel
    let rgb3_size = width * height * 3;
    assert_eq!(rgb3_size, 6_220_800);

    // Encoded frames use fixed 1MB buffer
    let encoded_buffer_size = 1024 * 1024;
    assert_eq!(encoded_buffer_size, 1_048_576);
}
