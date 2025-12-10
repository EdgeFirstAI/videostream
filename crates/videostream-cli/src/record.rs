// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use crate::utils;
use clap::Args as ClapArgs;
use std::fs::File;
use std::io::Write;
use std::sync::atomic::Ordering;
use std::time::Instant;
use videostream::{camera, encoder, fourcc::FourCC, frame::Frame};

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// Output file path (.h264 or .h265)
    output: String,

    /// Camera device
    #[arg(short, long, default_value = "/dev/video3")]
    device: String,

    /// Resolution in WxH format
    #[arg(short, long, default_value = "1920x1080")]
    resolution: String,

    /// Input pixel format
    #[arg(long, default_value = "YUYV")]
    format: String,

    /// Target frame rate
    #[arg(short = 'F', long, default_value = "30")]
    fps: i32,

    /// Encoding bitrate in kbps
    #[arg(short, long, default_value = "25000")]
    bitrate: String,

    /// Number of frames (0=unlimited)
    #[arg(short, long, default_value = "0")]
    frames: u64,

    /// Recording duration in seconds
    #[arg(short = 't', long)]
    duration: Option<u64>,

    /// Video codec: h264|h265
    #[arg(long, default_value = "h264")]
    codec: String,
}

pub fn execute(args: Args, _json: bool) -> Result<(), CliError> {
    log::info!("Recording to file: {}", args.output);
    log::debug!("Record parameters: {:?}", args);

    // Validate output file extension
    let expected_ext = match args.codec.as_str() {
        "h264" => ".h264",
        "h265" | "hevc" => ".h265",
        _ => "",
    };
    if !args.output.ends_with(expected_ext) {
        log::warn!(
            "Output file doesn't have {} extension (recommended for {})",
            expected_ext,
            args.codec.to_uppercase()
        );
    }

    // Parse resolution
    let (width, height) = utils::parse_resolution(&args.resolution)?;
    log::debug!("Resolution: {}x{}", width, height);

    // Parse FOURCC for input format
    let fourcc = utils::fourcc_from_str(&args.format)?;
    log::debug!("Input format: {} (0x{:08x})", args.format, fourcc);

    // Parse bitrate
    let bitrate_kbps = utils::parse_bitrate(&args.bitrate)?;
    log::info!("Encoding at {} kbps", bitrate_kbps);

    // Parse codec
    let output_fourcc = match args.codec.to_lowercase().as_str() {
        "h264" => u32::from_le_bytes(*b"H264"),
        "h265" | "hevc" => u32::from_le_bytes(*b"HEVC"),
        _ => {
            return Err(CliError::InvalidArgs(format!(
                "Invalid codec: {} (supported: h264, h265)",
                args.codec
            )))
        }
    };

    // Check encoder availability
    if !encoder::is_available().unwrap_or(false) {
        return Err(CliError::EncoderUnavailable(
            "VPU encoder not available on this system. Recording requires hardware encoder."
                .to_string(),
        ));
    }

    // Install signal handler for graceful shutdown
    let term = utils::install_signal_handler()?;

    // Open camera FIRST (before encoder to avoid DMA conflicts)
    log::info!("Opening camera: {}", args.device);
    let cam = camera::create_camera()
        .with_device(&args.device)
        .with_resolution(width, height)
        .with_format(FourCC(fourcc.to_le_bytes()))
        .open()?;

    log::info!("Starting camera capture");
    cam.start()?;

    // Map bitrate to encoder profile
    let profile = match bitrate_kbps {
        0..=7500 => encoder::VSLEncoderProfileEnum::Kbps5000,
        7501..=37500 => encoder::VSLEncoderProfileEnum::Kbps25000,
        37501..=75000 => encoder::VSLEncoderProfileEnum::Kbps50000,
        _ => encoder::VSLEncoderProfileEnum::Kbps100000,
    };

    // Create encoder AFTER camera is started
    log::info!("Creating {} encoder", args.codec.to_uppercase());
    let encoder = encoder::Encoder::create(profile as u32, output_fourcc, args.fps)?;
    log::debug!("Created encoder with profile {:?}", profile);

    // Open output file for raw bitstream
    let mut output_file = File::create(&args.output)
        .map_err(|e| CliError::General(format!("Failed to create output file: {}", e)))?;

    log::info!("Recording started...");
    log::info!(
        "Output format: Raw {} Annex-B bitstream (power-loss resilient)",
        args.codec.to_uppercase()
    );

    // Calculate limits
    let start_time = Instant::now();
    let max_frames = if args.frames == 0 {
        u64::MAX
    } else {
        args.frames
    };
    let max_duration = args.duration.map(std::time::Duration::from_secs);

    let mut frame_count = 0u64;
    let crop = encoder::VSLRect::new(0, 0, width, height);

    // Main recording loop
    while frame_count < max_frames && !term.load(Ordering::Relaxed) {
        // Check duration limit
        if let Some(max_dur) = max_duration {
            if start_time.elapsed() >= max_dur {
                log::info!("Duration limit reached");
                break;
            }
        }

        // Read frame from camera
        log::trace!("Reading frame {} from camera", frame_count);
        let buffer = cam.read()?;
        log::debug!("Camera read succeeded: frame {}", frame_count);

        // Create output frame for encoded data
        log::trace!("Creating output frame for encoder");
        let output_frame = encoder.new_output_frame(
            width,
            height,
            -1,                 // duration
            frame_count as i64, // PTS
            frame_count as i64, // DTS
        )?;
        log::debug!("Output frame created successfully");

        // Encode the frame - keep buffer alive during encoding
        let keyframe = {
            // Create input frame from camera buffer (borrows buffer)
            log::trace!("Converting CameraBuffer to Frame");
            let input_frame: Frame = (&buffer).try_into()?;
            log::debug!("Input frame created successfully");

            // Encode the frame
            log::trace!("Encoding frame {}", frame_count);
            let mut keyframe: i32 = 0;
            unsafe {
                encoder.frame(&input_frame, &output_frame, &crop, &mut keyframe)?;
            }
            log::debug!(
                "Frame {} encoded successfully (keyframe={})",
                frame_count,
                keyframe
            );

            // input_frame drops here, before buffer
            keyframe
        };
        // buffer drops here, after input_frame

        // Write encoded frame to file (raw Annex-B bitstream)
        // Note: Encoder output frames don't need locking (they're not from a client)
        log::trace!("Memory mapping output frame");
        let frame_data = output_frame.mmap()?;
        log::debug!("Output frame mapped, size={} bytes", frame_data.len());

        log::trace!("Writing frame data to file");
        output_file
            .write_all(frame_data)
            .map_err(|e| CliError::General(format!("Failed to write frame data: {}", e)))?;
        log::debug!("Frame data written successfully");

        if keyframe != 0 {
            log::trace!("Recorded keyframe {}", frame_count);
        }

        frame_count += 1;

        // Log progress periodically
        if frame_count.is_multiple_of(30) {
            let elapsed = start_time.elapsed().as_secs_f64();
            let fps = frame_count as f64 / elapsed;
            log::info!("Recorded {} frames ({:.1} fps)", frame_count, fps);
        }
    }

    if term.load(Ordering::Relaxed) {
        log::info!("Received Ctrl+C, stopping...");
    }

    // Flush and close file
    output_file
        .flush()
        .map_err(|e| CliError::General(format!("Failed to flush output file: {}", e)))?;

    let elapsed = start_time.elapsed();
    let fps = frame_count as f64 / elapsed.as_secs_f64();

    log::info!(
        "Recording complete: {} frames in {:.1}s ({:.1} fps)",
        frame_count,
        elapsed.as_secs_f64(),
        fps
    );
    log::info!("Output file: {}", args.output);
    log::info!(
        "Format: Raw {} Annex-B bitstream",
        args.codec.to_uppercase()
    );

    // Print playback and conversion instructions
    println!();
    println!("===================================================================");
    println!("  Playback:");
    println!("    vlc {}", args.output);
    println!("    mpv --demuxer={} {}", args.codec, args.output);
    println!(
        "    ffplay -f {} -framerate {} {}",
        args.codec, args.fps, args.output
    );
    println!();
    println!("  Convert to MP4:");
    println!("    videostream convert {} output.mp4", args.output);
    println!("    ffmpeg -i {} -c copy output.mp4", args.output);
    println!("===================================================================");

    Ok(())
}
