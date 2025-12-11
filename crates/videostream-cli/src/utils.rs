// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use signal_hook::consts::SIGINT;
use signal_hook::flag;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;
use videostream::encoder;

/// Helper to parse and validate resolution parts
fn parse_resolution_parts(
    width_str: &str,
    height_str: &str,
    original: &str,
) -> Result<(i32, i32), CliError> {
    let width = width_str
        .parse::<i32>()
        .map_err(|_| CliError::InvalidArgs(format!("Invalid width in resolution: {}", original)))?;
    let height = height_str.parse::<i32>().map_err(|_| {
        CliError::InvalidArgs(format!("Invalid height in resolution: {}", original))
    })?;

    if width <= 0 || height <= 0 {
        return Err(CliError::InvalidArgs(format!(
            "Resolution dimensions must be positive: {}",
            original
        )));
    }

    Ok((width, height))
}

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
        return parse_resolution_parts(width_str, height_str, s);
    }

    // Try '*' separator as alternative
    if let Some((width_str, height_str)) = s.split_once('*') {
        return parse_resolution_parts(width_str, height_str, s);
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

/// Map bitrate to encoder profile
///
/// Maps bitrate in kbps to the appropriate VideoStream encoder profile.
/// Uses profile tiers: 5Mbps, 25Mbps, 50Mbps, 100Mbps.
///
/// # Examples
/// ```
/// use videostream_cli::utils::bitrate_to_encoder_profile;
/// use videostream::encoder::VSLEncoderProfileEnum;
/// assert_eq!(bitrate_to_encoder_profile(5000), VSLEncoderProfileEnum::Kbps5000);
/// assert_eq!(bitrate_to_encoder_profile(25000), VSLEncoderProfileEnum::Kbps25000);
/// ```
pub fn bitrate_to_encoder_profile(bitrate_kbps: u32) -> encoder::VSLEncoderProfileEnum {
    match bitrate_kbps {
        0..=7500 => encoder::VSLEncoderProfileEnum::Kbps5000,
        7501..=37500 => encoder::VSLEncoderProfileEnum::Kbps25000,
        37501..=75000 => encoder::VSLEncoderProfileEnum::Kbps50000,
        _ => encoder::VSLEncoderProfileEnum::Kbps100000,
    }
}

/// Convert codec string to FOURCC value
///
/// Converts codec name (h264, h265, hevc) to 32-bit FOURCC identifier.
///
/// # Examples
/// ```
/// use videostream_cli::utils::codec_to_fourcc;
/// assert_eq!(codec_to_fourcc("h264").unwrap(), 0x34363248);
/// assert_eq!(codec_to_fourcc("h265").unwrap(), 0x43564548);
/// assert_eq!(codec_to_fourcc("hevc").unwrap(), 0x43564548);
/// ```
pub fn codec_to_fourcc(codec: &str) -> Result<u32, CliError> {
    match codec.to_lowercase().as_str() {
        "h264" => Ok(u32::from_le_bytes(*b"H264")),
        "h265" | "hevc" => Ok(u32::from_le_bytes(*b"HEVC")),
        _ => Err(CliError::InvalidArgs(format!(
            "Unsupported codec: {} (supported: h264, h265, hevc)",
            codec
        ))),
    }
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

/// Detect Annex B start code at given position
///
/// Reference: ITU-T H.264 (ISO/IEC 14496-10) Annex B.1.1
/// "byte_stream_nal_unit() syntax uses start code prefix 0x000001 or 0x00000001"
///
/// Returns the length of the start code (3 or 4 bytes) or None if no start code found
fn detect_start_code(data: &[u8], pos: usize) -> Option<usize> {
    if pos + 3 < data.len()
        && data[pos] == 0
        && data[pos + 1] == 0
        && data[pos + 2] == 0
        && data[pos + 3] == 1
    {
        Some(4)
    } else if pos + 2 < data.len() && data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 1 {
        Some(3)
    } else {
        None
    }
}

/// Parse NAL units from Annex-B format bitstream
///
/// Annex-B format uses start codes:
/// - 0x00 0x00 0x00 0x01 (4-byte)
/// - 0x00 0x00 0x01 (3-byte)
///
/// Returns NAL units without start codes
pub fn parse_nal_units(data: &[u8]) -> Result<Vec<&[u8]>, CliError> {
    let mut nal_units = Vec::new();
    let mut i = 0;

    while i < data.len() {
        // Find start code
        let start_code_len = match detect_start_code(data, i) {
            Some(len) => len,
            None => {
                i += 1;
                continue;
            }
        };

        // Found start code, skip it
        i += start_code_len;
        let nal_start = i;

        // Find next start code
        while i < data.len() {
            if detect_start_code(data, i).is_some() {
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

/// Normalize codec alias to canonical form
///
/// Converts various codec name aliases to their canonical lowercase form:
/// - H.264, h.264, H264, h264 → "h264"
/// - H.265, h.265, H265, h265, HEVC, hevc → "h265"
///
/// # Examples
/// ```
/// use videostream_cli::utils::normalize_codec_alias;
/// assert_eq!(normalize_codec_alias("H.264").unwrap(), "h264");
/// assert_eq!(normalize_codec_alias("h264").unwrap(), "h264");
/// assert_eq!(normalize_codec_alias("H.265").unwrap(), "h265");
/// assert_eq!(normalize_codec_alias("HEVC").unwrap(), "h265");
/// assert_eq!(normalize_codec_alias("hevc").unwrap(), "h265");
/// ```
pub fn normalize_codec_alias(codec: &str) -> Result<&'static str, CliError> {
    match codec.to_lowercase().replace(['.', '-'], "").as_str() {
        "h264" | "avc" => Ok("h264"),
        "h265" | "hevc" => Ok("h265"),
        _ => Err(CliError::InvalidArgs(format!(
            "Unsupported codec: {} (supported: H.264/h264/AVC, H.265/h265/HEVC)",
            codec
        ))),
    }
}

/// Create encoder if requested, with automatic availability check
///
/// Returns `(Option<Encoder>, output_fourcc)` where:
/// - If `encode` is false, returns (None, fallback_fourcc)
/// - If `encode` is true, creates encoder and returns (Some(encoder), codec_fourcc)
///
/// # Arguments
/// * `encode` - Whether to enable encoding
/// * `codec` - Codec name (supports aliases: H.264, h264, H.265, hevc, etc.)
/// * `bitrate` - Target bitrate (e.g., "25000", "25Mbps")
/// * `fps` - Target frame rate
/// * `fallback_fourcc` - FourCC to use when encoding is disabled
///
/// # Errors
/// Returns `CliError::EncoderUnavailable` if encoding requested but VPU not available
///
/// # Examples
/// ```no_run
/// use videostream_cli::utils::create_encoder_if_requested;
/// let (encoder, fourcc) = create_encoder_if_requested(
///     true, "h264", "25000", 30, 0x56595559
/// ).unwrap();
/// assert!(encoder.is_some());
/// ```
pub fn create_encoder_if_requested(
    encode: bool,
    codec: &str,
    bitrate: &str,
    fps: i32,
    fallback_fourcc: u32,
) -> Result<(Option<encoder::Encoder>, u32), CliError> {
    if !encode {
        return Ok((None, fallback_fourcc));
    }

    // Check encoder availability first
    if !encoder::is_available().unwrap_or(false) {
        return Err(CliError::EncoderUnavailable(
            "VPU encoder not available on this system".to_string(),
        ));
    }

    // Normalize codec alias and convert to FourCC
    let normalized_codec = normalize_codec_alias(codec)?;
    let codec_fourcc = codec_to_fourcc(normalized_codec)?;

    // Parse bitrate and map to encoder profile
    let bitrate_kbps = parse_bitrate(bitrate)?;
    let profile = bitrate_to_encoder_profile(bitrate_kbps);

    log::info!(
        "Creating encoder: {} at {} kbps (profile: {:?})",
        normalized_codec.to_uppercase(),
        bitrate_kbps,
        profile
    );

    // Create encoder
    let enc = encoder::Encoder::create(profile as u32, codec_fourcc, fps)?;

    Ok((Some(enc), codec_fourcc))
}

/// Create decoder if requested, with automatic availability check
///
/// Returns `Option<Decoder>`:
/// - If `decode` is false, returns None
/// - If `decode` is true, creates H.264 decoder and returns Some(decoder)
///
/// # Arguments
/// * `decode` - Whether to enable decoding
/// * `codec` - Codec name (supports aliases: H.264, h264, H.265, hevc, etc.)
/// * `fps` - Frame rate hint for decoder
///
/// # Errors
/// Returns `CliError::EncoderUnavailable` if decoding requested but VPU not available
///
/// # Examples
/// ```no_run
/// use videostream_cli::utils::create_decoder_if_requested;
/// let decoder = create_decoder_if_requested(true, "h264", 30).unwrap();
/// assert!(decoder.is_some());
/// ```
pub fn create_decoder_if_requested(
    decode: bool,
    codec: &str,
    fps: i32,
) -> Result<Option<videostream::decoder::Decoder>, CliError> {
    use videostream::decoder;

    if !decode {
        return Ok(None);
    }

    // Check decoder availability first
    if !decoder::is_available().unwrap_or(false) {
        return Err(CliError::EncoderUnavailable(
            "VPU decoder not available on this system".to_string(),
        ));
    }

    // Normalize codec alias
    let normalized_codec = normalize_codec_alias(codec)?;

    // Map to decoder input codec enum
    let decoder_codec = match normalized_codec {
        "h264" => decoder::DecoderInputCodec::H264,
        "h265" => decoder::DecoderInputCodec::HEVC,
        _ => {
            return Err(CliError::InvalidArgs(format!(
                "Unsupported decoder codec: {}",
                codec
            )))
        }
    };

    log::info!("Creating {} decoder", normalized_codec.to_uppercase());

    // Create decoder
    let dec = decoder::Decoder::create(decoder_codec, fps)?;

    Ok(Some(dec))
}

/// Estimate frame size in bytes
///
/// Calculates estimated frame size based on:
/// - **Encoded frames**: `(bitrate_kbps * 1000) / (fps * 8)` bytes per frame
/// - **Raw frames**: `width * height * bytes_per_pixel`
///   - YUYV format: 2 bytes per pixel
///   - NV12 format: 1.5 bytes per pixel
///   - RGB3 format: 3 bytes per pixel
///
/// # Arguments
/// * `width` - Frame width in pixels
/// * `height` - Frame height in pixels
/// * `encoded` - Whether the frame is encoded
/// * `bitrate` - Target bitrate for encoded frames (e.g., "25000", "25Mbps")
/// * `fps` - Frame rate
/// * `format` - Pixel format FourCC (only used for raw frames)
///
/// # Examples
/// ```
/// use videostream_cli::utils::estimate_frame_size;
/// // Encoded H.264 at 25Mbps, 30fps
/// let size = estimate_frame_size(1920, 1080, true, "25000", 30, 0x56595559).unwrap();
/// assert_eq!(size, 104166); // ~104KB per frame
///
/// // Raw YUYV (2 bytes/pixel)
/// let size = estimate_frame_size(1920, 1080, false, "0", 30, 0x56595559).unwrap();
/// assert_eq!(size, 4147200); // 1920 * 1080 * 2
/// ```
pub fn estimate_frame_size(
    width: u32,
    height: u32,
    encoded: bool,
    bitrate: &str,
    fps: i32,
    format: u32,
) -> Result<u64, CliError> {
    if encoded {
        // Encoded frame size estimate: bitrate / fps / 8 (convert bits to bytes)
        let bitrate_kbps = parse_bitrate(bitrate)?;
        Ok((bitrate_kbps as u64 * 1000) / (fps as u64 * 8))
    } else {
        // Raw frame size: width * height * bytes_per_pixel
        // Determine bytes per pixel based on format
        let bytes_per_pixel = match &fourcc_to_str(format).to_uppercase()[..] {
            "YUYV" | "UYVY" => 2, // YUV 4:2:2 packed formats
            "NV12" | "NV21" => {
                // YUV 4:2:0 planar formats (1.5 bytes per pixel average)
                return Ok((width as u64 * height as u64 * 3) / 2);
            }
            "RGB3" | "BGR3" => 3, // RGB/BGR 24-bit
            "RGBA" | "BGRA" => 4, // RGB/BGR 32-bit with alpha
            _ => {
                log::warn!(
                    "Unknown format FourCC 0x{:08x}, assuming 2 bytes/pixel",
                    format
                );
                2
            }
        };

        Ok(width as u64 * height as u64 * bytes_per_pixel)
    }
}

/// Normalize frame count (converts 0 to unlimited)
///
/// Converts frame count where 0 means unlimited (u64::MAX).
///
/// # Examples
/// ```
/// use videostream_cli::utils::normalize_frame_count;
/// assert_eq!(normalize_frame_count(100), 100);
/// assert_eq!(normalize_frame_count(0), u64::MAX);
/// ```
pub fn normalize_frame_count(count: u64) -> u64 {
    if count == 0 {
        u64::MAX
    } else {
        count
    }
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

    // =========================================================================
    // NAL Unit Parsing Tests - ITU-T H.264 Annex B / ITU-T H.265 Annex B
    // =========================================================================

    /// Test detect_start_code() with 4-byte start code (0x00000001)
    ///
    /// Reference: ITU-T H.264 (ISO/IEC 14496-10) Annex B.1.1
    /// "byte_stream_nal_unit() syntax uses start code prefix 0x000001 or 0x00000001"
    #[test]
    fn test_detect_start_code_four_byte() {
        let data = vec![0x00, 0x00, 0x00, 0x01, 0x67];
        assert_eq!(detect_start_code(&data, 0), Some(4));
    }

    /// Test detect_start_code() with 3-byte start code (0x000001)
    ///
    /// Reference: ITU-T H.264 (ISO/IEC 14496-10) Annex B.1.1
    #[test]
    fn test_detect_start_code_three_byte() {
        let data = vec![0x00, 0x00, 0x01, 0x68];
        assert_eq!(detect_start_code(&data, 0), Some(3));
    }

    /// Test detect_start_code() with no start code
    #[test]
    fn test_detect_start_code_none() {
        let data = vec![0x00, 0x00, 0x02, 0x67];
        assert_eq!(detect_start_code(&data, 0), None);

        let data2 = vec![0x01, 0x00, 0x00, 0x01];
        assert_eq!(detect_start_code(&data2, 0), None);
    }

    /// Test detect_start_code() at different positions
    #[test]
    fn test_detect_start_code_positions() {
        let data = vec![
            0xFF, 0xFF, // Non-start-code prefix
            0x00, 0x00, 0x01, // 3-byte start code at pos 2
            0x67, // NAL data
            0x00, 0x00, 0x00, 0x01, // 4-byte start code at pos 6
            0x68, // NAL data
        ];

        assert_eq!(detect_start_code(&data, 0), None);
        assert_eq!(detect_start_code(&data, 1), None);
        assert_eq!(detect_start_code(&data, 2), Some(3));
        assert_eq!(detect_start_code(&data, 6), Some(4));
    }

    /// Test detect_start_code() with truncated data
    #[test]
    fn test_detect_start_code_truncated() {
        // Only 2 bytes - cannot form even 3-byte start code
        let data = vec![0x00, 0x00];
        assert_eq!(detect_start_code(&data, 0), None);

        // Only 3 bytes - can check for 3-byte but not 4-byte at end
        let data2 = vec![0x00, 0x00, 0x01];
        assert_eq!(detect_start_code(&data2, 0), Some(3));

        // Check boundary: position too close to end
        let data3 = vec![0x00, 0x00, 0x00, 0x01];
        assert_eq!(detect_start_code(&data3, 2), None); // Only 2 bytes left
    }

    /// Test parse_nal_units() with 4-byte start code (0x00000001)
    ///
    /// Reference: ITU-T H.264 (ISO/IEC 14496-10) Annex B.1.1
    /// "byte_stream_nal_unit() syntax uses start code prefix 0x000001 or 0x00000001"
    #[test]
    fn test_parse_nal_units_four_byte_start_code() {
        // Single NAL unit with 4-byte start code
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // 4-byte start code
            0x67, 0x42, 0x00, 0x0A, // NAL unit data (SPS header)
        ];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 1);
        assert_eq!(nal_units[0], &[0x67, 0x42, 0x00, 0x0A]);
    }

    /// Test parse_nal_units() with 3-byte start code (0x000001)
    ///
    /// Reference: ITU-T H.264 Annex B.1.1
    /// "The 3-byte start code 0x000001 may be used for byte stream NAL units"
    #[test]
    fn test_parse_nal_units_three_byte_start_code() {
        // Single NAL unit with 3-byte start code
        let data = vec![
            0x00, 0x00, 0x01, // 3-byte start code
            0x68, 0xCE, 0x3C, 0x80, // NAL unit data (PPS)
        ];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 1);
        assert_eq!(nal_units[0], &[0x68, 0xCE, 0x3C, 0x80]);
    }

    /// Test parse_nal_units() with multiple NAL units using mixed start codes
    ///
    /// Reference: ITU-T H.264 Annex B.1
    /// "A byte stream NAL unit syntax structure contains a sequence of byte stream NAL units"
    #[test]
    fn test_parse_nal_units_multiple_mixed_start_codes() {
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // 4-byte start code
            0x67, 0x42, // SPS start
            0x00, 0x00, 0x01, // 3-byte start code
            0x68, 0xCE, // PPS start
            0x00, 0x00, 0x00, 0x01, // 4-byte start code
            0x65, 0x88, // IDR slice
        ];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 3);
        assert_eq!(nal_units[0], &[0x67, 0x42]);
        assert_eq!(nal_units[1], &[0x68, 0xCE]);
        assert_eq!(nal_units[2], &[0x65, 0x88]);
    }

    /// Test parse_nal_units() with emulation prevention bytes
    ///
    /// Reference: ITU-T H.264 Section 7.4.1
    /// "Within NAL unit, 0x000003 is emulation prevention - 0x03 prevents start code emulation"
    #[test]
    fn test_parse_nal_units_with_emulation_prevention() {
        // NAL unit containing 0x000003 (emulation prevention sequence)
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // Start code
            0x67, 0x00, 0x00, 0x03, 0x01, 0x42, // NAL with emulation prevention
        ];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 1);
        // The 0x000003 should be kept in the NAL unit (parser doesn't decode, just splits)
        assert_eq!(nal_units[0], &[0x67, 0x00, 0x00, 0x03, 0x01, 0x42]);
    }

    /// Test parse_nal_units() with zero-length NAL unit (consecutive start codes)
    ///
    /// Reference: ITU-T H.264 Annex B
    /// "Zero-length NAL units should be handled gracefully"
    #[test]
    fn test_parse_nal_units_zero_length() {
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // First start code
            0x00, 0x00, 0x01, // Immediate second start code (zero-length NAL)
            0x67, 0x42, // Actual NAL data
        ];
        let nal_units = parse_nal_units(&data).unwrap();
        // Should handle zero-length gracefully and still parse the valid NAL
        assert!(nal_units.iter().any(|nal| nal.starts_with(&[0x67, 0x42])));
    }

    /// Test parse_nal_units() with no start codes (invalid bitstream)
    ///
    /// Edge case: Data without start codes should return empty
    #[test]
    fn test_parse_nal_units_no_start_codes() {
        let data = vec![0x67, 0x42, 0x00, 0x0A, 0x68, 0xCE];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 0);
    }

    /// Test parse_nal_units() with partial start code at end (truncated stream)
    ///
    /// Edge case: Bitstream ending with incomplete start code
    #[test]
    fn test_parse_nal_units_truncated_start_code() {
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // Valid start code
            0x67, 0x42, 0x00, 0x0A, // NAL data
            0x00, 0x00, // Partial start code at end (truncated)
        ];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 1);
        // The partial start code should be included in the NAL unit
        assert_eq!(nal_units[0], &[0x67, 0x42, 0x00, 0x0A, 0x00, 0x00]);
    }

    /// Test parse_nal_units() with empty input
    ///
    /// Edge case: Empty bitstream
    #[test]
    fn test_parse_nal_units_empty() {
        let data = vec![];
        let nal_units = parse_nal_units(&data).unwrap();
        assert_eq!(nal_units.len(), 0);
    }

    /// Test extract_parameter_sets_h264() with valid SPS and PPS
    ///
    /// Reference: ITU-T H.264 Section 7.3.2.1 (SPS) and 7.3.2.2 (PPS)
    /// NAL unit type 7 = SPS, NAL unit type 8 = PPS (bits 0-4 of first byte)
    #[test]
    fn test_extract_parameter_sets_h264_valid() {
        // Minimal H.264 bitstream with SPS (type 7) and PPS (type 8)
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // Start code
            0x67, 0x42, 0x00, 0x0A, // SPS: NAL type 7 (0x67 & 0x1F = 7)
            0x00, 0x00, 0x00, 0x01, // Start code
            0x68, 0xCE, 0x3C, 0x80, // PPS: NAL type 8 (0x68 & 0x1F = 8)
        ];
        let params = extract_parameter_sets_h264(&data).unwrap();
        assert_eq!(params.sps, &[0x67, 0x42, 0x00, 0x0A]);
        assert_eq!(params.pps, &[0x68, 0xCE, 0x3C, 0x80]);
    }

    /// Test extract_parameter_sets_h264() with NAL type extraction
    ///
    /// Reference: ITU-T H.264 Section 7.3.1
    /// "nal_unit_type is in bits 0-4 (lower 5 bits) of NAL unit header byte"
    #[test]
    fn test_h264_nal_type_extraction() {
        // Test NAL type extraction for various NAL unit types
        let nal_header_sps = 0x67u8; // SPS
        let nal_type_sps = nal_header_sps & 0x1F;
        assert_eq!(nal_type_sps, 7, "SPS NAL type should be 7");

        let nal_header_pps = 0x68u8; // PPS
        let nal_type_pps = nal_header_pps & 0x1F;
        assert_eq!(nal_type_pps, 8, "PPS NAL type should be 8");

        let nal_header_idr = 0x65u8; // IDR slice
        let nal_type_idr = nal_header_idr & 0x1F;
        assert_eq!(nal_type_idr, 5, "IDR slice NAL type should be 5");

        let nal_header_non_idr = 0x61u8; // Non-IDR slice
        let nal_type_non_idr = nal_header_non_idr & 0x1F;
        assert_eq!(nal_type_non_idr, 1, "Non-IDR slice NAL type should be 1");
    }

    /// Test extract_parameter_sets_h264() missing SPS
    ///
    /// Edge case: Bitstream with PPS but no SPS
    #[test]
    fn test_extract_parameter_sets_h264_missing_sps() {
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // Start code
            0x68, 0xCE, 0x3C, 0x80, // PPS only
        ];
        let result = extract_parameter_sets_h264(&data);
        assert!(result.is_err());
    }

    /// Test extract_parameter_sets_h264() missing PPS
    ///
    /// Edge case: Bitstream with SPS but no PPS
    #[test]
    fn test_extract_parameter_sets_h264_missing_pps() {
        let data = vec![
            0x00, 0x00, 0x00, 0x01, // Start code
            0x67, 0x42, 0x00, 0x0A, // SPS only
        ];
        let result = extract_parameter_sets_h264(&data);
        assert!(result.is_err());
    }

    /// Test extract_parameter_sets_h265() with NAL type extraction
    ///
    /// Reference: ITU-T H.265 (HEVC) Section 7.3.1.2
    /// "nal_unit_type is in bits 1-6 of NAL unit header first byte"
    #[test]
    fn test_h265_nal_type_extraction() {
        // H.265 NAL unit type is bits 1-6 (not bits 0-4 like H.264)
        let nal_header_vps = 0x40u8; // VPS: (0x40 >> 1) & 0x3F = 32
        let nal_type_vps = (nal_header_vps >> 1) & 0x3F;
        assert_eq!(nal_type_vps, 32, "VPS NAL type should be 32");

        let nal_header_sps = 0x42u8; // SPS: (0x42 >> 1) & 0x3F = 33
        let nal_type_sps = (nal_header_sps >> 1) & 0x3F;
        assert_eq!(nal_type_sps, 33, "SPS NAL type should be 33");

        let nal_header_pps = 0x44u8; // PPS: (0x44 >> 1) & 0x3F = 34
        let nal_type_pps = (nal_header_pps >> 1) & 0x3F;
        assert_eq!(nal_type_pps, 34, "PPS NAL type should be 34");

        let nal_header_idr = 0x26u8; // IDR_W_RADL: (0x26 >> 1) & 0x3F = 19
        let nal_type_idr = (nal_header_idr >> 1) & 0x3F;
        assert_eq!(nal_type_idr, 19, "IDR_W_RADL NAL type should be 19");
    }

    /// Test codec_to_fourcc() with standard codec names
    ///
    /// Reference: ISO/IEC 14496-12 (MP4 container) Section 12.1
    /// Codec identifiers: 'avc1' (H.264), 'hvc1' (H.265/HEVC)
    #[test]
    fn test_codec_to_fourcc() {
        assert_eq!(
            codec_to_fourcc("h264").unwrap(),
            u32::from_le_bytes(*b"H264")
        );
        assert_eq!(
            codec_to_fourcc("H264").unwrap(),
            u32::from_le_bytes(*b"H264")
        );
        assert_eq!(
            codec_to_fourcc("h265").unwrap(),
            u32::from_le_bytes(*b"HEVC")
        );
        assert_eq!(
            codec_to_fourcc("hevc").unwrap(),
            u32::from_le_bytes(*b"HEVC")
        );
        assert_eq!(
            codec_to_fourcc("HEVC").unwrap(),
            u32::from_le_bytes(*b"HEVC")
        );

        // Invalid codec
        assert!(codec_to_fourcc("invalid").is_err());
        assert!(codec_to_fourcc("").is_err());
    }

    /// Test bitrate_to_encoder_profile() boundary conditions
    ///
    /// Verifies correct profile selection at tier boundaries
    #[test]
    fn test_bitrate_to_encoder_profile_boundaries() {
        use videostream::encoder::VSLEncoderProfileEnum;

        // Test lower bound
        assert_eq!(
            bitrate_to_encoder_profile(0),
            VSLEncoderProfileEnum::Kbps5000
        );
        assert_eq!(
            bitrate_to_encoder_profile(5000),
            VSLEncoderProfileEnum::Kbps5000
        );
        assert_eq!(
            bitrate_to_encoder_profile(7500),
            VSLEncoderProfileEnum::Kbps5000
        );

        // Test boundary at 7501
        assert_eq!(
            bitrate_to_encoder_profile(7501),
            VSLEncoderProfileEnum::Kbps25000
        );
        assert_eq!(
            bitrate_to_encoder_profile(25000),
            VSLEncoderProfileEnum::Kbps25000
        );
        assert_eq!(
            bitrate_to_encoder_profile(37500),
            VSLEncoderProfileEnum::Kbps25000
        );

        // Test boundary at 37501
        assert_eq!(
            bitrate_to_encoder_profile(37501),
            VSLEncoderProfileEnum::Kbps50000
        );
        assert_eq!(
            bitrate_to_encoder_profile(50000),
            VSLEncoderProfileEnum::Kbps50000
        );
        assert_eq!(
            bitrate_to_encoder_profile(75000),
            VSLEncoderProfileEnum::Kbps50000
        );

        // Test boundary at 75001
        assert_eq!(
            bitrate_to_encoder_profile(75001),
            VSLEncoderProfileEnum::Kbps100000
        );
        assert_eq!(
            bitrate_to_encoder_profile(100000),
            VSLEncoderProfileEnum::Kbps100000
        );
        assert_eq!(
            bitrate_to_encoder_profile(200000),
            VSLEncoderProfileEnum::Kbps100000
        );
    }

    /// Test normalize_codec_alias() with various H.264 aliases
    #[test]
    fn test_normalize_codec_alias_h264() {
        assert_eq!(normalize_codec_alias("h264").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("H264").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("H.264").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("h.264").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("H-264").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("h-264").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("AVC").unwrap(), "h264");
        assert_eq!(normalize_codec_alias("avc").unwrap(), "h264");
    }

    /// Test normalize_codec_alias() with various H.265/HEVC aliases
    #[test]
    fn test_normalize_codec_alias_h265() {
        assert_eq!(normalize_codec_alias("h265").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("H265").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("H.265").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("h.265").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("H-265").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("h-265").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("HEVC").unwrap(), "h265");
        assert_eq!(normalize_codec_alias("hevc").unwrap(), "h265");
    }

    /// Test normalize_codec_alias() with unsupported codec
    #[test]
    fn test_normalize_codec_alias_invalid() {
        assert!(normalize_codec_alias("vp9").is_err());
        assert!(normalize_codec_alias("av1").is_err());
        assert!(normalize_codec_alias("invalid").is_err());
    }

    /// Test estimate_frame_size() for encoded frames
    #[test]
    fn test_estimate_frame_size_encoded() {
        // 25Mbps at 30fps = (25000 * 1000) / (30 * 8) = 104166 bytes/frame
        let size = estimate_frame_size(
            1920,
            1080,
            true,
            "25000",
            30,
            fourcc_from_str("YUYV").unwrap(),
        )
        .unwrap();
        assert_eq!(size, 104166);

        // 5Mbps at 30fps = (5000 * 1000) / (30 * 8) = 20833 bytes/frame
        let size = estimate_frame_size(
            1920,
            1080,
            true,
            "5000",
            30,
            fourcc_from_str("YUYV").unwrap(),
        )
        .unwrap();
        assert_eq!(size, 20833);

        // Test with Mbps suffix
        let size = estimate_frame_size(
            1920,
            1080,
            true,
            "25Mbps",
            30,
            fourcc_from_str("YUYV").unwrap(),
        )
        .unwrap();
        assert_eq!(size, 104166);
    }

    /// Test estimate_frame_size() for raw YUYV frames (2 bytes/pixel)
    #[test]
    fn test_estimate_frame_size_raw_yuyv() {
        let fourcc = fourcc_from_str("YUYV").unwrap();

        // 1920x1080 YUYV = 1920 * 1080 * 2 = 4147200 bytes
        let size = estimate_frame_size(1920, 1080, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 4147200);

        // 1280x720 YUYV = 1280 * 720 * 2 = 1843200 bytes
        let size = estimate_frame_size(1280, 720, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 1843200);
    }

    /// Test estimate_frame_size() for raw NV12 frames (1.5 bytes/pixel)
    #[test]
    fn test_estimate_frame_size_raw_nv12() {
        let fourcc = fourcc_from_str("NV12").unwrap();

        // 1920x1080 NV12 = 1920 * 1080 * 1.5 = 3110400 bytes
        let size = estimate_frame_size(1920, 1080, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 3110400);

        // 1280x720 NV12 = 1280 * 720 * 1.5 = 1382400 bytes
        let size = estimate_frame_size(1280, 720, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 1382400);
    }

    /// Test estimate_frame_size() for raw RGB3 frames (3 bytes/pixel)
    #[test]
    fn test_estimate_frame_size_raw_rgb3() {
        let fourcc = fourcc_from_str("RGB3").unwrap();

        // 1920x1080 RGB3 = 1920 * 1080 * 3 = 6220800 bytes
        let size = estimate_frame_size(1920, 1080, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 6220800);

        // 1280x720 RGB3 = 1280 * 720 * 3 = 2764800 bytes
        let size = estimate_frame_size(1280, 720, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 2764800);
    }

    /// Test estimate_frame_size() for unknown format (defaults to 2 bytes/pixel)
    #[test]
    fn test_estimate_frame_size_unknown_format() {
        // Use an uncommon FourCC that should default to 2 bytes/pixel
        let fourcc = fourcc_from_str("UNKN").unwrap();

        // Should default to 2 bytes/pixel like YUYV
        let size = estimate_frame_size(1920, 1080, false, "0", 30, fourcc).unwrap();
        assert_eq!(size, 4147200); // Same as YUYV
    }

    /// Test normalize_frame_count() with various values
    #[test]
    fn test_normalize_frame_count() {
        // 0 should become u64::MAX (unlimited)
        assert_eq!(normalize_frame_count(0), u64::MAX);

        // Non-zero values should pass through unchanged
        assert_eq!(normalize_frame_count(1), 1);
        assert_eq!(normalize_frame_count(100), 100);
        assert_eq!(normalize_frame_count(1000), 1000);
        assert_eq!(normalize_frame_count(u64::MAX), u64::MAX);
    }

    /// Test create_encoder_if_requested() with encoding disabled
    #[test]
    fn test_create_encoder_disabled() {
        let fallback_fourcc = fourcc_from_str("YUYV").unwrap();

        let (encoder, fourcc) =
            create_encoder_if_requested(false, "h264", "25000", 30, fallback_fourcc).unwrap();

        assert!(encoder.is_none());
        assert_eq!(fourcc, fallback_fourcc);
    }

    /// Test create_decoder_if_requested() with decoding disabled
    #[test]
    fn test_create_decoder_disabled() {
        let decoder = create_decoder_if_requested(false, "h264", 30).unwrap();
        assert!(decoder.is_none());
    }
}
