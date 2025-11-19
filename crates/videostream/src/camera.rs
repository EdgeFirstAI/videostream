// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{fourcc::FourCC, vsl, Error};
use dma_buf::DmaBuf;
use std::{
    ffi::{c_int, CString},
    fmt, io,
    os::fd::{BorrowedFd, FromRawFd, RawFd},
};
use unix_ts::Timestamp;
use videostream_sys as ffi;

type CameraFormats = Vec<FourCC>;

#[derive(Debug, Clone, Copy, Default)]
pub enum Mirror {
    #[default]
    None,
    Horizontal,
    Vertical,
    Both,
}

impl fmt::Display for Mirror {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Mirror::None => write!(f, "none"),
            Mirror::Horizontal => write!(f, "horizontal"),
            Mirror::Vertical => write!(f, "vertical"),
            Mirror::Both => write!(f, "both"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Camera {
    /// video device file for the camera
    device: String,

    /// request camera width, actual camera width may be different
    width: i32,

    /// request camera height, actual camera height may be different
    height: i32,

    /// request camera pixel format, actual format may be different
    format: FourCC,

    /// request mirroring mode, default is none.
    mirror: Mirror,

    /// number of camera buffers to create
    num_buffers: i32,
}

impl Camera {
    pub fn with_device(self, device: &str) -> Camera {
        Camera {
            device: device.to_owned(),
            width: self.width,
            height: self.height,
            format: self.format,
            mirror: self.mirror,
            num_buffers: self.num_buffers,
        }
    }

    pub fn with_resolution(self, width: i32, height: i32) -> Camera {
        Camera {
            device: self.device,
            width,
            height,
            format: self.format,
            mirror: self.mirror,
            num_buffers: self.num_buffers,
        }
    }

    pub fn with_format(self, format: FourCC) -> Camera {
        Camera {
            device: self.device,
            width: self.width,
            height: self.height,
            format,
            mirror: self.mirror,
            num_buffers: self.num_buffers,
        }
    }

    pub fn with_mirror(self, mirror: Mirror) -> Camera {
        Camera {
            device: self.device,
            width: self.width,
            height: self.height,
            format: self.format,
            mirror,
            num_buffers: self.num_buffers,
        }
    }

    pub fn with_buffers(self, num_buffers: i32) -> Camera {
        Camera {
            device: self.device,
            width: self.width,
            height: self.height,
            format: self.format,
            mirror: self.mirror,
            num_buffers,
        }
    }

    pub fn open(self) -> Result<CameraReader, Error> {
        CameraReader::init(self)
    }

    pub fn formats(self) -> Result<CameraFormats, Error> {
        let device_str_c = CString::new(self.device)?;
        let ptr = vsl!(vsl_camera_open_device(device_str_c.as_ptr()));
        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        const MAX_FORMATS: usize = 20;
        let mut formats: [u32; MAX_FORMATS] = [0; MAX_FORMATS];
        let n_formats = match c_int::try_from(MAX_FORMATS) {
            Ok(val) => val,
            Err(err) => {
                vsl!(vsl_camera_close_device(ptr));
                return Err(err.into());
            }
        };

        let cnt = vsl!(vsl_camera_enum_fmts(ptr, formats.as_mut_ptr(), n_formats));
        vsl!(vsl_camera_close_device(ptr));

        let u_cnt = usize::try_from(cnt)?;
        let mut fmts: CameraFormats = CameraFormats::with_capacity(u_cnt);

        for i in 0..cnt {
            let idx = usize::try_from(i)?;
            fmts.push(FourCC::from(formats[idx]));
        }

        Ok(fmts)
    }
}

impl Default for Camera {
    fn default() -> Camera {
        Camera {
            device: "/dev/video0".to_owned(),
            width: 1920,
            height: 1080,
            format: FourCC(*b"YUYV"),
            mirror: Mirror::None,
            num_buffers: 4,
        }
    }
}

pub fn create_camera() -> Camera {
    Camera::default()
}

#[derive(Debug)]
pub struct CameraReader {
    ptr: *mut ffi::vsl_camera,
    width: i32,
    height: i32,
    format: FourCC,
    mirror: Mirror,
}

impl CameraReader {
    fn init(camera: Camera) -> Result<Self, Error> {
        let device_str_c = CString::new(camera.device)?;
        let ptr = vsl!(vsl_camera_open_device(device_str_c.as_ptr()));
        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        let mut width: c_int = camera.width;
        let mut height: c_int = camera.height;
        let mut num_buffers: c_int = camera.num_buffers;
        let mut format: u32 = camera.format.into();

        if vsl!(vsl_camera_init_device(
            ptr,
            &mut width,
            &mut height,
            &mut num_buffers,
            &mut format
        )) != 0
        {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        let cam = CameraReader {
            ptr,
            width,
            height,
            format: FourCC::from(format),
            mirror: camera.mirror,
        };

        match cam.mirror {
            Mirror::None => {
                cam.set_mirror_h(false)?;
                cam.set_mirror_v(false)?;
            }
            Mirror::Horizontal => {
                cam.set_mirror_h(true)?;
                cam.set_mirror_v(false)?;
            }
            Mirror::Vertical => {
                cam.set_mirror_h(false)?;
                cam.set_mirror_v(true)?;
            }
            Mirror::Both => {
                cam.set_mirror_h(true)?;
                cam.set_mirror_v(true)?;
            }
        }

        Ok(cam)
    }

    pub fn start(&self) -> Result<(), Error> {
        if vsl!(vsl_camera_start_capturing(self.ptr)) != 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(())
    }

    pub fn stop(&self) -> Result<(), Error> {
        if vsl!(vsl_camera_stop_capturing(self.ptr)) != 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(())
    }

    fn set_mirror_h(&self, enable: bool) -> Result<(), Error> {
        if vsl!(vsl_camera_mirror(self.ptr, enable)) != 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(())
    }

    fn set_mirror_v(&self, enable: bool) -> Result<(), Error> {
        if vsl!(vsl_camera_mirror_v(self.ptr, enable)) != 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(())
    }

    pub fn set_mirror(&mut self, mirror: Mirror) -> Result<(), Error> {
        match mirror {
            Mirror::None => {
                self.set_mirror_h(false)?;
                self.set_mirror_v(false)?;
            }
            Mirror::Horizontal => {
                self.set_mirror_h(true)?;
                self.set_mirror_v(false)?;
            }
            Mirror::Vertical => {
                self.set_mirror_h(false)?;
                self.set_mirror_v(true)?;
            }
            Mirror::Both => {
                self.set_mirror_h(true)?;
                self.set_mirror_v(true)?;
            }
        }

        self.mirror = mirror;

        Ok(())
    }

    pub fn mirror(&self) -> Mirror {
        self.mirror
    }

    pub fn width(&self) -> i32 {
        self.width
    }

    pub fn height(&self) -> i32 {
        self.height
    }

    pub fn format(&self) -> FourCC {
        self.format
    }

    pub fn read(&self) -> Result<CameraBuffer<'_>, Error> {
        let ptr = vsl!(vsl_camera_get_data(self.ptr));
        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        CameraBuffer::new(ptr, self)
    }
}

impl Drop for CameraReader {
    fn drop(&mut self) {
        let _ = self.stop();
        if let Ok(lib) = ffi::init() {
            unsafe {
                lib.vsl_camera_uninit_device(self.ptr);
                lib.vsl_camera_close_device(self.ptr);
            }
        }
    }
}

#[derive(Debug)]
pub struct CameraBuffer<'a> {
    raw_fd: RawFd,
    ptr: *mut ffi::vsl_camera_buffer,
    parent: &'a CameraReader,
}

impl CameraBuffer<'_> {
    fn new(
        ptr: *mut ffi::vsl_camera_buffer,
        parent: &CameraReader,
    ) -> Result<CameraBuffer<'_>, Error> {
        let original_fd: RawFd = vsl!(vsl_camera_buffer_dma_fd(ptr));
        Ok(CameraBuffer {
            raw_fd: original_fd,
            ptr,
            parent,
        })
    }

    pub fn fd(&self) -> BorrowedFd<'_> {
        // SAFETY: the raw_fd allocated by v4l2 will stay valid until the CameraReader
        // is closed. The camerabuffer lifetime is at most the same as the
        // CameraReader, so this borrow is safe
        unsafe { BorrowedFd::borrow_raw(self.raw_fd) }
    }

    pub fn dmabuf(&self) -> DmaBuf {
        unsafe { DmaBuf::from_raw_fd(self.raw_fd) }
    }

    pub fn rawfd(&self) -> RawFd {
        self.raw_fd
    }

    pub fn length(&self) -> Result<usize, Error> {
        Ok(usize::try_from(vsl!(vsl_camera_buffer_length(self.ptr))).unwrap_or(0))
    }

    pub fn width(&self) -> i32 {
        self.parent.width()
    }

    pub fn height(&self) -> i32 {
        self.parent.height()
    }

    pub fn format(&self) -> FourCC {
        self.parent.format()
    }

    pub fn timestamp(&self) -> Result<Timestamp, Error> {
        let mut sec: i64 = 0;
        let mut ns: i64 = 0;
        vsl!(vsl_camera_buffer_timestamp(self.ptr, &mut sec, &mut ns));

        Ok(Timestamp::new(sec, ns as u32))
    }
}

impl Drop for CameraBuffer<'_> {
    fn drop(&mut self) {
        if let Ok(lib) = ffi::init() {
            let _ = unsafe { lib.vsl_camera_release_buffer(self.parent.ptr, self.ptr) };
        }
    }
}

impl fmt::Display for CameraBuffer<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{}x{} {} fd:{:?} ptr:{:?}",
            self.width(),
            self.height(),
            self.format(),
            self.rawfd(),
            self.ptr
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serial_test::serial;
    use std::time::Instant;

    #[ignore = "test requires maivin 2 hardware (run with --include-ignored to enable)"]
    #[test]
    #[serial]
    fn test_formats() -> Result<(), Error> {
        let device = "/dev/video3";

        let fmts = create_camera().with_device(device).formats()?;
        println!("camera formats: {:?}", fmts);
        assert_ne!(fmts.len(), 0);

        Ok(())
    }

    #[ignore = "test requires maivin 2 hardware (run with --include-ignored to enable)"]
    #[test]
    #[serial]
    fn test_resolutions() -> Result<(), Error> {
        let device = "/dev/video3";

        let cam = create_camera()
            .with_device(device)
            .with_resolution(640, 480)
            .open()?;
        println!(
            "camera resolution {}x{} format {} mirrored {}",
            cam.width(),
            cam.height(),
            cam.format(),
            cam.mirror()
        );
        assert_eq!(cam.width(), 640);
        assert_eq!(cam.height(), 480);

        Ok(())
    }

    #[ignore = "test requires maivin 2 hardware (run with --include-ignored to enable)"]
    #[test]
    #[serial]
    fn test_capture() -> Result<(), Error> {
        let device = "/dev/video3";

        let cam = create_camera()
            .with_device(device)
            .with_format(FourCC(*b"YUYV"))
            .open()?;
        println!(
            "camera resolution {}x{} format {} mirrored {}",
            cam.width(),
            cam.height(),
            cam.format(),
            cam.mirror(),
        );

        cam.start()?;

        for _ in 0..100 {
            let buf = cam.read()?;

            let now = Instant::now();
            let dma = buf.dmabuf();
            let mem = dma
                .memory_map()
                .map_err(|e| Error::Io(io::Error::new(io::ErrorKind::Other, format!("DMA map error: {}", e))))?;
            let stats = mem
                .read(pixel_metrics_boxed, Some((buf.width(), buf.height())))
                .map_err(|e| Error::Io(io::Error::new(io::ErrorKind::Other, format!("DMA read error: {}", e))))?;
            let elapsed = now.elapsed();

            println!(
                "camera y-component min {} max {} avg {} [elapsed: {:.2?}]",
                stats.0, stats.1, stats.2, elapsed
            );
        }

        Ok(())
    }

    fn pixel_metrics_boxed(
        img: &[u8],
        dim: Option<(i32, i32)>,
    ) -> Result<(u8, u8, u8), Box<dyn std::error::Error>> {
        pixel_metrics(img, dim).map_err(|e| Box::new(e) as Box<dyn std::error::Error>)
    }

    fn pixel_metrics(img: &[u8], dim: Option<(i32, i32)>) -> Result<(u8, u8, u8), Error> {
        let width = dim.unwrap_or_default().0;
        let height = dim.unwrap_or_default().1;

        let mut y_min = 255;
        let mut y_max = 0;
        let mut y_avg = 0;

        for y in 0..height {
            for x in (0..width).step_by(2) {
                let y = img[(y * width + x) as usize];
                if y < y_min {
                    y_min = y;
                }
                if y > y_max {
                    y_max = y;
                }
                y_avg += y as i32;
            }
        }

        y_avg /= width * height / 2;

        Ok((y_min, y_max, y_avg as u8))
    }
}
