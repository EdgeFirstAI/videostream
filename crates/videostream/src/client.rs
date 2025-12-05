// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{frame::Frame, vsl, Error};
use std::{
    ffi::{CStr, CString},
    io,
    path::PathBuf,
};
use videostream_sys as ffi;

/// Client structure for connecting to a VideoStream host.
///
/// Provides functionality to subscribe to video frames published by a
/// [`crate::host::Host`].
///
/// # Examples
///
/// ```no_run
/// use videostream::client::Client;
///
/// let client = Client::new("/tmp/video.sock", true)?;
/// println!("Connected to: {:?}", client.path()?);
/// # Ok::<(), videostream::Error>(())
/// ```
pub struct Client {
    ptr: *mut ffi::VSLClient,
}

unsafe impl Send for Client {}
unsafe impl Sync for Client {}

impl std::fmt::Debug for Client {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let path = self
            .path()
            .unwrap_or_else(|_| PathBuf::from("<invalid_path>"));
        f.debug_struct("Client").field("path", &path).finish()
    }
}

impl Client {
    pub fn new(path: &str, reconnect: bool) -> Result<Self, Error> {
        let path_str_c = CString::new(path)?;
        let ptr = vsl!(vsl_client_init(
            path_str_c.as_ptr(),
            std::ptr::null_mut(),
            reconnect
        ));
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

    #[deprecated(
        since = "2.0.0",
        note = "Use get_userptr() instead which returns Result"
    )]
    pub fn userptr(&self) {
        panic!("This method has been deprecated and will panic. Use get_userptr() which returns a Result<Option<_>, Error> instead.");
    }

    /// Returns the optional userptr associated with this client connection.
    ///
    /// # Returns
    ///
    /// Returns the user pointer associated with this client connection, or `None` if none was set.
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Safety
    ///
    /// The returned pointer is a raw void pointer. The caller is responsible for
    /// ensuring the pointer is valid and properly cast to the correct type.
    pub fn get_userptr(&self) -> Result<Option<*mut std::os::raw::c_void>, Error> {
        let ptr = vsl!(vsl_client_userptr(self.ptr));
        if ptr.is_null() {
            Ok(None)
        } else {
            Ok(Some(ptr))
        }
    }

    pub fn path(&self) -> Result<PathBuf, Error> {
        let path_ptr = vsl!(vsl_client_path(self.ptr));
        if path_ptr.is_null() {
            return Err(Error::NullPointer);
        }
        unsafe {
            let path_ref = CStr::from_ptr(path_ptr).to_str()?;
            Ok(PathBuf::from(path_ref))
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn test_client_debug() {
        let client = Client::new("/tmp/test_debug.vsl", true).unwrap();
        let debug_str = format!("{:?}", client);

        assert!(debug_str.contains("Client"));
        assert!(debug_str.contains("test_debug.vsl"));
    }

    #[test]
    fn test_client_get_userptr() {
        // Test 1: Client created with null userptr should return None
        let client_none = Client::new("/tmp/test_userptr_none.vsl", true).unwrap();
        let userptr_none = client_none.get_userptr().unwrap();
        assert_eq!(
            userptr_none, None,
            "Client with null userptr should return None"
        );

        // Test 2: Client created with a userptr should return Some(ptr)
        // Create a CString to use as user data
        let user_data = CString::new("test_user_data").unwrap();
        let user_data_ptr = user_data.as_ptr() as *mut std::os::raw::c_void;

        // Create client with userptr using FFI directly since Client::new doesn't expose it
        let path_str_c = CString::new("/tmp/test_userptr_some.vsl").unwrap();
        let lib = videostream_sys::init().unwrap();
        let ptr = unsafe { lib.vsl_client_init(path_str_c.as_ptr(), user_data_ptr, true) };
        assert!(!ptr.is_null(), "Client initialization should succeed");

        let client_some = Client { ptr };
        let userptr_some = client_some.get_userptr().unwrap();
        assert!(
            userptr_some.is_some(),
            "Client with userptr should return Some"
        );
        assert_eq!(
            userptr_some.unwrap(),
            user_data_ptr,
            "Userptr should match what was set"
        );

        // Keep user_data alive until client is dropped
        drop(client_some);
        drop(user_data);
    }
}
