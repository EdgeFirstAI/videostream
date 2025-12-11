// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use crate::metrics::MetricsCollector;
use crate::utils;
use clap::Args as ClapArgs;
use std::sync::atomic::Ordering;
use videostream::{camera, encoder, fourcc::FourCC, frame::Frame, host::Host};

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// VSL socket path
    socket: String,

    /// Camera device
    #[arg(short, long, default_value = "/dev/video3")]
    device: String,

    /// Resolution in WxH format
    #[arg(short, long, default_value = "1920x1080")]
    resolution: String,

    /// Pixel format FOURCC
    #[arg(long, default_value = "YUYV")]
    format: String,

    /// Target frame rate
    #[arg(short = 'F', long, default_value = "30")]
    fps: i32,

    /// Enable H.264 encoding
    #[arg(short, long)]
    encode: bool,

    /// Encoding bitrate in kbps
    #[arg(short, long, default_value = "25000")]
    bitrate: String,

    /// Number of frames to process (0=unlimited)
    #[arg(short, long, default_value = "0")]
    frames: u64,

    /// Print performance metrics on exit
    #[arg(long)]
    metrics: bool,

    /// Print metrics every N seconds
    #[arg(long)]
    metrics_interval: Option<u64>,
}

pub fn execute(args: Args, json: bool) -> Result<(), CliError> {
    log::info!("Starting camera stream to {}", args.socket);
    log::debug!("Stream parameters: {:?}", args);

    // Parse resolution
    let (width, height) = utils::parse_resolution(&args.resolution)?;
    log::debug!("Resolution: {}x{}", width, height);

    // Parse FOURCC
    let fourcc = utils::fourcc_from_str(&args.format)?;
    log::debug!("Input format: {} (0x{:08x})", args.format, fourcc);

    // Install signal handler for graceful shutdown
    let term = utils::install_signal_handler()?;

    // Warn if metrics-interval is requested (not yet implemented)
    if let Some(interval) = args.metrics_interval {
        log::warn!(
            "--metrics-interval is not yet implemented (requested: {}s). \
             Metrics will only be displayed at the end of streaming.",
            interval
        );
    }

    // Create encoder if requested (using helper to reduce complexity)
    let (encoder_opt, _output_fourcc) =
        utils::create_encoder_if_requested(args.encode, "h264", &args.bitrate, args.fps, fourcc)?;

    // Open camera
    log::info!("Opening camera: {}", args.device);
    let cam = camera::create_camera()
        .with_device(&args.device)
        .with_resolution(width, height)
        .with_format(FourCC(fourcc.to_le_bytes()))
        .open()?;

    log::info!("Starting camera capture");
    cam.start()?;

    // Create VSL host
    log::info!("Creating VSL host at: {}", args.socket);
    let host = Host::new(&args.socket)?;
    log::info!("VSL host ready, waiting for clients...");

    // Metrics collection
    let mut metrics_collector = if args.metrics || json {
        Some(MetricsCollector::new())
    } else {
        None
    };

    let mut frame_count = 0u64;
    let max_frames = utils::normalize_frame_count(args.frames);

    // Pre-calculate estimated frame size for metrics (using helper to reduce complexity)
    let estimated_frame_size = utils::estimate_frame_size(
        width as u32,
        height as u32,
        args.encode,
        &args.bitrate,
        args.fps,
        fourcc,
    )?;

    // Main streaming loop
    log::info!(
        "Streaming {} frames (Ctrl+C to stop)...",
        if max_frames == u64::MAX {
            "unlimited".to_string()
        } else {
            max_frames.to_string()
        }
    );

    while frame_count < max_frames && !term.load(Ordering::Relaxed) {
        // Read frame from camera
        let buffer = cam.read()?;

        // Convert camera buffer to frame or encode it
        let output_frame = if let Some(ref encoder) = encoder_opt {
            // Create input frame from camera buffer
            let input_frame: Frame = (&buffer).try_into()?;

            // Create output frame for encoded data
            let output_frame = encoder.new_output_frame(
                width, height, -1, // duration (not used)
                -1, // PTS (not used)
                -1, // DTS (not used)
            )?;

            // Encode the frame
            let crop = encoder::VSLRect::new(0, 0, width, height);
            let mut keyframe: i32 = 0;

            unsafe {
                encoder.frame(&input_frame, &output_frame, &crop, &mut keyframe)?;
            }

            log::trace!(
                "Encoded frame {} (keyframe: {})",
                frame_count,
                keyframe != 0
            );

            output_frame
        } else {
            // Raw frame - convert camera buffer to frame
            (&buffer).try_into()?
        };

        // Get current timestamp for frame expiration
        let now = videostream::timestamp()?;
        let expires = now + 90_000_000; // 90ms expiration (like camhost.c)

        // Post frame to host (ownership transfers)
        host.post(output_frame, expires, -1, -1, -1)?;

        // Poll for client activity
        if host.poll(1)? > 0 {
            host.process()?;
        }

        // Track metrics if enabled
        if let Some(ref mut metrics) = metrics_collector {
            // Use pre-calculated frame size estimate
            metrics.record_bytes(estimated_frame_size);
            // Latency not applicable for streaming (we're the source)
            metrics.record_latency_us(0);
            metrics.track_serial(frame_count as i64);
        }

        frame_count += 1;

        // Log progress periodically
        if frame_count.is_multiple_of(30) {
            log::debug!("Streamed {} frames", frame_count);
        }

        // Print interval metrics if requested
        if let Some(interval) = args.metrics_interval {
            // TODO: Implement interval metrics printing
            // This would require tracking time and printing every N seconds
            let _ = interval; // Suppress unused warning for now
        }
    }

    if term.load(Ordering::Relaxed) {
        log::info!("Received Ctrl+C, stopping...");
    }

    log::info!("Streamed {} frames total", frame_count);

    // Print final metrics if requested
    if let Some(ref mut metrics) = metrics_collector {
        if json {
            metrics
                .print_json()
                .map_err(|e| CliError::General(format!("Failed to output JSON metrics: {}", e)))?;
        } else if args.metrics {
            metrics.print_text();
        }
    }

    Ok(())
}
