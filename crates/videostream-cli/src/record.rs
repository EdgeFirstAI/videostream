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

/// Parsed recording configuration
struct RecordConfig {
    width: i32,
    height: i32,
    fourcc: u32,
    bitrate_kbps: u32,
    output_fourcc: u32,
}

impl RecordConfig {
    fn from_args(args: &Args) -> Result<Self, CliError> {
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

        let (width, height) = utils::parse_resolution(&args.resolution)?;
        log::debug!("Resolution: {}x{}", width, height);

        let fourcc = utils::fourcc_from_str(&args.format)?;
        log::debug!("Input format: {} (0x{:08x})", args.format, fourcc);

        let bitrate_kbps = utils::parse_bitrate(&args.bitrate)?;
        let output_fourcc = utils::codec_to_fourcc(&args.codec)?;
        log::info!(
            "Encoding {} at {} kbps",
            args.codec.to_uppercase(),
            bitrate_kbps
        );

        // Check encoder availability
        if !encoder::is_available().unwrap_or(false) {
            return Err(CliError::EncoderUnavailable(
                "VPU encoder not available on this system. Recording requires hardware encoder."
                    .to_string(),
            ));
        }

        Ok(Self {
            width,
            height,
            fourcc,
            bitrate_kbps,
            output_fourcc,
        })
    }
}

/// Recording duration and frame limits
struct RecordLimits {
    max_frames: u64,
    max_duration: Option<std::time::Duration>,
}

impl RecordLimits {
    fn from_args(args: &Args) -> Self {
        Self {
            max_frames: if args.frames == 0 {
                u64::MAX
            } else {
                args.frames
            },
            max_duration: args.duration.map(std::time::Duration::from_secs),
        }
    }

    fn should_continue(&self, frame_count: u64, elapsed: std::time::Duration) -> bool {
        if frame_count >= self.max_frames {
            return false;
        }
        if let Some(max_dur) = self.max_duration {
            if elapsed >= max_dur {
                log::info!("Duration limit reached");
                return false;
            }
        }
        true
    }
}

/// Initialize camera with specified configuration
fn init_camera(
    args: &Args,
    width: i32,
    height: i32,
    fourcc: u32,
) -> Result<camera::CameraReader, CliError> {
    log::info!("Opening camera: {}", args.device);
    let cam = camera::create_camera()
        .with_device(&args.device)
        .with_resolution(width, height)
        .with_format(FourCC(fourcc.to_le_bytes()))
        .open()?;

    log::info!("Starting camera capture");
    cam.start()?;
    Ok(cam)
}

/// Initialize encoder with specified configuration
fn init_encoder(args: &Args, config: &RecordConfig) -> Result<encoder::Encoder, CliError> {
    log::info!("Creating {} encoder", args.codec.to_uppercase());
    let profile = utils::bitrate_to_encoder_profile(config.bitrate_kbps);
    let encoder = encoder::Encoder::create(profile as u32, config.output_fourcc, args.fps)?;
    log::debug!("Created encoder with profile {:?}", profile);
    Ok(encoder)
}

/// Create output file for bitstream
fn create_output_file(path: &str) -> Result<File, CliError> {
    File::create(path)
        .map_err(|e| CliError::General(format!("Failed to create output file: {}", e)))
}

pub fn execute(args: Args, _json: bool) -> Result<(), CliError> {
    log::info!("Recording to file: {}", args.output);
    log::debug!("Record parameters: {:?}", args);

    // Validate and parse arguments
    let config = RecordConfig::from_args(&args)?;

    // Initialize resources
    let term = utils::install_signal_handler()?;
    let cam = init_camera(&args, config.width, config.height, config.fourcc)?;
    let encoder = init_encoder(&args, &config)?;
    let mut output_file = create_output_file(&args.output)?;

    log::info!("Recording started...");
    log::info!(
        "Output format: Raw {} Annex-B bitstream (power-loss resilient)",
        args.codec.to_uppercase()
    );

    // Main recording loop
    let start_time = Instant::now();
    let limits = RecordLimits::from_args(&args);
    let mut frame_count = 0u64;
    let crop = encoder::VSLRect::new(0, 0, config.width, config.height);

    while limits.should_continue(frame_count, start_time.elapsed()) && !term.load(Ordering::Relaxed)
    {
        // Read frame from camera
        log::trace!("Reading frame {} from camera", frame_count);
        let buffer = cam.read()?;
        log::debug!("Camera read succeeded: frame {}", frame_count);

        // Create output frame for encoded data
        log::trace!("Creating output frame for encoder");
        let output_frame = encoder.new_output_frame(
            config.width,
            config.height,
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

        // IMPORTANT: Buffer lifetime and memory safety
        // -------------------------------------------
        // The buffer MUST outlive input_frame because input_frame borrows from buffer.
        // Dropping buffer before input_frame would cause use-after-free bugs.
        //
        // In hardware-accelerated pipelines, buffer may be mapped directly to device
        // memory (DMA buffers). Premature deallocation can cause:
        // - Memory corruption
        // - DMA transfer failures
        // - Hardware encoder hangs
        //
        // Always preserve this scoping pattern to ensure memory safety.
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
