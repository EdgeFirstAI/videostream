// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::error::CliError;
use crate::utils;
use clap::Args as ClapArgs;
use mp4::{AvcConfig, MediaConfig, Mp4Config, Mp4Sample, Mp4Writer, TrackConfig};
use std::fs::File;
use std::io::Read;

#[derive(ClapArgs, Debug)]
pub struct Args {
    /// Input raw bitstream file (.h264 or .h265)
    input: String,

    /// Output MP4 file
    output: String,

    /// Frame rate (required for proper timing)
    #[arg(short = 'F', long, default_value = "30")]
    fps: u32,

    /// Force codec detection (h264|h265), auto-detect from extension if not specified
    #[arg(long)]
    codec: Option<String>,
}

pub fn execute(args: Args, _json: bool) -> Result<(), CliError> {
    log::info!("Converting {} to {}", args.input, args.output);

    // Detect codec from input file extension or explicit arg
    let codec = if let Some(ref c) = args.codec {
        c.to_lowercase()
    } else if args.input.ends_with(".h264") {
        "h264".to_string()
    } else if args.input.ends_with(".h265") || args.input.ends_with(".hevc") {
        "h265".to_string()
    } else {
        return Err(CliError::InvalidArgs(
            "Cannot detect codec from file extension. Use --codec h264 or --codec h265".to_string(),
        ));
    };

    if codec != "h264" {
        return Err(CliError::InvalidArgs(
            "MP4 muxing currently only supports H.264. H.265/HEVC support limited in mp4 crate."
                .to_string(),
        ));
    }

    log::info!("Codec: {}", codec.to_uppercase());
    log::info!("Frame rate: {} fps", args.fps);

    // Read input file
    log::info!("Reading input file...");
    let mut input_file = File::open(&args.input)
        .map_err(|e| CliError::General(format!("Failed to open input file: {}", e)))?;

    let mut bitstream_data = Vec::new();
    input_file
        .read_to_end(&mut bitstream_data)
        .map_err(|e| CliError::General(format!("Failed to read input file: {}", e)))?;

    log::info!("Read {} bytes", bitstream_data.len());

    // Extract SPS/PPS from bitstream
    log::info!("Extracting codec parameters...");
    let param_sets = utils::extract_parameter_sets_h264(&bitstream_data)?;
    log::info!(
        "Found SPS: {} bytes, PPS: {} bytes",
        param_sets.sps.len(),
        param_sets.pps.len()
    );

    // Parse NAL units to extract individual frames
    log::info!("Parsing NAL units...");
    let nal_units = parse_nal_units(&bitstream_data)?;
    log::info!("Found {} NAL units", nal_units.len());

    // Filter to get only VCL (Video Coding Layer) NAL units (actual frame data)
    let mut frames = Vec::new();
    for nal in nal_units {
        if nal.is_empty() {
            continue;
        }

        let nal_type = nal[0] & 0x1F;
        // H.264 VCL NAL types: 1-5 (non-IDR, IDR, etc.)
        if (1..=5).contains(&nal_type) {
            let is_keyframe = nal_type == 5; // IDR frame
            frames.push((nal.to_vec(), is_keyframe));
        }
    }

    log::info!("Found {} video frames", frames.len());

    if frames.is_empty() {
        return Err(CliError::General(
            "No video frames found in input file".to_string(),
        ));
    }

    // Detect resolution from SPS (simplified - assumes standard SPS structure)
    // For a more robust solution, we'd need a full SPS parser
    let (width, height) = detect_resolution_from_sps(&param_sets.sps)?;
    log::info!("Detected resolution: {}x{}", width, height);

    // Create MP4 file
    log::info!("Creating MP4 file...");
    let output_file = File::create(&args.output)
        .map_err(|e| CliError::General(format!("Failed to create output file: {}", e)))?;

    let mp4_config = Mp4Config {
        major_brand: str::parse("isom").unwrap(),
        minor_version: 512,
        compatible_brands: vec![
            str::parse("isom").unwrap(),
            str::parse("iso2").unwrap(),
            str::parse("avc1").unwrap(),
            str::parse("mp41").unwrap(),
        ],
        timescale: 1000,
    };

    let mut writer = Mp4Writer::write_start(output_file, &mp4_config)
        .map_err(|e| CliError::General(format!("Failed to initialize MP4 writer: {}", e)))?;

    // Create video track
    let avc_config = AvcConfig {
        width: width as u16,
        height: height as u16,
        seq_param_set: param_sets.sps,
        pic_param_set: param_sets.pps,
    };

    let track_conf = TrackConfig {
        track_type: mp4::TrackType::Video,
        timescale: 1000,
        language: "und".to_string(),
        media_conf: MediaConfig::AvcConfig(avc_config),
    };

    writer
        .add_track(&track_conf)
        .map_err(|e| CliError::General(format!("Failed to add video track: {}", e)))?;

    let track_id: u32 = 1;
    let frame_duration_ms = 1000 / args.fps;

    // Write frames (convert from Annex-B to AVCC format)
    log::info!("Writing {} frames to MP4...", frames.len());
    for (i, (frame_data, is_keyframe)) in frames.iter().enumerate() {
        // Convert NAL unit from Annex-B to AVCC format
        // AVCC format: [4-byte length in big-endian][NAL data]
        let nal_size = frame_data.len() as u32;
        let mut avcc_data = Vec::with_capacity(4 + frame_data.len());

        // Write 4-byte length prefix in big-endian
        avcc_data.extend_from_slice(&nal_size.to_be_bytes());
        // Write NAL unit data
        avcc_data.extend_from_slice(frame_data);

        let sample = Mp4Sample {
            start_time: (i as u64 * frame_duration_ms as u64),
            duration: frame_duration_ms,
            rendering_offset: 0,
            is_sync: *is_keyframe,
            bytes: mp4::Bytes::from(avcc_data),
        };

        writer
            .write_sample(track_id, &sample)
            .map_err(|e| CliError::General(format!("Failed to write sample {}: {}", i, e)))?;

        if (i + 1) % 100 == 0 {
            log::debug!("Wrote {} / {} frames", i + 1, frames.len());
        }
    }

    // Finalize MP4
    log::info!("Finalizing MP4 file...");
    writer
        .write_end()
        .map_err(|e| CliError::General(format!("Failed to finalize MP4: {}", e)))?;

    log::info!("Conversion complete!");
    log::info!("Input:  {} ({} bytes)", args.input, bitstream_data.len());
    log::info!("Output: {}", args.output);
    log::info!("Frames: {} ({} fps)", frames.len(), args.fps);

    Ok(())
}

/// Parse NAL units from Annex-B format bitstream
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

        // Extract NAL unit
        if nal_start < i {
            nal_units.push(&data[nal_start..i]);
        }
    }

    Ok(nal_units)
}

/// Detect resolution from SPS NAL unit
///
/// Parses H.264 SPS to extract video resolution using exponential-Golomb decoding.
fn detect_resolution_from_sps(sps: &[u8]) -> Result<(i32, i32), CliError> {
    if sps.len() < 4 {
        return Err(CliError::General(
            "SPS too short to parse resolution".to_string(),
        ));
    }

    // Create a bit reader for exponential-Golomb decoding
    let mut reader = BitReader::new(&sps[1..]); // Skip NAL header

    // Skip profile_idc, constraint flags, reserved bits, level_idc
    reader.skip_bits(8 + 8); // profile_idc + (6 constraint flags + 2 reserved + level_idc)

    // Read seq_parameter_set_id
    let _seq_param_id = reader
        .read_ue()
        .ok_or_else(|| CliError::General("Failed to read seq_parameter_set_id".to_string()))?;

    // Check profile for chroma format (high profiles have additional fields)
    let profile_idc = sps[1];
    if matches!(
        profile_idc,
        100 | 110 | 122 | 244 | 44 | 83 | 86 | 118 | 128 | 138 | 139 | 134
    ) {
        // chroma_format_idc
        let chroma_format_idc = reader
            .read_ue()
            .ok_or_else(|| CliError::General("Failed to read chroma_format_idc".to_string()))?;

        if chroma_format_idc == 3 {
            // separate_colour_plane_flag
            reader.skip_bits(1);
        }

        // bit_depth_luma_minus8
        let _bit_depth_luma = reader
            .read_ue()
            .ok_or_else(|| CliError::General("Failed to read bit_depth_luma".to_string()))?;

        // bit_depth_chroma_minus8
        let _bit_depth_chroma = reader
            .read_ue()
            .ok_or_else(|| CliError::General("Failed to read bit_depth_chroma".to_string()))?;

        // qpprime_y_zero_transform_bypass_flag
        reader.skip_bits(1);

        // seq_scaling_matrix_present_flag
        let scaling_matrix_present = reader.read_bit();
        if scaling_matrix_present {
            // Skip scaling lists (complex, varies by chroma format)
            // For simplicity, we'll try to skip them
            let num_lists = if chroma_format_idc != 3 { 8 } else { 12 };
            for _ in 0..num_lists {
                let present = reader.read_bit();
                if present {
                    // Skip the scaling list
                    // This is complex - for now we'll just hope we can continue
                    reader.skip_scaling_list();
                }
            }
        }
    }

    // log2_max_frame_num_minus4
    let _log2_max_frame_num = reader
        .read_ue()
        .ok_or_else(|| CliError::General("Failed to read log2_max_frame_num".to_string()))?;

    // pic_order_cnt_type
    let pic_order_cnt_type = reader
        .read_ue()
        .ok_or_else(|| CliError::General("Failed to read pic_order_cnt_type".to_string()))?;

    if pic_order_cnt_type == 0 {
        // log2_max_pic_order_cnt_lsb_minus4
        let _log2_max_poc = reader.read_ue().ok_or_else(|| {
            CliError::General("Failed to read log2_max_pic_order_cnt".to_string())
        })?;
    } else if pic_order_cnt_type == 1 {
        // delta_pic_order_always_zero_flag
        reader.skip_bits(1);
        // offset_for_non_ref_pic
        let _offset_non_ref = reader.read_se().ok_or_else(|| {
            CliError::General("Failed to read offset_for_non_ref_pic".to_string())
        })?;
        // offset_for_top_to_bottom_field
        let _offset_top_bottom = reader.read_se().ok_or_else(|| {
            CliError::General("Failed to read offset_for_top_to_bottom_field".to_string())
        })?;
        // num_ref_frames_in_pic_order_cnt_cycle
        let num_ref_frames = reader.read_ue().ok_or_else(|| {
            CliError::General("Failed to read num_ref_frames_in_pic_order_cnt_cycle".to_string())
        })?;
        // offset_for_ref_frame[i]
        for _ in 0..num_ref_frames {
            let _offset = reader.read_se().ok_or_else(|| {
                CliError::General("Failed to read offset_for_ref_frame".to_string())
            })?;
        }
    }

    // max_num_ref_frames
    let _max_num_ref_frames = reader
        .read_ue()
        .ok_or_else(|| CliError::General("Failed to read max_num_ref_frames".to_string()))?;

    // gaps_in_frame_num_value_allowed_flag
    reader.skip_bits(1);

    // pic_width_in_mbs_minus1
    let pic_width_in_mbs_minus1 = reader
        .read_ue()
        .ok_or_else(|| CliError::General("Failed to read pic_width_in_mbs_minus1".to_string()))?;

    // pic_height_in_map_units_minus1
    let pic_height_in_map_units_minus1 = reader.read_ue().ok_or_else(|| {
        CliError::General("Failed to read pic_height_in_map_units_minus1".to_string())
    })?;

    // frame_mbs_only_flag
    let frame_mbs_only_flag = reader.read_bit();

    // Calculate resolution
    let width = (pic_width_in_mbs_minus1 + 1) * 16;
    let mut height = (pic_height_in_map_units_minus1 + 1) * 16;

    // If not frame_mbs_only (interlaced), height is doubled
    if !frame_mbs_only_flag {
        height *= 2;
    }

    log::info!("Parsed resolution from SPS: {}x{}", width, height);

    Ok((width as i32, height as i32))
}

/// Simple bit reader for H.264 bitstream parsing
struct BitReader<'a> {
    data: &'a [u8],
    byte_pos: usize,
    bit_pos: u8, // 0-7, position within current byte
}

impl<'a> BitReader<'a> {
    fn new(data: &'a [u8]) -> Self {
        Self {
            data,
            byte_pos: 0,
            bit_pos: 0,
        }
    }

    fn read_bit(&mut self) -> bool {
        if self.byte_pos >= self.data.len() {
            return false;
        }

        let bit = (self.data[self.byte_pos] >> (7 - self.bit_pos)) & 1;
        self.bit_pos += 1;
        if self.bit_pos == 8 {
            self.bit_pos = 0;
            self.byte_pos += 1;
        }

        bit == 1
    }

    fn skip_bits(&mut self, n: usize) {
        for _ in 0..n {
            self.read_bit();
        }
    }

    fn read_bits(&mut self, n: usize) -> Option<u32> {
        if n > 32 {
            return None;
        }

        let mut result = 0u32;
        for _ in 0..n {
            result = (result << 1) | (self.read_bit() as u32);
        }

        Some(result)
    }

    /// Read unsigned exponential-Golomb coded value
    fn read_ue(&mut self) -> Option<u32> {
        // Count leading zeros
        let mut leading_zeros = 0;
        while !self.read_bit() {
            leading_zeros += 1;
            if leading_zeros > 31 {
                return None; // Prevent infinite loop
            }
        }

        if leading_zeros == 0 {
            return Some(0);
        }

        // Read the remaining bits
        let value = self.read_bits(leading_zeros)?;
        Some((1 << leading_zeros) - 1 + value)
    }

    /// Read signed exponential-Golomb coded value
    fn read_se(&mut self) -> Option<i32> {
        let ue_value = self.read_ue()?;
        let sign = if ue_value & 1 == 0 { -1 } else { 1 };
        Some(sign * (ue_value + 1).div_ceil(2) as i32)
    }

    /// Skip a scaling list (simplified - just try to skip it)
    fn skip_scaling_list(&mut self) {
        // This is complex - scaling lists have variable length
        // For simplicity, we'll skip a fixed number of values
        // A proper implementation would parse the actual structure
        for _ in 0..64 {
            // Try to read a se(v) value
            if self.read_ue().is_none() {
                break;
            }
        }
    }
}
