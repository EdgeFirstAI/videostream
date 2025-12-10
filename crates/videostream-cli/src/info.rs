// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use clap::Args as ClapArgs;
use serde::Serialize;
use videostream::{camera, decoder, encoder};

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// Camera device path
    #[arg(short, long, default_value = "/dev/video3")]
    device: String,

    /// Show all information
    #[arg(long)]
    all: bool,

    /// Show camera capabilities
    #[arg(long)]
    camera: bool,

    /// Show encoder capabilities
    #[arg(long)]
    encoder: bool,

    /// Show decoder capabilities
    #[arg(long)]
    decoder: bool,
}

#[derive(Debug, Serialize)]
struct SystemInfo {
    #[serde(skip_serializing_if = "Option::is_none")]
    camera: Option<CameraInfo>,

    #[serde(skip_serializing_if = "Option::is_none")]
    encoder: Option<HardwareInfo>,

    #[serde(skip_serializing_if = "Option::is_none")]
    decoder: Option<HardwareInfo>,
}

#[derive(Debug, Serialize)]
struct CameraInfo {
    device: String,
    available: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    formats: Option<Vec<String>>,
}

#[derive(Debug, Serialize)]
struct HardwareInfo {
    available: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    codecs: Option<Vec<String>>,
}

pub fn execute(args: Args, json: bool) -> Result<(), CliError> {
    log::debug!("Executing info command: {:?}", args);

    // Determine what to show
    let show_all = args.all;
    let show_camera = show_all || args.camera;
    let show_encoder = show_all || args.encoder;
    let show_decoder = show_all || args.decoder;

    // If nothing specific requested, show all
    let show_all = show_all || !(show_camera || show_encoder || show_decoder);

    let mut info = SystemInfo {
        camera: None,
        encoder: None,
        decoder: None,
    };

    // Query camera information
    if show_all || show_camera {
        info.camera = Some(query_camera_info(&args.device)?);
    }

    // Query encoder information
    if show_all || show_encoder {
        info.encoder = Some(query_encoder_info());
    }

    // Query decoder information
    if show_all || show_decoder {
        info.decoder = Some(query_decoder_info());
    }

    // Output results
    if json {
        let json_str = serde_json::to_string_pretty(&info)
            .map_err(|e| CliError::General(format!("Failed to serialize JSON: {}", e)))?;
        println!("{}", json_str);
    } else {
        print_text_info(&info);
    }

    Ok(())
}

fn query_camera_info(device: &str) -> Result<CameraInfo, CliError> {
    log::debug!("Querying camera info for: {}", device);

    // Try to open the camera device
    let cam_result = camera::create_camera().with_device(device).open();

    match cam_result {
        Ok(_cam) => {
            // Camera is available
            // TODO: Query supported formats from camera
            // For now, just report that it's available
            Ok(CameraInfo {
                device: device.to_string(),
                available: true,
                formats: None, // TODO: Implement format querying
            })
        }
        Err(e) => {
            log::warn!("Camera not available: {}", e);
            Ok(CameraInfo {
                device: device.to_string(),
                available: false,
                formats: None,
            })
        }
    }
}

fn query_encoder_info() -> HardwareInfo {
    log::debug!("Querying encoder availability");

    let available = encoder::is_available().unwrap_or(false);

    let codecs = if available {
        // VPU encoder typically supports H.264 and H.265
        Some(vec!["H.264".to_string(), "H.265 (HEVC)".to_string()])
    } else {
        None
    };

    HardwareInfo { available, codecs }
}

fn query_decoder_info() -> HardwareInfo {
    log::debug!("Querying decoder availability");

    let available = decoder::is_available().unwrap_or(false);

    let codecs = if available {
        // VPU decoder typically supports H.264 and H.265
        Some(vec!["H.264".to_string(), "H.265 (HEVC)".to_string()])
    } else {
        None
    };

    HardwareInfo { available, codecs }
}

fn print_text_info(info: &SystemInfo) {
    println!("VideoStream System Information");
    println!("===============================\n");

    // Print camera info
    if let Some(ref cam) = info.camera {
        println!("Camera: {}", cam.device);
        println!(
            "  Status: {}",
            if cam.available {
                "✓ Available"
            } else {
                "✗ Not available"
            }
        );

        if let Some(ref formats) = cam.formats {
            println!("  Supported formats:");
            for format in formats {
                println!("    - {}", format);
            }
        }
        println!();
    }

    // Print encoder info
    if let Some(ref enc) = info.encoder {
        println!("Hardware Encoder (VPU):");
        println!(
            "  Status: {}",
            if enc.available {
                "✓ Available"
            } else {
                "✗ Not available"
            }
        );

        if let Some(ref codecs) = enc.codecs {
            println!("  Supported codecs:");
            for codec in codecs {
                println!("    - {}", codec);
            }
        }
        println!();
    }

    // Print decoder info
    if let Some(ref dec) = info.decoder {
        println!("Hardware Decoder (VPU):");
        println!(
            "  Status: {}",
            if dec.available {
                "✓ Available"
            } else {
                "✗ Not available"
            }
        );

        if let Some(ref codecs) = dec.codecs {
            println!("  Supported codecs:");
            for codec in codecs {
                println!("    - {}", codec);
            }
        }
        println!();
    }
}
