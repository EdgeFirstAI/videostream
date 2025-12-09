// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies
//
// Taken from https://docs.rs/crate/four-cc/latest and adapted to handle endianess.
#![forbid(unsafe_code)]

use core::{fmt, result::Result};

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
#[repr(C, packed)]
pub struct FourCC(pub [u8; 4]);

impl FourCC {
    const fn to_u32(self) -> u32 {
        #[cfg(target_endian = "little")]
        {
            ((self.0[3] as u32) << 24 & 0xff000000)
                | ((self.0[2] as u32) << 16 & 0x00ff0000)
                | ((self.0[1] as u32) << 8 & 0x0000ff00)
                | ((self.0[0] as u32) & 0x000000ff)
        }
        #[cfg(target_endian = "big")]
        {
            ((self.0[0] as u32) << 24 & 0xff000000)
                | ((self.0[1] as u32) << 16 & 0x00ff0000)
                | ((self.0[2] as u32) << 8 & 0x0000ff00)
                | ((self.0[3] as u32) & 0x000000ff)
        }
    }
}

impl From<&[u8; 4]> for FourCC {
    fn from(buf: &[u8; 4]) -> FourCC {
        FourCC([buf[0], buf[1], buf[2], buf[3]])
    }
}
impl From<&[u8]> for FourCC {
    fn from(buf: &[u8]) -> FourCC {
        FourCC([buf[0], buf[1], buf[2], buf[3]])
    }
}
impl From<u32> for FourCC {
    fn from(val: u32) -> FourCC {
        #[cfg(target_endian = "little")]
        {
            FourCC([
                (val & 0xff) as u8,
                (val >> 8 & 0xff) as u8,
                (val >> 16 & 0xff) as u8,
                (val >> 24 & 0xff) as u8,
            ])
        }
        #[cfg(target_endian = "big")]
        {
            FourCC([
                (val >> 24 & 0xff) as u8,
                (val >> 16 & 0xff) as u8,
                (val >> 8 & 0xff) as u8,
                (val & 0xff) as u8,
            ])
        }
    }
}

impl From<FourCC> for u32 {
    fn from(val: FourCC) -> Self {
        val.to_u32()
    }
}

impl fmt::Display for FourCC {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        match core::str::from_utf8(&self.0) {
            Ok(s) => f.write_str(s),
            Err(_) => {
                // If we return fmt::Error, then for example format!() will panic, so we choose
                // an alternative representation instead
                let b = &self.0;
                f.write_fmt(format_args!(
                    "{}{}{}{}",
                    core::ascii::escape_default(b[0]),
                    core::ascii::escape_default(b[1]),
                    core::ascii::escape_default(b[2]),
                    core::ascii::escape_default(b[3])
                ))
            }
        }
    }
}

impl fmt::Debug for FourCC {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        let b = self.0;
        f.debug_tuple("FourCC")
            .field(&format_args!(
                "{}{}{}{}",
                core::ascii::escape_default(b[0]),
                core::ascii::escape_default(b[1]),
                core::ascii::escape_default(b[2]),
                core::ascii::escape_default(b[3])
            ))
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fourcc_from_bytes() {
        let fourcc = FourCC(*b"YUYV");
        assert_eq!(fourcc.0, [b'Y', b'U', b'Y', b'V']);
    }

    #[test]
    fn test_fourcc_from_byte_slice() {
        let bytes: &[u8; 4] = b"RGB3";
        let fourcc = FourCC::from(bytes);
        assert_eq!(fourcc.0, *b"RGB3");
    }

    #[test]
    fn test_fourcc_from_u32() {
        // YUYV = 0x56595559 in little-endian
        let fourcc = FourCC::from(0x56595559u32);
        assert_eq!(fourcc.to_string(), "YUYV");
    }

    #[test]
    fn test_fourcc_to_u32() {
        let fourcc = FourCC(*b"YUYV");
        let val: u32 = fourcc.into();
        assert_eq!(val, 0x56595559);
    }

    #[test]
    fn test_fourcc_roundtrip() {
        let original = FourCC(*b"NV12");
        let as_u32: u32 = original.into();
        let back = FourCC::from(as_u32);
        assert_eq!(original, back);
    }

    #[test]
    fn test_fourcc_display() {
        let fourcc = FourCC(*b"H264");
        assert_eq!(format!("{}", fourcc), "H264");
    }

    #[test]
    fn test_fourcc_debug() {
        let fourcc = FourCC(*b"HEVC");
        let debug_str = format!("{:?}", fourcc);
        assert!(debug_str.contains("HEVC"));
    }

    #[test]
    fn test_fourcc_equality() {
        let a = FourCC(*b"YUYV");
        let b = FourCC(*b"YUYV");
        let c = FourCC(*b"NV12");
        assert_eq!(a, b);
        assert_ne!(a, c);
    }

    #[test]
    fn test_fourcc_hash() {
        use std::collections::HashSet;
        let mut set = HashSet::new();
        set.insert(FourCC(*b"YUYV"));
        set.insert(FourCC(*b"NV12"));
        set.insert(FourCC(*b"YUYV")); // Duplicate
        assert_eq!(set.len(), 2);
    }

    #[test]
    fn test_fourcc_non_ascii_display() {
        // Test that non-ASCII bytes are handled properly
        let fourcc = FourCC([0x00, 0x01, 0x02, 0x03]);
        let display = format!("{}", fourcc);
        // The display should contain escape sequences for non-printable chars
        // ascii::escape_default uses \x00 format
        assert!(
            !display.is_empty(),
            "Display should produce output for non-ASCII"
        );
    }

    #[test]
    fn test_common_formats() {
        // Test common video formats
        let formats = [
            ("YUYV", 0x56595559u32),
            ("NV12", 0x3231564eu32),
            ("RGB3", 0x33424752u32),
            ("GREY", 0x59455247u32),
        ];

        for (name, expected_val) in formats {
            let fourcc = FourCC(*name.as_bytes().first_chunk::<4>().unwrap());
            let val: u32 = fourcc.into();
            assert_eq!(val, expected_val, "FourCC {} value mismatch", name);
        }
    }

    #[test]
    fn test_fourcc_from_slice() {
        let bytes: &[u8] = b"MJPG";
        let fourcc = FourCC::from(bytes);
        assert_eq!(fourcc.0, *b"MJPG");
    }

    #[test]
    fn test_fourcc_clone() {
        let original = FourCC(*b"YUYV");
        let cloned = original;
        assert_eq!(original, cloned);
    }

    #[test]
    fn test_fourcc_copy() {
        let original = FourCC(*b"NV12");
        let copied = original;
        // Both should be equal and independent
        assert_eq!(original, copied);
    }
}
