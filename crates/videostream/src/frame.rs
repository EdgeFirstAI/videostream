// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{camera::CameraBuffer, client, vsl, Error};
use std::{
    ffi::{CStr, CString},
    io,
    os::fd::{AsRawFd, RawFd},
    path::Path,
    ptr, slice,
};
use videostream_sys as ffi;

/// The Frame structure handles the frame and underlying framebuffer.  A frame
/// can be an image or a single video frame, the distinction is not considered.
///
/// A frame can be created and used as a free-standing frame, which means it is
/// not published through a Host nor was it created from a receiving Client. A
/// free-standing frame can be mapped and copied to other frames which provides
/// an optimized method for resizing or converting between formats.
pub struct Frame {
    ptr: *mut ffi::VSLFrame,
}

unsafe impl Send for Frame {}

impl Frame {
    pub fn new(width: u32, height: u32, stride: u32, fourcc_str: &str) -> Result<Self, Error> {
        let buf = fourcc_str.as_bytes();
        if buf.len() != 4 {
            return Err(Error::NullPointer); // Invalid fourcc is a library error
        }
        let mut fourcc: u32 = 0;
        for (i, &byte) in buf.iter().enumerate() {
            fourcc += (byte as u32) << (i * 8);
        }

        let ptr = vsl!(vsl_frame_init(
            width,
            height,
            stride,
            fourcc,
            std::ptr::null_mut(),
            None
        ));

        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(Error::Io(err));
        }
        Ok(Frame { ptr })
    }

    pub fn alloc(&self, path: Option<&Path>) -> Result<(), Error> {
        let path_ptr;
        if let Some(path) = path {
            let path = path.to_str().unwrap();
            let path = CString::new(path).unwrap();
            path_ptr = path.into_raw();
        } else {
            path_ptr = ptr::null_mut();
        }
        let ret = vsl!(vsl_frame_alloc(self.ptr, path_ptr)) as i32;
        if ret != 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    #[allow(clippy::result_unit_err)]
    pub fn wrap(ptr: *mut ffi::VSLFrame) -> Result<Self, ()> {
        if ptr.is_null() {
            return Err(());
        }

        Ok(Frame { ptr })
    }

    pub fn wait(client: &client::Client, until: i64) -> Result<Self, Error> {
        let wrapper = client.get_frame(until)?;
        Ok(Frame { ptr: wrapper.ptr })
    }

    pub fn trylock(&self) -> Result<(), Error> {
        let ret = vsl!(vsl_frame_trylock(self.ptr));
        if ret != 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    pub fn unlock(&self) -> Result<(), Error> {
        if vsl!(vsl_frame_unlock(self.ptr)) as i32 == -1 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    pub fn sync(&self, enable: bool, mode: i32) -> Result<(), Error> {
        let ret = vsl!(vsl_frame_sync(self.ptr, enable as i32, mode));
        if ret >= 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    pub fn serial(&self) -> Result<i64, Error> {
        Ok(vsl!(vsl_frame_serial(self.ptr)))
    }

    pub fn timestamp(&self) -> Result<i64, Error> {
        Ok(vsl!(vsl_frame_timestamp(self.ptr)))
    }

    pub fn duration(&self) -> Result<i64, Error> {
        Ok(vsl!(vsl_frame_duration(self.ptr)))
    }

    pub fn pts(&self) -> Result<i64, Error> {
        Ok(vsl!(vsl_frame_pts(self.ptr)))
    }

    pub fn dts(&self) -> Result<i64, Error> {
        Ok(vsl!(vsl_frame_dts(self.ptr)))
    }

    pub fn expires(&self) -> Result<i64, Error> {
        Ok(vsl!(vsl_frame_expires(self.ptr)))
    }

    pub fn fourcc(&self) -> Result<u32, Error> {
        Ok(vsl!(vsl_frame_fourcc(self.ptr)))
    }

    pub fn width(&self) -> Result<i32, Error> {
        let width: std::os::raw::c_int = vsl!(vsl_frame_width(self.ptr));
        Ok(width as i32)
    }

    pub fn height(&self) -> Result<i32, Error> {
        let height: std::os::raw::c_int = vsl!(vsl_frame_height(self.ptr));
        Ok(height as i32)
    }

    pub fn size(&self) -> Result<i32, Error> {
        Ok(vsl!(vsl_frame_size(self.ptr)) as i32)
    }

    /// Returns the stride in bytes of the video frame.
    ///
    /// Stride is the number of bytes from the start of one row to the next.
    /// May be larger than width*bytes_per_pixel due to alignment requirements.
    ///
    /// # Returns
    ///
    /// Returns the row stride in bytes.
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::frame::Frame;
    ///
    /// let frame = Frame::new(1920, 1080, 0, "YUYV")?;
    /// println!("Stride: {} bytes", frame.stride()?);
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn stride(&self) -> Result<i32, Error> {
        Ok(vsl!(vsl_frame_stride(self.ptr)) as i32)
    }

    pub fn handle(&self) -> Result<i32, Error> {
        let handle: std::os::raw::c_int = vsl!(vsl_frame_handle(self.ptr));
        Ok(handle as i32)
    }

    pub fn paddr(&self) -> Result<Option<isize>, Error> {
        let ret = vsl!(vsl_frame_paddr(self.ptr));
        if ret == -1 {
            return Ok(None);
        }
        Ok(Some(ret))
    }

    pub fn path(&self) -> Result<Option<&str>, Error> {
        let ret = vsl!(vsl_frame_path(self.ptr));
        if ret.is_null() {
            return Ok(None);
        }
        let c_str = unsafe { CStr::from_ptr(ret) };
        Ok(Some(c_str.to_str().unwrap_or("unknown")))
    }

    #[allow(clippy::result_unit_err)]
    pub fn mmap(&self) -> Result<&[u8], Error> {
        let ptr = vsl!(vsl_frame_mmap(self.ptr, std::ptr::null_mut::<usize>()));
        let size = self.size()?;
        if ptr.is_null() || size == 0 {
            return Err(Error::NullPointer);
        }
        Ok(unsafe { slice::from_raw_parts(ptr as *const u8, size as usize) })
    }

    /// # Safety
    /// This function returns a mutable reference from an immutable `&self`
    /// reference. This is safe because the underlying memory is
    /// memory-mapped and managed by the FFI layer. Callers must ensure they
    /// follow proper synchronization patterns when accessing the mmap'd
    /// memory from multiple threads.
    #[allow(clippy::result_unit_err)]
    #[allow(clippy::mut_from_ref)]
    pub fn mmap_mut(&self) -> Result<&mut [u8], Error> {
        let mut size: usize = 0;
        let ptr = vsl!(vsl_frame_mmap(self.ptr, &mut size as *mut usize));
        if ptr.is_null() || size == 0 {
            return Err(Error::NullPointer);
        }
        Ok(unsafe { slice::from_raw_parts_mut(ptr as *mut u8, size) })
    }

    pub fn munmap(&self) -> Result<(), Error> {
        vsl!(vsl_frame_munmap(self.ptr));
        Ok(())
    }

    pub fn attach(&self, fd: RawFd, size: usize, offset: usize) -> Result<(), Error> {
        let ret = vsl!(vsl_frame_attach(self.ptr, fd, size, offset));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    /// Returns the user pointer associated with this frame.
    ///
    /// # Returns
    ///
    /// Returns the user pointer provided to [`Frame::new`], or `None` if none was set.
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Safety
    ///
    /// The returned pointer is a raw void pointer. The caller is responsible for
    /// ensuring the pointer is valid and properly cast to the correct type.
    pub fn userptr(&self) -> Result<Option<*mut std::os::raw::c_void>, Error> {
        let ptr = vsl!(vsl_frame_userptr(self.ptr));
        if ptr.is_null() {
            Ok(None)
        } else {
            Ok(Some(ptr))
        }
    }

    /// Associates a user pointer with this frame.
    ///
    /// Sets or updates the user data pointer for this frame. This can be used to
    /// attach arbitrary application data to a frame.
    ///
    /// # Arguments
    ///
    /// * `userptr` - User data pointer to associate with frame
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Safety
    ///
    /// The caller must ensure the pointer remains valid for the lifetime of the frame.
    /// The pointer will not be dereferenced by this library, but is stored and can be
    /// retrieved later via `userptr()`.
    pub unsafe fn set_userptr(&self, userptr: *mut std::os::raw::c_void) -> Result<(), Error> {
        vsl!(vsl_frame_set_userptr(self.ptr, userptr));
        Ok(())
    }

    /// Frees the allocated buffer for this frame.
    ///
    /// Releases the underlying memory (DmaBuf or shared memory) but does not
    /// destroy the frame object. Use [`Drop`] to destroy the frame.
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::frame::Frame;
    ///
    /// let frame = Frame::new(1920, 1080, 0, "YUYV")?;
    /// frame.alloc(None)?;
    /// // Use frame...
    /// frame.unalloc()?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn unalloc(&self) -> Result<(), Error> {
        vsl!(vsl_frame_unalloc(self.ptr));
        Ok(())
    }

    /// Copies this frame into the target frame with optional format conversion and cropping.
    ///
    /// Handles format conversion, rescaling, and cropping using hardware
    /// acceleration when available (G2D on i.MX8). Both frames can be host or client
    /// frames. Automatically locks frames during copy (safe for free-standing frames
    /// too).
    ///
    /// Copy sequence: 1) Crop source, 2) Convert format, 3) Scale to target size.
    ///
    /// # Arguments
    ///
    /// * `target` - Destination frame (receives copied data)
    /// * `crop` - Optional crop region in source coordinates (None for full frame)
    ///
    /// # Returns
    ///
    /// Returns the number of bytes copied on success.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if the copy operation fails.
    ///
    /// # Warning
    ///
    /// Copying to/from a posted frame may cause visual tearing.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::frame::Frame;
    ///
    /// let source = Frame::new(1920, 1080, 0, "YUYV")?;
    /// source.alloc(None)?;
    ///
    /// let target = Frame::new(640, 480, 0, "RGB3")?;
    /// target.alloc(None)?;
    ///
    /// let bytes = source.copy_to(&target, None)?;
    /// println!("Copied {} bytes", bytes);
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn copy_to(&self, target: &Frame, crop: Option<&ffi::VSLRect>) -> Result<i32, Error> {
        let crop_ptr = crop.map_or(std::ptr::null(), |c| c as *const ffi::VSLRect);
        let ret = vsl!(vsl_frame_copy(target.ptr, self.ptr, crop_ptr));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(ret)
    }

    pub fn get_ptr(&self) -> *mut ffi::VSLFrame {
        self.ptr
    }
}

impl TryFrom<*mut ffi::VSLFrame> for Frame {
    type Error = ();

    fn try_from(ptr: *mut ffi::VSLFrame) -> Result<Self, Self::Error> {
        if ptr.is_null() {
            return Err(());
        }
        Ok(Frame { ptr })
    }
}
impl TryFrom<&CameraBuffer<'_>> for Frame {
    type Error = Error;

    fn try_from(buf: &CameraBuffer<'_>) -> Result<Self, Self::Error> {
        let frame = Frame::new(
            buf.width().try_into().unwrap(),
            buf.height().try_into().unwrap(),
            0,
            buf.format().to_string().as_str(),
        )?;
        match frame.attach(buf.fd().as_raw_fd(), 0, 0) {
            Ok(_) => (),
            Err(e) => return Err(e),
        }
        Ok(frame)
    }
}

impl Drop for Frame {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            if let Ok(lib) = ffi::init() {
                unsafe {
                    lib.vsl_frame_release(self.ptr);
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use rand::{self, Rng};
    use std::{
        fs::{self, File},
        io::Write,
        os::fd::AsRawFd,
    };

    #[test]
    fn frame() {
        //let fourcc = 0x33424752 as u32; //Hex for RGB3
        let frame = Frame::new(640, 480, 0, "RGB3").unwrap();

        assert_eq!(frame.width().unwrap(), 640);
        assert_eq!(frame.height().unwrap(), 480);
        assert_eq!(frame.fourcc().unwrap(), 0x33424752);
        assert_eq!(frame.path().unwrap(), None);

        frame.alloc(None).unwrap();

        assert_eq!(frame.size().unwrap(), 640 * 480 * 3);

        let mem: &mut [u8] = frame.mmap_mut().unwrap();
        let mut rng = rand::rng();
        for elem in &mut *mem {
            let num: u8 = rng.random();
            *elem = num;
        }
        let mem2 = frame.mmap().unwrap();
        for i in 0..mem.len() {
            assert_eq!(mem[i], mem2[i]);
        }

        let frame2 = Frame::new(640, 480, 0, "RGB3").unwrap();
        frame2
            .attach(frame.handle().unwrap(), frame.size().unwrap() as usize, 0)
            .unwrap();
        let v2: &mut [u8] = frame2.mmap_mut().unwrap();
        for i in 0..mem.len() {
            assert_eq!(mem[i], v2[i]);
        }

        for elem in &mut *v2 {
            let num: u8 = rng.random();
            *elem = num;
        }
        assert_eq!(mem[0], v2[0]);
        assert_eq!(mem2[0], v2[0]);
    }

    #[test]
    fn attach_file() {
        let frame = Frame::new(640, 480, 0, "RGB3").unwrap();

        let mut expect = Vec::new();
        let mut rng = rand::rng();
        for _ in 0..(frame.height().unwrap() * frame.width().unwrap() * 3) {
            expect.push(rng.random::<u8>() as u8);
        }
        let mut file = File::options()
            .read(true)
            .write(true)
            .create(true)
            .truncate(true)
            .open("./temp.txt")
            .unwrap();
        file.write_all(&expect).unwrap();

        frame
            .attach(
                file.as_raw_fd(),
                (frame.height().unwrap() * frame.width().unwrap() * 3) as usize,
                0,
            )
            .unwrap();

        let mem = frame.mmap().unwrap();

        for i in 0..mem.len() {
            assert_eq!(mem[i], expect[i])
        }
        if fs::remove_file("./temp.txt").is_err() {
            panic!("Test succeeded but file \"./temp.txt\" was not deleted");
        }
    }

    #[test]
    fn bad_attach() {
        let frame = Frame::new(640, 480, 0, "RGB3").unwrap();

        if frame.attach(-1, 1_usize, 0).is_ok() {
            panic!("Failed")
        };

        if frame.attach(9000, 1_usize, 0).is_ok() {
            panic!("Failed")
        };
    }

    #[test]
    fn fourcc() {}

    #[test]
    fn bad_fourcc() {}

    #[test]
    fn nodma() {}

    #[test]
    fn invalid_shm_name() {}
}
