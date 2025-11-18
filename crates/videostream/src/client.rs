// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{frame::Frame, vsl, Error};
use std::{
    ffi::{CStr, CString},
    io,
    path::PathBuf,
};
use videostream_sys as ffi;

pub struct Client {
    ptr: *mut ffi::VSLClient,
}

unsafe impl Send for Client {}
unsafe impl Sync for Client {}

impl Client {
    pub fn new(path: &str, reconnect: bool) -> Result<Self, Error> {
        let path_str_c = CString::new(path)?;
        let ptr =
            vsl!(vsl_client_init(path_str_c.as_ptr(), std::ptr::null_mut(), reconnect));
        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(Client { ptr })
    }

    pub fn release(&self) -> Result<(), Error> {
        vsl!(vsl_client_release(self.ptr));
        Ok(())
    }

    pub fn disconnect(&self) -> Result<(), Error> {
        vsl!(vsl_client_disconnect(self.ptr));
        Ok(())
    }

    pub fn userptr() {
        panic!("CURRENTLY NOT USED");
    }

    pub fn path(&self) -> Result<PathBuf, Error> {
        let path_ptr = vsl!(vsl_client_path(self.ptr));
        if path_ptr.is_null() {
            return Err(Error::NullPointer);
        }
        unsafe {
            let path_ref = CStr::from_ptr(path_ptr).to_str()?;
            return Ok(PathBuf::from(path_ref));
        }
    }

    pub fn set_timeout(&self, timeout: f32) -> Result<(), Error> {
        vsl!(vsl_client_set_timeout(self.ptr, timeout));
        Ok(())
    }

    pub fn get_frame(&self, until: i64) -> Result<Frame, Error> {
        let frame = vsl!(vsl_frame_wait(self.ptr, until));
        if frame.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(Frame::wrap(frame).unwrap())
    }
}

impl Drop for Client {
    fn drop(&mut self) {
        let _ = self.release();
        let _ = self.disconnect();
    }
}
