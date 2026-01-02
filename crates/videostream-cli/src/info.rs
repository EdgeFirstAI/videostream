// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use clap::Args as ClapArgs;
use serde::Serialize;
use std::fs;
use std::path::Path;
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

    /// Show V4L2 codec device information
    #[arg(long)]
    v4l2: bool,
}

#[derive(Debug, Serialize)]
struct SystemInfo {
    version: String,

    #[serde(skip_serializing_if = "Option::is_none")]
    camera: Option<CameraInfo>,

    #[serde(skip_serializing_if = "Option::is_none")]
    encoder: Option<HardwareInfo>,

    #[serde(skip_serializing_if = "Option::is_none")]
    decoder: Option<HardwareInfo>,

    #[serde(skip_serializing_if = "Option::is_none")]
    v4l2_codecs: Option<V4L2CodecInfo>,
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
    backend_selection: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    codecs: Option<Vec<String>>,
}

#[derive(Debug, Serialize)]
struct V4L2CodecInfo {
    devices: Vec<V4L2Device>,
}

#[derive(Debug, Serialize)]
struct V4L2Device {
    path: String,
    name: String,
    driver: String,
    is_encoder: bool,
    is_decoder: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    formats: Option<Vec<String>>,
}

pub fn execute(args: Args, json: bool) -> Result<(), CliError> {
    log::debug!("Executing info command: {:?}", args);

    // Determine what to show: if --all or no specific flags, show everything
    let show_all = args.all || !(args.camera || args.encoder || args.decoder || args.v4l2);
    let show_camera = show_all || args.camera;
    let show_encoder = show_all || args.encoder;
    let show_decoder = show_all || args.decoder;
    let show_v4l2 = show_all || args.v4l2;

    let version = videostream::version().unwrap_or_else(|_| "unknown".to_string());

    let mut info = SystemInfo {
        version,
        camera: None,
        encoder: None,
        decoder: None,
        v4l2_codecs: None,
    };

    // Query camera information
    if show_camera {
        info.camera = Some(query_camera_info(&args.device)?);
    }

    // Query encoder information
    if show_encoder {
        info.encoder = Some(query_encoder_info());
    }

    // Query decoder information
    if show_decoder {
        info.decoder = Some(query_decoder_info());
    }

    // Query V4L2 codec devices
    if show_v4l2 {
        info.v4l2_codecs = Some(query_v4l2_codecs());
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

    HardwareInfo {
        available,
        backend_selection: None, // Encoder doesn't have backend selection yet
        codecs,
    }
}

fn query_decoder_info() -> HardwareInfo {
    log::debug!("Querying decoder availability");

    let available = decoder::is_available().unwrap_or(false);
    let backend_selection = decoder::is_backend_selection_available().unwrap_or(false);

    let codecs = if available {
        // VPU decoder typically supports H.264 and H.265
        Some(vec!["H.264".to_string(), "H.265 (HEVC)".to_string()])
    } else {
        None
    };

    HardwareInfo {
        available,
        backend_selection: Some(backend_selection),
        codecs,
    }
}

fn query_v4l2_codecs() -> V4L2CodecInfo {
    log::debug!("Querying V4L2 codec devices");

    let mut devices = Vec::new();

    // Scan /dev/video* for codec devices
    if let Ok(entries) = fs::read_dir("/dev") {
        for entry in entries.flatten() {
            let path = entry.path();
            if let Some(name) = path.file_name() {
                let name_str = name.to_string_lossy();
                if name_str.starts_with("video") {
                    if let Some(device) = probe_v4l2_device(&path) {
                        devices.push(device);
                    }
                }
            }
        }
    }

    // Sort by path
    devices.sort_by(|a, b| a.path.cmp(&b.path));

    V4L2CodecInfo { devices }
}

fn probe_v4l2_device(path: &Path) -> Option<V4L2Device> {
    // Read device capabilities from sysfs
    let path_str = path.to_string_lossy().to_string();
    let video_num = path_str.strip_prefix("/dev/video")?;

    // Try to read device name from sysfs
    let sysfs_name = format!("/sys/class/video4linux/video{}/name", video_num);
    let name = fs::read_to_string(&sysfs_name)
        .map(|s| s.trim().to_string())
        .unwrap_or_default();

    // Read the symlink to check device path (contains driver info)
    let sysfs_link = format!("/sys/class/video4linux/video{}", video_num);
    let device_path = fs::read_link(&sysfs_link)
        .map(|p| p.to_string_lossy().to_string())
        .unwrap_or_default();

    let name_lower = name.to_lowercase();
    let path_lower = device_path.to_lowercase();

    // Check if it's a VPU codec device by path or name
    let is_vpu_device = path_lower.contains("vpu")
        || path_lower.contains("vsi")
        || name_lower.contains("vpu")
        || name_lower.contains("vsi")
        || name_lower.contains("hantro");

    // Check if it's a codec device by name patterns
    let is_encoder = name_lower.contains("enc");
    let is_decoder = name_lower.contains("dec");

    // For VPU devices with empty names, check device index convention
    // On i.MX8: video0 is typically decoder, video1 is encoder
    let (is_encoder, is_decoder) = if is_vpu_device && name.is_empty() {
        match video_num {
            "0" => (false, true), // VPU decoder
            "1" => (true, false), // VPU encoder
            _ => (is_encoder, is_decoder),
        }
    } else {
        (is_encoder, is_decoder)
    };

    // Skip non-codec devices (cameras, ISP, etc.)
    let is_codec = is_vpu_device
        || is_encoder
        || is_decoder
        || name_lower.contains("codec")
        || name_lower.contains("m2m");

    if !is_codec {
        return None;
    }

    // Skip ISI m2m devices (image scaling, not video codec)
    if name_lower.contains("isi") || name_lower.contains("isp") {
        return None;
    }

    // Infer driver from path or name
    let driver = if path_lower.contains("vpu") || path_lower.contains("vsi") {
        "vsi_v4l2".to_string()
    } else if name_lower.contains("hantro") || path_lower.contains("hantro") {
        "hantro-vpu".to_string()
    } else {
        "unknown".to_string()
    };

    // Display name - use path-based inference if name is empty
    let display_name = if name.is_empty() {
        if is_decoder {
            "VPU Decoder".to_string()
        } else if is_encoder {
            "VPU Encoder".to_string()
        } else {
            "VPU Device".to_string()
        }
    } else {
        name
    };

    // Determine supported formats based on device type
    let formats = if is_decoder || is_encoder {
        Some(vec![
            "H.264".to_string(),
            "H.265/HEVC".to_string(),
            "VP8".to_string(),
            "VP9".to_string(),
        ])
    } else {
        None
    };

    Some(V4L2Device {
        path: path_str,
        name: display_name,
        driver,
        is_encoder,
        is_decoder,
        formats,
    })
}

fn print_text_info(info: &SystemInfo) {
    println!("VideoStream System Information");
    println!("===============================");
    println!("Library Version: {}\n", info.version);

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

        if let Some(backend_sel) = dec.backend_selection {
            println!(
                "  Backend Selection: {}",
                if backend_sel {
                    "✓ Supported (V4L2, Hantro)"
                } else {
                    "✗ Not supported"
                }
            );
        }

        if let Some(ref codecs) = dec.codecs {
            println!("  Supported codecs:");
            for codec in codecs {
                println!("    - {}", codec);
            }
        }
        println!();
    }

    // Print V4L2 codec devices
    if let Some(ref v4l2) = info.v4l2_codecs {
        println!("V4L2 Codec Devices:");
        if v4l2.devices.is_empty() {
            println!("  No codec devices found");
        } else {
            for device in &v4l2.devices {
                println!("  {}:", device.path);
                println!("    Name: {}", device.name);
                println!("    Driver: {}", device.driver);
                println!(
                    "    Type: {}",
                    match (device.is_encoder, device.is_decoder) {
                        (true, true) => "Encoder + Decoder",
                        (true, false) => "Encoder",
                        (false, true) => "Decoder",
                        (false, false) => "Codec (type unknown)",
                    }
                );
                if let Some(ref formats) = device.formats {
                    println!("    Formats: {}", formats.join(", "));
                }
            }
        }
        println!();
    }
}
