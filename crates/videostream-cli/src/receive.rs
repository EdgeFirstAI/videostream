// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use crate::metrics::MetricsCollector;
use crate::utils;
use clap::Args as ClapArgs;
use std::sync::atomic::Ordering;
use videostream::{client::Client, client::Reconnect, decoder};

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// VSL socket path to connect to
    socket: String,

    /// Number of frames to receive
    #[arg(short, long, default_value = "100")]
    frames: u64,

    /// Receive timeout in seconds
    #[arg(short, long, default_value = "5.0")]
    timeout: f64,

    /// Decode H.264/H.265 frames
    #[arg(long)]
    decode: bool,

    /// Print detailed performance metrics
    #[arg(long)]
    metrics: bool,
}

pub fn execute(args: Args, json: bool) -> Result<(), CliError> {
    log::info!("Connecting to socket: {}", args.socket);
    log::debug!("Receive parameters: {:?}", args);

    // Install signal handler for graceful shutdown
    let term = utils::install_signal_handler()?;

    // Create VSL client with auto-reconnect
    let client = Client::new(&args.socket, Reconnect::Yes)?;
    log::info!("Connected to {}", args.socket);

    // Set timeout
    let timeout_secs = args.timeout as f32;
    client.set_timeout(timeout_secs)?;
    log::debug!("Set timeout to {:.1} seconds", timeout_secs);

    // Create decoder if requested
    let mut decoder_opt = None;
    if args.decode {
        if !decoder::is_available().unwrap_or(false) {
            return Err(CliError::EncoderUnavailable(
                "VPU decoder not available on this system".to_string(),
            ));
        }

        // Try H.264 first, most common
        let dec = decoder::Decoder::create(decoder::DecoderInputCodec::H264, 30)?;
        log::info!("Created H.264 decoder");
        decoder_opt = Some(dec);
    }

    // Create metrics collector
    let mut metrics_collector = MetricsCollector::new();
    let mut frame_count = 0u64;
    let max_frames = if args.frames == 0 {
        u64::MAX
    } else {
        args.frames
    };

    // Receive frames
    log::info!(
        "Receiving {} frames (Ctrl+C to stop)...",
        if max_frames == u64::MAX {
            "unlimited".to_string()
        } else {
            max_frames.to_string()
        }
    );

    while frame_count < max_frames && !term.load(Ordering::Relaxed) {
        // Get frame with timeout (0 = wait indefinitely)
        let frame = match client.get_frame(0) {
            Ok(f) => f,
            Err(e) => {
                // Check if it's a timeout
                if matches!(e, videostream::Error::Io(ref io_err) if io_err.kind() == std::io::ErrorKind::TimedOut)
                {
                    log::warn!("Timeout waiting for frame");
                    return Err(CliError::Timeout(format!(
                        "Timeout after {:.1}s waiting for frame",
                        timeout_secs
                    )));
                }
                return Err(e.into());
            }
        };

        // Calculate latency
        let now = videostream::timestamp()?;
        let frame_ts = frame.timestamp()?;
        let latency_ns = now - frame_ts;
        metrics_collector.record_latency_ns(latency_ns);

        // Record bytes
        let frame_size = frame.size()? as u64;
        metrics_collector.record_bytes(frame_size);

        // Track serial for dropped frames
        let serial = frame.serial()?;
        let drops = metrics_collector.track_serial(serial);
        if drops > 0 {
            log::warn!("Detected {} dropped frame(s)", drops);
        }

        // Decode if requested
        if let Some(ref decoder) = decoder_opt {
            // Lock frame for reading
            frame.trylock()?;
            let data = frame.mmap()?;

            // Decode the frame
            let (_ret_code, _bytes_used, _output_frame) = decoder.decode_frame(data)?;

            frame.unlock()?;

            log::trace!("Decoded frame {}", serial);
        }

        frame_count += 1;

        // Log progress periodically
        if frame_count.is_multiple_of(30) {
            log::debug!("Received {} frames", frame_count);
        }
    }

    if term.load(Ordering::Relaxed) {
        log::info!("Received Ctrl+C, stopping...");
    }

    log::info!("Received {} frames total", frame_count);

    // Print metrics if requested or JSON mode
    if args.metrics || json {
        if json {
            metrics_collector
                .print_json()
                .map_err(|e| CliError::General(format!("Failed to output JSON metrics: {}", e)))?;
        } else {
            metrics_collector.print_text();
        }
    }

    Ok(())
}
