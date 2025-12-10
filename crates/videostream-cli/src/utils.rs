// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use signal_hook::consts::SIGINT;
use signal_hook::flag;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

/// Parse resolution string in format "WxH" or "W*H"
///
/// # Examples
/// ```
/// use videostream_cli::utils::parse_resolution;
/// assert_eq!(parse_resolution("1920x1080").unwrap(), (1920, 1080));
/// assert_eq!(parse_resolution("1280x720").unwrap(), (1280, 720));
/// ```
pub fn parse_resolution(s: &str) -> Result<(i32, i32), CliError> {
    // Try 'x' separator first
    if let Some((width_str, height_str)) = s.split_once('x') {
        let width = width_str
            .parse::<i32>()
            .map_err(|_| CliError::InvalidArgs(format!("Invalid width in resolution: {}", s)))?;
        let height = height_str
            .parse::<i32>()
            .map_err(|_| CliError::InvalidArgs(format!("Invalid height in resolution: {}", s)))?;

        if width <= 0 || height <= 0 {
            return Err(CliError::InvalidArgs(format!(
                "Resolution dimensions must be positive: {}",
                s
            )));
        }

        return Ok((width, height));
    }

    // Try '*' separator as alternative
    if let Some((width_str, height_str)) = s.split_once('*') {
        let width = width_str
            .parse::<i32>()
            .map_err(|_| CliError::InvalidArgs(format!("Invalid width in resolution: {}", s)))?;
        let height = height_str
            .parse::<i32>()
            .map_err(|_| CliError::InvalidArgs(format!("Invalid height in resolution: {}", s)))?;

        if width <= 0 || height <= 0 {
            return Err(CliError::InvalidArgs(format!(
                "Resolution dimensions must be positive: {}",
                s
            )));
        }

        return Ok((width, height));
    }

    Err(CliError::InvalidArgs(format!(
        "Invalid resolution format (expected WxH or W*H): {}",
        s
    )))
}

/// Convert FOURCC string to u32
///
/// # Examples
/// ```
/// use videostream_cli::utils::fourcc_from_str;
/// assert_eq!(fourcc_from_str("YUYV").unwrap(), 0x56595559);
/// assert_eq!(fourcc_from_str("NV12").unwrap(), 0x3231564e);
/// ```
pub fn fourcc_from_str(s: &str) -> Result<u32, CliError> {
    if s.len() != 4 {
        return Err(CliError::InvalidArgs(format!(
            "FOURCC must be exactly 4 characters: {}",
            s
        )));
    }

    let bytes = s.as_bytes();
    Ok(u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
}

/// Convert u32 to FOURCC string
#[allow(dead_code)]
pub fn fourcc_to_str(fourcc: u32) -> String {
    let bytes = fourcc.to_le_bytes();
    String::from_utf8_lossy(&bytes).to_string()
}

/// Install signal handler for graceful shutdown on Ctrl+C
///
/// Returns an Arc<AtomicBool> that will be set to true when SIGINT is received.
/// Check this flag periodically in your main loop to exit gracefully.
///
/// # Example
/// ```no_run
/// use videostream_cli::utils::install_signal_handler;
/// use std::sync::atomic::Ordering;
///
/// let term = install_signal_handler().expect("Failed to install signal handler");
///
/// while !term.load(Ordering::Relaxed) {
///     // Do work...
/// }
/// println!("Received Ctrl+C, exiting gracefully");
/// ```
pub fn install_signal_handler() -> Result<Arc<AtomicBool>, CliError> {
    let term = Arc::new(AtomicBool::new(false));

    flag::register(SIGINT, Arc::clone(&term))
        .map_err(|e| CliError::General(format!("Failed to register signal handler: {}", e)))?;

    log::debug!("Installed SIGINT handler");
    Ok(term)
}

/// Parse bitrate from string (supports kbps suffix)
///
/// # Examples
/// ```
/// use videostream_cli::utils::parse_bitrate;
/// assert_eq!(parse_bitrate("25000").unwrap(), 25000);
/// assert_eq!(parse_bitrate("5000kbps").unwrap(), 5000);
/// assert_eq!(parse_bitrate("25Mbps").unwrap(), 25000);
/// ```
pub fn parse_bitrate(s: &str) -> Result<u32, CliError> {
    let s_lower = s.to_lowercase();

    // Try to parse with Mbps suffix
    if let Some(value_str) = s_lower.strip_suffix("mbps") {
        let value = value_str
            .trim()
            .parse::<u32>()
            .map_err(|_| CliError::InvalidArgs(format!("Invalid bitrate: {}", s)))?;
        return Ok(value * 1000);
    }

    // Try to parse with kbps suffix
    if let Some(value_str) = s_lower.strip_suffix("kbps") {
        let value = value_str
            .trim()
            .parse::<u32>()
            .map_err(|_| CliError::InvalidArgs(format!("Invalid bitrate: {}", s)))?;
        return Ok(value);
    }

    // Parse as plain number (assume kbps)
    s.parse::<u32>()
        .map_err(|_| CliError::InvalidArgs(format!("Invalid bitrate: {}", s)))
}

/// NAL unit types for H.264/H.265
const NAL_TYPE_SPS_H264: u8 = 7;
const NAL_TYPE_PPS_H264: u8 = 8;
#[allow(dead_code)]
const NAL_TYPE_SPS_H265: u8 = 33;
#[allow(dead_code)]
const NAL_TYPE_PPS_H265: u8 = 34;

/// Extracted parameter sets from H.264/H.265 stream
#[derive(Debug, Clone)]
pub struct ParameterSets {
    pub sps: Vec<u8>,
    pub pps: Vec<u8>,
}

/// Extract SPS and PPS from H.264 Annex-B bitstream
///
/// Parses NAL units from the stream and extracts Sequence Parameter Set (SPS)
/// and Picture Parameter Set (PPS) which are needed for MP4 container creation.
pub fn extract_parameter_sets_h264(data: &[u8]) -> Result<ParameterSets, CliError> {
    let mut sps = Vec::new();
    let mut pps = Vec::new();

    let nal_units = parse_nal_units(data)?;

    for nal in nal_units {
        if nal.is_empty() {
            continue;
        }

        let nal_type = nal[0] & 0x1F; // Lower 5 bits = NAL unit type

        match nal_type {
            NAL_TYPE_SPS_H264 => {
                sps = nal.to_vec();
                log::debug!("Found H.264 SPS: {} bytes", sps.len());
            }
            NAL_TYPE_PPS_H264 => {
                pps = nal.to_vec();
                log::debug!("Found H.264 PPS: {} bytes", pps.len());
            }
            _ => {}
        }

        // Stop once we have both
        if !sps.is_empty() && !pps.is_empty() {
            break;
        }
    }

    if sps.is_empty() || pps.is_empty() {
        return Err(CliError::General(
            "Failed to find SPS/PPS in H.264 stream".to_string(),
        ));
    }

    Ok(ParameterSets { sps, pps })
}

/// Extract SPS and PPS from H.265/HEVC Annex-B bitstream
#[allow(dead_code)]
pub fn extract_parameter_sets_h265(data: &[u8]) -> Result<ParameterSets, CliError> {
    let mut sps = Vec::new();
    let mut pps = Vec::new();

    let nal_units = parse_nal_units(data)?;

    for nal in nal_units {
        if nal.is_empty() {
            continue;
        }

        let nal_type = (nal[0] >> 1) & 0x3F; // H.265 NAL type is bits 1-6

        match nal_type {
            NAL_TYPE_SPS_H265 => {
                sps = nal.to_vec();
                log::debug!("Found H.265 SPS: {} bytes", sps.len());
            }
            NAL_TYPE_PPS_H265 => {
                pps = nal.to_vec();
                log::debug!("Found H.265 PPS: {} bytes", pps.len());
            }
            _ => {}
        }

        // Stop once we have both
        if !sps.is_empty() && !pps.is_empty() {
            break;
        }
    }

    if sps.is_empty() || pps.is_empty() {
        return Err(CliError::General(
            "Failed to find SPS/PPS in H.265 stream".to_string(),
        ));
    }

    Ok(ParameterSets { sps, pps })
}

/// Parse NAL units from Annex-B format bitstream
///
/// Annex-B format uses start codes:
/// - 0x00 0x00 0x00 0x01 (4-byte)
/// - 0x00 0x00 0x01 (3-byte)
///
/// Returns NAL units without start codes
fn parse_nal_units(data: &[u8]) -> Result<Vec<&[u8]>, CliError> {
    let mut nal_units = Vec::new();
    let mut i = 0;

    while i < data.len() {
        // Find start code
        let start_code_len = if i + 3 < data.len()
            && data[i] == 0
            && data[i + 1] == 0
            && data[i + 2] == 0
            && data[i + 3] == 1
        {
            4
        } else if i + 2 < data.len() && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 {
            3
        } else {
            i += 1;
            continue;
        };

        // Found start code, skip it
        i += start_code_len;
        let nal_start = i;

        // Find next start code
        while i < data.len() {
            if i + 2 < data.len() && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 {
                break;
            }
            if i + 3 < data.len()
                && data[i] == 0
                && data[i + 1] == 0
                && data[i + 2] == 0
                && data[i + 3] == 1
            {
                break;
            }
            i += 1;
        }

        // Extract NAL unit (without start code)
        if nal_start < i {
            nal_units.push(&data[nal_start..i]);
        }
    }

    Ok(nal_units)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_resolution_valid() {
        assert_eq!(parse_resolution("1920x1080").unwrap(), (1920, 1080));
        assert_eq!(parse_resolution("1280x720").unwrap(), (1280, 720));
        assert_eq!(parse_resolution("640x480").unwrap(), (640, 480));
        assert_eq!(parse_resolution("3840x2160").unwrap(), (3840, 2160));

        // Test alternative separator
        assert_eq!(parse_resolution("1920*1080").unwrap(), (1920, 1080));
    }

    #[test]
    fn test_parse_resolution_invalid() {
        assert!(parse_resolution("1920").is_err());
        assert!(parse_resolution("1920x").is_err());
        assert!(parse_resolution("x1080").is_err());
        assert!(parse_resolution("1920x1080x60").is_err());
        assert!(parse_resolution("widthxheight").is_err());
        assert!(parse_resolution("0x0").is_err());
        assert!(parse_resolution("-1920x1080").is_err());
    }

    #[test]
    fn test_fourcc_conversion() {
        // Test conversion to u32
        assert_eq!(fourcc_from_str("YUYV").unwrap(), 0x56595559);
        assert_eq!(fourcc_from_str("NV12").unwrap(), 0x3231564e);
        assert_eq!(fourcc_from_str("H264").unwrap(), 0x34363248);
        assert_eq!(fourcc_from_str("HEVC").unwrap(), 0x43564548);

        // Test round-trip
        let fourcc = fourcc_from_str("MJPG").unwrap();
        assert_eq!(fourcc_to_str(fourcc), "MJPG");
    }

    #[test]
    fn test_fourcc_invalid() {
        assert!(fourcc_from_str("YUY").is_err()); // Too short
        assert!(fourcc_from_str("YUYVV").is_err()); // Too long
        assert!(fourcc_from_str("").is_err()); // Empty
    }

    #[test]
    fn test_parse_bitrate() {
        assert_eq!(parse_bitrate("25000").unwrap(), 25000);
        assert_eq!(parse_bitrate("5000kbps").unwrap(), 5000);
        assert_eq!(parse_bitrate("5000KBPS").unwrap(), 5000);
        assert_eq!(parse_bitrate("25Mbps").unwrap(), 25000);
        assert_eq!(parse_bitrate("25mbps").unwrap(), 25000);
        assert_eq!(parse_bitrate("100MBPS").unwrap(), 100000);
    }

    #[test]
    fn test_parse_bitrate_invalid() {
        assert!(parse_bitrate("abc").is_err());
        assert!(parse_bitrate("-1000").is_err());
        assert!(parse_bitrate("25 Mbps extra").is_err());
    }
}
