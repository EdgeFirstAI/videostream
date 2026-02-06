// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! V4L2 device enumeration with filtering and smart grouping.

use crate::error::CliError;
use clap::Args as ClapArgs;
use serde::Serialize;
use std::collections::HashMap;
use videostream::v4l2::{Device, DeviceEnumerator, DeviceType};

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// Show only camera/capture devices
    #[arg(long)]
    cameras: bool,

    /// Show only video encoders (H.264, HEVC, JPEG)
    #[arg(long)]
    encoders: bool,

    /// Show only video decoders (H.264, HEVC, JPEG)
    #[arg(long)]
    decoders: bool,

    /// Show only ISP/scaler devices (non-codec M2M)
    #[arg(long)]
    converters: bool,

    /// Show all device nodes (disable grouping by hardware)
    #[arg(long)]
    all: bool,

    /// Show detailed format information
    #[arg(short, long)]
    verbose: bool,
}

#[derive(Debug, Serialize)]
struct DevicesOutput {
    cameras: Vec<DeviceGroup>,
    encoders: Vec<DeviceGroup>,
    decoders: Vec<DeviceGroup>,
    converters: Vec<DeviceGroup>,
    summary: Summary,
}

#[derive(Debug, Serialize)]
struct DeviceGroup {
    name: String,
    driver: String,
    bus: String,
    devices: Vec<DeviceInfo>,
    #[serde(skip_serializing_if = "Option::is_none")]
    formats: Option<Vec<String>>,
    memory: Vec<String>,
}

#[derive(Debug, Serialize)]
struct DeviceInfo {
    path: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    card: Option<String>,
}

#[derive(Debug, Serialize)]
struct Summary {
    total_devices: usize,
    hardware_units: usize,
    cameras: usize,
    encoders: usize,
    decoders: usize,
    converters: usize,
}

pub fn execute(args: Args, json: bool) -> Result<(), CliError> {
    log::debug!("Executing devices command: {:?}", args);

    // Determine filters - if none specified, show all types
    let show_all_types = !args.cameras && !args.encoders && !args.decoders && !args.converters;
    let show_cameras = show_all_types || args.cameras;
    let show_encoders = show_all_types || args.encoders;
    let show_decoders = show_all_types || args.decoders;
    let show_converters = show_all_types || args.converters;

    // Enumerate all devices
    let devices = DeviceEnumerator::enumerate()
        .map_err(|e| CliError::General(format!("Failed to enumerate V4L2 devices: {}", e)))?;

    // Classify devices
    let mut cameras: Vec<&Device> = Vec::new();
    let mut encoders: Vec<&Device> = Vec::new();
    let mut decoders: Vec<&Device> = Vec::new();
    let mut converters: Vec<&Device> = Vec::new();

    for device in &devices {
        match device.device_type() {
            DeviceType::Camera => cameras.push(device),
            DeviceType::Encoder => encoders.push(device),
            DeviceType::Decoder => decoders.push(device),
            DeviceType::Isp | DeviceType::Output | DeviceType::M2m => converters.push(device),
            DeviceType::Unknown => {
                // Try to classify by name
                let name = device.card().to_lowercase();
                if name.contains("isp") || name.contains("scaler") {
                    converters.push(device);
                }
            }
        }
    }

    // Group by bus_info unless --all is specified
    let camera_groups = if args.all {
        devices_to_ungrouped(&cameras, args.verbose)
    } else {
        group_by_bus(&cameras, args.verbose)
    };

    let encoder_groups = if args.all {
        devices_to_ungrouped(&encoders, args.verbose)
    } else {
        group_by_bus(&encoders, args.verbose)
    };

    let decoder_groups = if args.all {
        devices_to_ungrouped(&decoders, args.verbose)
    } else {
        group_by_bus(&decoders, args.verbose)
    };

    let converter_groups = if args.all {
        devices_to_ungrouped(&converters, args.verbose)
    } else {
        group_by_bus(&converters, args.verbose)
    };

    let output = DevicesOutput {
        summary: Summary {
            total_devices: devices.len(),
            hardware_units: camera_groups.len()
                + encoder_groups.len()
                + decoder_groups.len()
                + converter_groups.len(),
            cameras: camera_groups.len(),
            encoders: encoder_groups.len(),
            decoders: decoder_groups.len(),
            converters: converter_groups.len(),
        },
        cameras: if show_cameras { camera_groups } else { vec![] },
        encoders: if show_encoders {
            encoder_groups
        } else {
            vec![]
        },
        decoders: if show_decoders {
            decoder_groups
        } else {
            vec![]
        },
        converters: if show_converters {
            converter_groups
        } else {
            vec![]
        },
    };

    if json {
        let json_str = serde_json::to_string_pretty(&output)
            .map_err(|e| CliError::General(format!("JSON serialization failed: {}", e)))?;
        println!("{}", json_str);
    } else {
        print_text_output(
            &output,
            show_cameras,
            show_encoders,
            show_decoders,
            show_converters,
            args.verbose,
        );
    }

    Ok(())
}

/// Group devices by bus_info to deduplicate same hardware
fn group_by_bus(devices: &[&Device], verbose: bool) -> Vec<DeviceGroup> {
    let mut groups: HashMap<String, Vec<&Device>> = HashMap::new();

    for device in devices {
        let bus = device.bus().to_string();
        groups.entry(bus).or_default().push(device);
    }

    let mut result: Vec<DeviceGroup> = groups
        .into_iter()
        .map(|(bus, devs)| {
            // Use first device for group info
            let first = devs[0];
            let device_infos: Vec<DeviceInfo> = devs
                .iter()
                .map(|d| DeviceInfo {
                    path: d.path_str().to_string(),
                    card: if devs.len() > 1 && d.card() != first.card() {
                        Some(d.card().to_string())
                    } else {
                        None
                    },
                })
                .collect();

            // Collect formats from first device
            let formats = if verbose {
                collect_formats(first)
            } else {
                collect_formats_summary(first)
            };

            // Collect memory capabilities
            let memory = collect_memory_caps(first);

            DeviceGroup {
                name: first.card().to_string(),
                driver: first.driver().to_string(),
                bus,
                devices: device_infos,
                formats,
                memory,
            }
        })
        .collect();

    // Sort by name
    result.sort_by(|a, b| a.name.cmp(&b.name));
    result
}

/// Create ungrouped list (one group per device)
fn devices_to_ungrouped(devices: &[&Device], verbose: bool) -> Vec<DeviceGroup> {
    devices
        .iter()
        .map(|d| {
            let formats = if verbose {
                collect_formats(d)
            } else {
                collect_formats_summary(d)
            };

            DeviceGroup {
                name: d.card().to_string(),
                driver: d.driver().to_string(),
                bus: d.bus().to_string(),
                devices: vec![DeviceInfo {
                    path: d.path_str().to_string(),
                    card: None,
                }],
                formats,
                memory: collect_memory_caps(d),
            }
        })
        .collect()
}

/// Collect all formats (verbose mode)
fn collect_formats(device: &Device) -> Option<Vec<String>> {
    let mut formats = Vec::new();

    // Capture formats
    for fmt in device.capture_formats() {
        let suffix = if fmt.compressed { " (compressed)" } else { "" };
        formats.push(format!("{}{}", fmt.fourcc, suffix));
    }

    // Output formats for M2M devices
    if device.is_encoder() || device.is_decoder() {
        for fmt in device.output_formats() {
            let suffix = if fmt.compressed { " (compressed)" } else { "" };
            if !formats.contains(&format!("{}{}", fmt.fourcc, suffix)) {
                formats.push(format!("{}{}", fmt.fourcc, suffix));
            }
        }
    }

    if formats.is_empty() {
        None
    } else {
        Some(formats)
    }
}

/// Collect format summary (non-verbose mode)
fn collect_formats_summary(device: &Device) -> Option<Vec<String>> {
    let mut formats = Vec::new();

    if device.is_encoder() {
        // Show compressed output formats
        for fmt in device.capture_formats() {
            if fmt.compressed {
                formats.push(format!("{}", fmt.fourcc));
            }
        }
    } else if device.is_decoder() {
        // Show compressed input formats
        for fmt in device.output_formats() {
            if fmt.compressed {
                formats.push(format!("{}", fmt.fourcc));
            }
        }
    } else {
        // Camera/ISP - show first few capture formats
        let cap_fmts: Vec<String> = device
            .capture_formats()
            .iter()
            .filter(|f| !f.compressed)
            .take(5)
            .map(|f| format!("{}", f.fourcc))
            .collect();

        if !cap_fmts.is_empty() {
            let remaining = device.capture_formats().len().saturating_sub(5);
            if remaining > 0 {
                formats.extend(cap_fmts);
                formats.push(format!("+{} more", remaining));
            } else {
                formats.extend(cap_fmts);
            }
        }
    }

    if formats.is_empty() {
        None
    } else {
        Some(formats)
    }
}

/// Collect memory capability strings
fn collect_memory_caps(device: &Device) -> Vec<String> {
    let mut caps = Vec::new();

    // Use capture memory for cameras, both for M2M
    let mem = device.capture_memory();
    if mem.mmap {
        caps.push("MMAP".to_string());
    }
    if mem.userptr {
        caps.push("USERPTR".to_string());
    }
    if mem.dmabuf {
        caps.push("DMABUF".to_string());
    }

    caps
}

fn print_text_output(
    output: &DevicesOutput,
    show_cameras: bool,
    show_encoders: bool,
    show_decoders: bool,
    show_converters: bool,
    verbose: bool,
) {
    println!(
        "V4L2 Devices ({} devices, {} hardware units)\n",
        output.summary.total_devices, output.summary.hardware_units
    );

    if show_cameras && !output.cameras.is_empty() {
        println!("Cameras ({}):", output.cameras.len());
        for group in &output.cameras {
            print_device_group(group, verbose);
        }
        println!();
    }

    if show_encoders && !output.encoders.is_empty() {
        println!("Encoders ({}):", output.encoders.len());
        for group in &output.encoders {
            print_device_group(group, verbose);
        }
        println!();
    }

    if show_decoders && !output.decoders.is_empty() {
        println!("Decoders ({}):", output.decoders.len());
        for group in &output.decoders {
            print_device_group(group, verbose);
        }
        println!();
    }

    if show_converters && !output.converters.is_empty() {
        println!("Converters/ISP ({}):", output.converters.len());
        for group in &output.converters {
            print_device_group(group, verbose);
        }
        println!();
    }

    // Print recommendation
    if show_encoders && !output.encoders.is_empty() {
        if let Some(enc) = output.encoders.iter().find(|g| {
            g.formats
                .as_ref()
                .map(|f| f.iter().any(|s| s.contains("H264") || s.contains("HEVC")))
                .unwrap_or(false)
        }) {
            println!(
                "Recommended encoder: {} ({})",
                enc.devices.first().map(|d| d.path.as_str()).unwrap_or("?"),
                enc.name
            );
        }
    }

    if show_decoders && !output.decoders.is_empty() {
        if let Some(dec) = output.decoders.iter().find(|g| {
            g.formats
                .as_ref()
                .map(|f| f.iter().any(|s| s.contains("H264") || s.contains("HEVC")))
                .unwrap_or(false)
        }) {
            println!(
                "Recommended decoder: {} ({})",
                dec.devices.first().map(|d| d.path.as_str()).unwrap_or("?"),
                dec.name
            );
        }
    }
}

fn print_device_group(group: &DeviceGroup, verbose: bool) {
    // Device path(s)
    if group.devices.len() == 1 {
        println!("  {}: {}", group.devices[0].path, group.name);
    } else {
        // Multiple devices on same bus - show range or list
        let paths: Vec<&str> = group.devices.iter().map(|d| d.path.as_str()).collect();
        let path_summary = summarize_paths(&paths);
        println!(
            "  {}: {} ({} nodes)",
            path_summary,
            group.name,
            group.devices.len()
        );
    }

    println!("    Driver: {}", group.driver);

    if verbose {
        println!("    Bus: {}", group.bus);
    }

    if let Some(ref formats) = group.formats {
        println!("    Formats: {}", formats.join(", "));
    }

    if !group.memory.is_empty() {
        println!("    Memory: {}", group.memory.join(", "));
    }
}

/// Summarize device paths like "/dev/video0-4" or "/dev/video11,12,14"
fn summarize_paths(paths: &[&str]) -> String {
    if paths.is_empty() {
        return String::new();
    }
    if paths.len() == 1 {
        return paths[0].to_string();
    }

    // Extract video numbers
    let mut nums: Vec<u32> = paths
        .iter()
        .filter_map(|p| {
            p.strip_prefix("/dev/video")
                .and_then(|s| s.parse::<u32>().ok())
        })
        .collect();
    nums.sort_unstable();

    if nums.is_empty() {
        return paths.join(", ");
    }

    // Check if consecutive
    let is_consecutive = nums.windows(2).all(|w| w[1] == w[0] + 1);

    if is_consecutive && nums.len() > 2 {
        format!(
            "/dev/video{}-{}",
            nums.first().unwrap(),
            nums.last().unwrap()
        )
    } else if nums.len() <= 4 {
        format!(
            "/dev/video{{{}}}",
            nums.iter()
                .map(|n| n.to_string())
                .collect::<Vec<_>>()
                .join(",")
        )
    } else {
        format!(
            "/dev/video{{{},..}} ({} devices)",
            nums.first().unwrap(),
            nums.len()
        )
    }
}
