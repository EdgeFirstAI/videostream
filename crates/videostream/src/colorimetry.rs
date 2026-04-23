// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Au-Zone Technologies

//! Colorimetry enums for camera capture formats.
//!
//! Mirrors the colorimetry fields exposed on V4L2's `struct v4l2_format`
//! (and, prospectively, `libcamera::ColorSpace`). The variants and naming
//! follow the EdgeFirst schema
//! ([`edgefirst_msgs/msg/CameraFrame.msg`](https://github.com/EdgeFirstAI/schemas/blob/main/edgefirst_msgs/msg/CameraFrame.msg))
//! so values produced by this crate round-trip through the ROS message
//! layer without an extra translation table.
//!
//! Each enum is `#[non_exhaustive]` — the V4L2 UAPI and libcamera both
//! grow new values over time (HDR transfer functions, wide-gamut
//! primaries) and callers should treat unknown driver values as `None`
//! rather than a breaking change.
//!
//! A V4L2 `_DEFAULT` value (0) maps to `None` on the accessor return
//! type, matching the library's contract that the driver did not resolve
//! the field. Callers can infer a default from the negotiated pixel
//! format or treat the field as unknown. V4L2 values that are valid but
//! not surfaced by this library (e.g. `V4L2_COLORSPACE_SMPTE240M`,
//! `V4L2_XFER_FUNC_DCI_P3`) also map to `None`; a future release may
//! widen the enum sets.

#![forbid(unsafe_code)]

use core::fmt;

// V4L2 UAPI constants mirrored from <linux/videodev2.h>. Declared here
// rather than pulled through `videostream-sys` so the mapping tables
// below stay self-documenting and reviewable against the kernel header.
// Values are stable kernel UAPI — any change upstream would be an ABI
// break for every V4L2 consumer on the system.

// enum v4l2_colorspace
const V4L2_COLORSPACE_SMPTE170M: u32 = 1;
const V4L2_COLORSPACE_REC709: u32 = 3;
const V4L2_COLORSPACE_470_SYSTEM_M: u32 = 5;
const V4L2_COLORSPACE_470_SYSTEM_BG: u32 = 6;
const V4L2_COLORSPACE_JPEG: u32 = 7;
const V4L2_COLORSPACE_SRGB: u32 = 8;
const V4L2_COLORSPACE_BT2020: u32 = 10;

// enum v4l2_xfer_func
const V4L2_XFER_FUNC_709: u32 = 1;
const V4L2_XFER_FUNC_SRGB: u32 = 2;
const V4L2_XFER_FUNC_NONE: u32 = 5;
const V4L2_XFER_FUNC_SMPTE2084: u32 = 7;

// enum v4l2_ycbcr_encoding
const V4L2_YCBCR_ENC_601: u32 = 1;
const V4L2_YCBCR_ENC_709: u32 = 2;
const V4L2_YCBCR_ENC_BT2020: u32 = 6;

// enum v4l2_quantization
const V4L2_QUANTIZATION_FULL_RANGE: u32 = 1;
const V4L2_QUANTIZATION_LIM_RANGE: u32 = 2;

/// Color primaries (`color_space` in the EdgeFirst schema).
///
/// Corresponds to `v4l2_colorspace` / `libcamera::ColorSpace::primaries`.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ColorSpace {
    /// BT.709 / Rec.709 (HDTV; sRGB shares these primaries).
    Bt709,
    /// BT.2020 / Rec.2020 (UHDTV wide-gamut).
    Bt2020,
    /// sRGB (IEC 61966-2-1).
    Srgb,
    /// SMPTE 170M (NTSC / BT.601 525-line).
    Smpte170m,
}

/// Transfer function (`color_transfer` in the EdgeFirst schema).
///
/// Corresponds to `v4l2_xfer_func` / `libcamera::ColorSpace::transferFunction`.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ColorTransfer {
    /// BT.709 OETF.
    Bt709,
    /// sRGB (piecewise gamma ~2.2).
    Srgb,
    /// SMPTE ST 2084 Perceptual Quantizer (HDR10).
    Pq,
    /// Hybrid Log-Gamma (ARIB STD-B67). Present for schema / libcamera
    /// alignment; not currently surfaced by the V4L2 backend because
    /// the kernel UAPI (as of Linux 6.17) has no `V4L2_XFER_FUNC_HLG`
    /// constant.
    Hlg,
    /// Linear (no transfer function; V4L2 `XFER_FUNC_NONE`).
    Linear,
}

/// YCbCr encoding matrix (`color_encoding` in the EdgeFirst schema).
///
/// Corresponds to `v4l2_ycbcr_encoding` / `libcamera::ColorSpace::ycbcrEncoding`.
/// Not applicable to RGB formats — drivers typically return
/// `V4L2_YCBCR_ENC_DEFAULT` in that case, which this enum surfaces as
/// `None`.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ColorEncoding {
    /// BT.601 (SDTV).
    Bt601,
    /// BT.709 (HDTV).
    Bt709,
    /// BT.2020 (UHDTV).
    Bt2020,
}

/// Quantization range (`color_range` in the EdgeFirst schema).
///
/// Corresponds to `v4l2_quantization` / `libcamera::ColorSpace::range`.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ColorRange {
    /// Full range (0..=255 for 8-bit).
    Full,
    /// Limited / studio range (16..=235 for 8-bit luma).
    Limited,
}

impl ColorSpace {
    /// Returns the schema-level string tag for this value
    /// (`"bt709"`, `"bt2020"`, `"srgb"`, `"smpte170m"`).
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Bt709 => "bt709",
            Self::Bt2020 => "bt2020",
            Self::Srgb => "srgb",
            Self::Smpte170m => "smpte170m",
        }
    }

    /// Maps a raw V4L2 `v4l2_colorspace` value to a [`ColorSpace`].
    ///
    /// Returns `None` for:
    /// - `V4L2_COLORSPACE_DEFAULT` (0) — the driver did not resolve.
    /// - V4L2 values that are valid but not in the EdgeFirst schema
    ///   vocabulary (`SMPTE240M`, `BT878`, `OPRGB`, `RAW`, `DCI_P3`).
    ///   A caller that needs these should query the raw C accessor
    ///   (`vsl_camera_color_space`) and interpret the value itself.
    ///
    /// `V4L2_COLORSPACE_JPEG` maps to [`ColorSpace::Srgb`] because the
    /// V4L2 kernel defines JPEG colorspace as sRGB primaries with
    /// BT.601 encoding and full range (see the `V4L2_MAP_*` macros in
    /// `<linux/videodev2.h>`); that is semantically a primaries-only
    /// alias for sRGB. `V4L2_COLORSPACE_470_SYSTEM_M` and
    /// `V4L2_COLORSPACE_470_SYSTEM_BG` (legacy NTSC / PAL-SECAM) map to
    /// [`ColorSpace::Smpte170m`] because they share primaries with
    /// SMPTE 170M within the precision of this enum.
    pub fn from_v4l2(v: u32) -> Option<Self> {
        match v {
            V4L2_COLORSPACE_SMPTE170M => Some(Self::Smpte170m),
            V4L2_COLORSPACE_REC709 => Some(Self::Bt709),
            V4L2_COLORSPACE_470_SYSTEM_M => Some(Self::Smpte170m),
            V4L2_COLORSPACE_470_SYSTEM_BG => Some(Self::Smpte170m),
            V4L2_COLORSPACE_JPEG => Some(Self::Srgb),
            V4L2_COLORSPACE_SRGB => Some(Self::Srgb),
            V4L2_COLORSPACE_BT2020 => Some(Self::Bt2020),
            _ => None,
        }
    }
}

impl ColorTransfer {
    /// Returns the schema-level string tag for this value
    /// (`"bt709"`, `"srgb"`, `"pq"`, `"hlg"`, `"linear"`).
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Bt709 => "bt709",
            Self::Srgb => "srgb",
            Self::Pq => "pq",
            Self::Hlg => "hlg",
            Self::Linear => "linear",
        }
    }

    /// Maps a raw V4L2 `v4l2_xfer_func` value to a [`ColorTransfer`].
    ///
    /// Returns `None` for `V4L2_XFER_FUNC_DEFAULT` (0) and for values
    /// that are valid but not in the EdgeFirst schema vocabulary
    /// (`OPRGB`, `SMPTE240M`, `DCI_P3`). Note that V4L2's
    /// `V4L2_XFER_FUNC_NONE` means "no transfer applied" (linear
    /// working space) and maps to [`ColorTransfer::Linear`] here.
    ///
    /// [`ColorTransfer::Hlg`] has no V4L2 mapping — the kernel UAPI
    /// (as of Linux 6.17) does not define `V4L2_XFER_FUNC_HLG`. The
    /// variant exists for schema / libcamera parity and for when the
    /// kernel adds one.
    pub fn from_v4l2(v: u32) -> Option<Self> {
        match v {
            V4L2_XFER_FUNC_709 => Some(Self::Bt709),
            V4L2_XFER_FUNC_SRGB => Some(Self::Srgb),
            V4L2_XFER_FUNC_NONE => Some(Self::Linear),
            V4L2_XFER_FUNC_SMPTE2084 => Some(Self::Pq),
            _ => None,
        }
    }
}

impl ColorEncoding {
    /// Returns the schema-level string tag for this value
    /// (`"bt601"`, `"bt709"`, `"bt2020"`).
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Bt601 => "bt601",
            Self::Bt709 => "bt709",
            Self::Bt2020 => "bt2020",
        }
    }

    /// Maps a raw V4L2 `v4l2_ycbcr_encoding` value to a [`ColorEncoding`].
    ///
    /// Returns `None` for `V4L2_YCBCR_ENC_DEFAULT` (0) and for values
    /// not in the EdgeFirst schema vocabulary (`XV601`, `XV709`,
    /// `SYCC`, `BT2020_CONST_LUM`, `SMPTE240M`).
    pub fn from_v4l2(v: u32) -> Option<Self> {
        match v {
            V4L2_YCBCR_ENC_601 => Some(Self::Bt601),
            V4L2_YCBCR_ENC_709 => Some(Self::Bt709),
            V4L2_YCBCR_ENC_BT2020 => Some(Self::Bt2020),
            _ => None,
        }
    }
}

impl ColorRange {
    /// Returns the schema-level string tag for this value
    /// (`"full"`, `"limited"`).
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Full => "full",
            Self::Limited => "limited",
        }
    }

    /// Maps a raw V4L2 `v4l2_quantization` value to a [`ColorRange`].
    ///
    /// Returns `None` for `V4L2_QUANTIZATION_DEFAULT` (0).
    pub fn from_v4l2(v: u32) -> Option<Self> {
        match v {
            V4L2_QUANTIZATION_FULL_RANGE => Some(Self::Full),
            V4L2_QUANTIZATION_LIM_RANGE => Some(Self::Limited),
            _ => None,
        }
    }
}

impl fmt::Display for ColorSpace {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl fmt::Display for ColorTransfer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl fmt::Display for ColorEncoding {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl fmt::Display for ColorRange {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ColorSpace: every V4L2 value in range 0..=12 has a defined outcome.

    #[test]
    fn color_space_default_is_none() {
        assert_eq!(ColorSpace::from_v4l2(0), None);
    }

    #[test]
    fn color_space_rec709_maps_to_bt709() {
        // REC709 = 3 in <linux/videodev2.h>; this is the dominant
        // HDTV colorspace and the most common non-default value.
        assert_eq!(ColorSpace::from_v4l2(3), Some(ColorSpace::Bt709));
    }

    #[test]
    fn color_space_srgb_maps_to_srgb() {
        // SRGB = 8 in <linux/videodev2.h>.
        assert_eq!(ColorSpace::from_v4l2(8), Some(ColorSpace::Srgb));
    }

    #[test]
    fn color_space_jpeg_maps_to_srgb() {
        // JPEG = 7; V4L2 defines JPEG colorspace as sRGB primaries.
        assert_eq!(ColorSpace::from_v4l2(7), Some(ColorSpace::Srgb));
    }

    #[test]
    fn color_space_smpte170m_maps_to_smpte170m() {
        assert_eq!(ColorSpace::from_v4l2(1), Some(ColorSpace::Smpte170m));
    }

    #[test]
    fn color_space_legacy_ntsc_pal_map_to_smpte170m() {
        assert_eq!(ColorSpace::from_v4l2(5), Some(ColorSpace::Smpte170m));
        assert_eq!(ColorSpace::from_v4l2(6), Some(ColorSpace::Smpte170m));
    }

    #[test]
    fn color_space_bt2020_maps_to_bt2020() {
        assert_eq!(ColorSpace::from_v4l2(10), Some(ColorSpace::Bt2020));
    }

    #[test]
    fn color_space_unsurfaced_values_return_none() {
        // SMPTE240M=2, BT878=4, OPRGB=9, RAW=11, DCI_P3=12.
        for v in [2u32, 4, 9, 11, 12] {
            assert_eq!(
                ColorSpace::from_v4l2(v),
                None,
                "V4L2 colorspace value {} should map to None",
                v
            );
        }
    }

    #[test]
    fn color_space_unknown_value_returns_none() {
        assert_eq!(ColorSpace::from_v4l2(99), None);
    }

    #[test]
    fn color_space_as_str_matches_schema() {
        assert_eq!(ColorSpace::Bt709.as_str(), "bt709");
        assert_eq!(ColorSpace::Bt2020.as_str(), "bt2020");
        assert_eq!(ColorSpace::Srgb.as_str(), "srgb");
        assert_eq!(ColorSpace::Smpte170m.as_str(), "smpte170m");
    }

    #[test]
    fn color_space_display_matches_as_str() {
        assert_eq!(format!("{}", ColorSpace::Bt709), "bt709");
        assert_eq!(format!("{}", ColorSpace::Bt2020), "bt2020");
        assert_eq!(format!("{}", ColorSpace::Srgb), "srgb");
        assert_eq!(format!("{}", ColorSpace::Smpte170m), "smpte170m");
    }

    // ColorTransfer

    #[test]
    fn color_transfer_default_is_none() {
        assert_eq!(ColorTransfer::from_v4l2(0), None);
    }

    #[test]
    fn color_transfer_bt709() {
        assert_eq!(ColorTransfer::from_v4l2(1), Some(ColorTransfer::Bt709));
    }

    #[test]
    fn color_transfer_srgb() {
        assert_eq!(ColorTransfer::from_v4l2(2), Some(ColorTransfer::Srgb));
    }

    #[test]
    fn color_transfer_none_maps_to_linear() {
        // V4L2_XFER_FUNC_NONE = 5 — "no transfer applied" == linear.
        assert_eq!(ColorTransfer::from_v4l2(5), Some(ColorTransfer::Linear));
    }

    #[test]
    fn color_transfer_pq() {
        // V4L2_XFER_FUNC_SMPTE2084 = 7.
        assert_eq!(ColorTransfer::from_v4l2(7), Some(ColorTransfer::Pq));
    }

    #[test]
    fn color_transfer_hlg_has_no_v4l2_mapping() {
        // The kernel UAPI has no V4L2_XFER_FUNC_HLG constant; the
        // variant exists for schema/libcamera alignment only.
        for v in 0u32..=10 {
            assert_ne!(ColorTransfer::from_v4l2(v), Some(ColorTransfer::Hlg));
        }
    }

    #[test]
    fn color_transfer_unsurfaced_values_return_none() {
        // OPRGB=3, SMPTE240M=4, DCI_P3=6.
        for v in [3u32, 4, 6] {
            assert_eq!(
                ColorTransfer::from_v4l2(v),
                None,
                "V4L2 xfer_func value {} should map to None",
                v
            );
        }
    }

    #[test]
    fn color_transfer_as_str_matches_schema() {
        assert_eq!(ColorTransfer::Bt709.as_str(), "bt709");
        assert_eq!(ColorTransfer::Srgb.as_str(), "srgb");
        assert_eq!(ColorTransfer::Pq.as_str(), "pq");
        assert_eq!(ColorTransfer::Hlg.as_str(), "hlg");
        assert_eq!(ColorTransfer::Linear.as_str(), "linear");
    }

    #[test]
    fn color_transfer_display_matches_as_str() {
        assert_eq!(format!("{}", ColorTransfer::Bt709), "bt709");
        assert_eq!(format!("{}", ColorTransfer::Linear), "linear");
    }

    // ColorEncoding

    #[test]
    fn color_encoding_default_is_none() {
        assert_eq!(ColorEncoding::from_v4l2(0), None);
    }

    #[test]
    fn color_encoding_all_mapped_variants() {
        assert_eq!(ColorEncoding::from_v4l2(1), Some(ColorEncoding::Bt601));
        assert_eq!(ColorEncoding::from_v4l2(2), Some(ColorEncoding::Bt709));
        assert_eq!(ColorEncoding::from_v4l2(6), Some(ColorEncoding::Bt2020));
    }

    #[test]
    fn color_encoding_unsurfaced_values_return_none() {
        // XV601=3, XV709=4, SYCC=5, BT2020_CONST_LUM=7, SMPTE240M=8.
        for v in [3u32, 4, 5, 7, 8] {
            assert_eq!(
                ColorEncoding::from_v4l2(v),
                None,
                "V4L2 ycbcr_enc value {} should map to None",
                v
            );
        }
    }

    #[test]
    fn color_encoding_as_str_matches_schema() {
        assert_eq!(ColorEncoding::Bt601.as_str(), "bt601");
        assert_eq!(ColorEncoding::Bt709.as_str(), "bt709");
        assert_eq!(ColorEncoding::Bt2020.as_str(), "bt2020");
    }

    #[test]
    fn color_encoding_display_matches_as_str() {
        assert_eq!(format!("{}", ColorEncoding::Bt601), "bt601");
        assert_eq!(format!("{}", ColorEncoding::Bt709), "bt709");
        assert_eq!(format!("{}", ColorEncoding::Bt2020), "bt2020");
    }

    // ColorRange

    #[test]
    fn color_range_default_is_none() {
        assert_eq!(ColorRange::from_v4l2(0), None);
    }

    #[test]
    fn color_range_full_and_limited() {
        assert_eq!(ColorRange::from_v4l2(1), Some(ColorRange::Full));
        assert_eq!(ColorRange::from_v4l2(2), Some(ColorRange::Limited));
    }

    #[test]
    fn color_range_unknown_values_return_none() {
        assert_eq!(ColorRange::from_v4l2(3), None);
        assert_eq!(ColorRange::from_v4l2(99), None);
    }

    #[test]
    fn color_range_as_str_matches_schema() {
        assert_eq!(ColorRange::Full.as_str(), "full");
        assert_eq!(ColorRange::Limited.as_str(), "limited");
    }

    #[test]
    fn color_range_display_matches_as_str() {
        assert_eq!(format!("{}", ColorRange::Full), "full");
        assert_eq!(format!("{}", ColorRange::Limited), "limited");
    }

    // Debug formatting — derived, but verify it renders the variant name
    // so users debugging logs aren't confused by the as_str lowercase form.

    #[test]
    fn debug_impls_include_variant_name() {
        assert!(format!("{:?}", ColorSpace::Bt709).contains("Bt709"));
        assert!(format!("{:?}", ColorTransfer::Pq).contains("Pq"));
        assert!(format!("{:?}", ColorEncoding::Bt2020).contains("Bt2020"));
        assert!(format!("{:?}", ColorRange::Limited).contains("Limited"));
    }
}
