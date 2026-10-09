// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Au-Zone Technologies

//! VideoStream's colorimetry mirrors `edgefirst_tensor::colorimetry`. This
//! test compares the schema strings both produce for every combination of
//! V4L2 colorspace, xfer_func, ycbcr_enc and quantization, including values
//! past the end of each kernel enum.
//!
//! The pinned edgefirst-tensor 0.34.1 derives a `DEFAULT` quantization from
//! the colorspace only, which is the kernel rule for YUV formats, so parity
//! is checked for YUV formats. RGB, Bayer and HSV formats are checked against the
//! kernel rule directly; RGB parity resumes once a HAL release with a
//! format-aware `from_v4l2` is pinned.

use edgefirst_tensor::Colorimetry as HalColorimetry;
use videostream::colorimetry::Colorimetry;

// One past the highest value of each enum in <linux/videodev2.h>, plus margin.
const COLORSPACES: u32 = 16;
const XFER_FUNCS: u32 = 10;
const YCBCR_ENCS: u32 = 12;
const QUANTIZATIONS: u32 = 5;

fn vsl_strings(c: Colorimetry) -> [Option<&'static str>; 4] {
    [
        c.space.map(|v| v.as_str()),
        c.transfer.map(|v| v.as_str()),
        c.encoding.map(|v| v.as_str()),
        c.range.map(|v| v.as_str()),
    ]
}

fn hal_strings(c: HalColorimetry) -> [Option<&'static str>; 4] {
    [
        c.space.map(|v| v.as_str()),
        c.transfer.map(|v| v.as_str()),
        c.encoding.map(|v| v.as_str()),
        c.range.map(|v| v.as_str()),
    ]
}

#[test]
fn yuv_colorimetry_matches_hal_for_every_v4l2_combination() {
    let mut checked = 0u32;
    for cs in (0..COLORSPACES).chain([u32::MAX]) {
        for xfer in (0..XFER_FUNCS).chain([u32::MAX]) {
            for enc in (0..YCBCR_ENCS).chain([u32::MAX]) {
                for quant in (0..QUANTIZATIONS).chain([u32::MAX]) {
                    assert_eq!(
                        vsl_strings(Colorimetry::from_v4l2(cs, xfer, enc, quant, false)),
                        hal_strings(HalColorimetry::from_v4l2(cs, xfer, enc, quant)),
                        "colorspace={cs} xfer_func={xfer} ycbcr_enc={enc} quantization={quant}",
                    );
                    checked += 1;
                }
            }
        }
    }
    assert_eq!(
        checked,
        (COLORSPACES + 1) * (XFER_FUNCS + 1) * (YCBCR_ENCS + 1) * (QUANTIZATIONS + 1)
    );
}

/// `V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb_or_hsv, ...)` is full range for
/// every RGB, Bayer or HSV format; every other axis is independent of the format,
/// so it matches the HAL's YUV result.
#[test]
fn rgb_colorimetry_follows_kernel_rule_for_every_v4l2_combination() {
    for cs in (0..COLORSPACES).chain([u32::MAX]) {
        for xfer in (0..XFER_FUNCS).chain([u32::MAX]) {
            for enc in (0..YCBCR_ENCS).chain([u32::MAX]) {
                for quant in (0..QUANTIZATIONS).chain([u32::MAX]) {
                    let mut expected = hal_strings(HalColorimetry::from_v4l2(cs, xfer, enc, quant));
                    if quant == 0 {
                        expected[3] = Some("full");
                    }
                    assert_eq!(
                        vsl_strings(Colorimetry::from_v4l2(cs, xfer, enc, quant, true)),
                        expected,
                        "colorspace={cs} xfer_func={xfer} ycbcr_enc={enc} quantization={quant}",
                    );
                }
            }
        }
    }
}

#[test]
fn per_axis_mapping_matches_hal() {
    use edgefirst_tensor::{
        ColorEncoding as HalEncoding, ColorRange as HalRange, ColorSpace as HalSpace,
        ColorTransfer as HalTransfer,
    };
    use videostream::colorimetry::{ColorEncoding, ColorRange, ColorSpace, ColorTransfer};

    for v in (0..COLORSPACES).chain([u32::MAX]) {
        assert_eq!(
            ColorSpace::from_v4l2(v).map(|c| c.as_str()),
            HalSpace::from_v4l2(v).map(|c| c.as_str()),
            "colorspace={v}"
        );
    }
    for v in (0..XFER_FUNCS).chain([u32::MAX]) {
        assert_eq!(
            ColorTransfer::from_v4l2(v).map(|c| c.as_str()),
            HalTransfer::from_v4l2(v).map(|c| c.as_str()),
            "xfer_func={v}"
        );
    }
    for v in (0..YCBCR_ENCS).chain([u32::MAX]) {
        assert_eq!(
            ColorEncoding::from_v4l2(v).map(|c| c.as_str()),
            HalEncoding::from_v4l2(v).map(|c| c.as_str()),
            "ycbcr_enc={v}"
        );
    }
    for v in (0..QUANTIZATIONS).chain([u32::MAX]) {
        assert_eq!(
            ColorRange::from_v4l2(v).map(|c| c.as_str()),
            HalRange::from_v4l2(v).map(|c| c.as_str()),
            "quantization={v}"
        );
    }
}

#[test]
fn every_label_parses_in_hal() {
    use edgefirst_tensor::{
        ColorEncoding as HalEncoding, ColorRange as HalRange, ColorSpace as HalSpace,
        ColorTransfer as HalTransfer,
    };
    use videostream::colorimetry::{ColorEncoding, ColorRange, ColorSpace, ColorTransfer};

    for v in [
        ColorSpace::Bt709,
        ColorSpace::Bt2020,
        ColorSpace::Srgb,
        ColorSpace::Smpte170m,
    ] {
        assert_eq!(
            HalSpace::from_str_code(v.as_str()).map(|h| h.as_str()),
            Some(v.as_str())
        );
    }
    for v in [
        ColorTransfer::Bt709,
        ColorTransfer::Srgb,
        ColorTransfer::Pq,
        ColorTransfer::Hlg,
        ColorTransfer::Linear,
    ] {
        assert_eq!(
            HalTransfer::from_str_code(v.as_str()).map(|h| h.as_str()),
            Some(v.as_str())
        );
    }
    for v in [
        ColorEncoding::Bt601,
        ColorEncoding::Bt709,
        ColorEncoding::Bt2020,
    ] {
        assert_eq!(
            HalEncoding::from_str_code(v.as_str()).map(|h| h.as_str()),
            Some(v.as_str())
        );
    }
    for v in [ColorRange::Full, ColorRange::Limited] {
        assert_eq!(
            HalRange::from_str_code(v.as_str()).map(|h| h.as_str()),
            Some(v.as_str())
        );
    }
}
