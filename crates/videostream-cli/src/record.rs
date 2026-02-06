// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use crate::utils;
use clap::{Args as ClapArgs, ValueEnum};
use std::fs::File;
use std::io::Write;
use std::sync::atomic::Ordering;
use std::time::Instant;
use videostream::{camera, client, encoder, fourcc::FourCC, frame::Frame};

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// Output file path (.h264 or .h265)
    output: String,

    /// Camera device (mutually exclusive with --ipc)
    #[arg(short, long, default_value = "/dev/video3", conflicts_with = "ipc")]
    device: String,

    /// VSL IPC socket path (mutually exclusive with --device)
    #[arg(long)]
    ipc: Option<String>,

    /// Resolution in WxH format
    #[arg(short, long, default_value = "1920x1080")]
    resolution: String,

    /// Input pixel format (for camera input)
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

    /// IPC timeout in seconds
    #[arg(long, default_value = "5.0")]
    timeout: f64,

    /// Encoder backend: auto|v4l2|hantro
    #[arg(long, default_value = "auto")]
    backend: Backend,
}

/// Encoder backend selection
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, ValueEnum)]
pub enum Backend {
    /// Auto-detect best available backend
    #[default]
    Auto,
    /// V4L2 kernel driver (i.MX 95 Wave6, etc.)
    V4l2,
    /// Hantro userspace library (i.MX 8M, etc.)
    Hantro,
}

impl From<Backend> for encoder::CodecBackend {
    fn from(backend: Backend) -> Self {
        match backend {
            Backend::Auto => encoder::CodecBackend::Auto,
            Backend::V4l2 => encoder::CodecBackend::V4L2,
            Backend::Hantro => encoder::CodecBackend::Hantro,
        }
    }
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
    log::info!(
        "Creating {} encoder (backend: {:?})",
        args.codec.to_uppercase(),
        args.backend
    );
    let profile = utils::bitrate_to_encoder_profile(config.bitrate_kbps);
    let backend: encoder::CodecBackend = args.backend.into();
    let encoder =
        encoder::Encoder::create_ex(profile as u32, config.output_fourcc, args.fps, backend)?;
    log::debug!(
        "Created encoder with profile {:?}, backend {:?}",
        profile,
        args.backend
    );
    Ok(encoder)
}

/// Create output file for bitstream
fn create_output_file(path: &str) -> Result<File, CliError> {
    File::create(path)
        .map_err(|e| CliError::General(format!("Failed to create output file: {}", e)))
}

/// Frame source abstraction for camera or IPC input
enum FrameSource {
    Camera(camera::CameraReader),
    Ipc(client::Client),
}

/// Initialize IPC client for receiving frames
fn init_ipc_client(socket_path: &str, timeout: f64) -> Result<client::Client, CliError> {
    log::info!("Connecting to IPC socket: {}", socket_path);
    let client = client::Client::new(socket_path, client::Reconnect::Yes)?;
    client.set_timeout(timeout as f32)?;
    log::info!("Connected to IPC socket");
    Ok(client)
}

pub fn execute(args: Args, _json: bool) -> Result<(), CliError> {
    log::info!("Recording to file: {}", args.output);
    log::debug!("Record parameters: {:?}", args);

    // Validate and parse arguments
    let config = RecordConfig::from_args(&args)?;

    // Initialize resources
    let term = utils::install_signal_handler()?;

    // Initialize frame source (camera or IPC)
    let source = if let Some(ref ipc_path) = args.ipc {
        FrameSource::Ipc(init_ipc_client(ipc_path, args.timeout)?)
    } else {
        FrameSource::Camera(init_camera(
            &args,
            config.width,
            config.height,
            config.fourcc,
        )?)
    };

    let encoder = init_encoder(&args, &config)?;
    let mut output_file = create_output_file(&args.output)?;

    let source_name = match &source {
        FrameSource::Camera(_) => "camera",
        FrameSource::Ipc(_) => "IPC",
    };
    log::info!("Recording from {} started...", source_name);
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

        // Get frame from source and encode
        let keyframe = match &source {
            FrameSource::Camera(cam) => {
                // Read frame from camera
                log::trace!("Reading frame {} from camera", frame_count);
                let buffer = cam.read()?;
                log::debug!("Camera read succeeded: frame {}", frame_count);

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
                keyframe
            }
            FrameSource::Ipc(client) => {
                // Get frame from IPC socket
                log::trace!("Waiting for frame {} from IPC", frame_count);
                let input_frame = client.get_frame(0)?;
                log::debug!(
                    "IPC frame received: {}x{} serial={}",
                    input_frame.width()?,
                    input_frame.height()?,
                    input_frame.serial()?
                );

                // Lock frame for reading (required for IPC frames)
                input_frame.trylock()?;

                // Encode the frame
                log::trace!("Encoding frame {}", frame_count);
                let mut keyframe: i32 = 0;
                unsafe {
                    encoder.frame(&input_frame, &output_frame, &crop, &mut keyframe)?;
                }

                // Unlock frame
                input_frame.unlock()?;

                log::debug!(
                    "Frame {} encoded successfully (keyframe={})",
                    frame_count,
                    keyframe
                );
                keyframe
            }
        };

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
