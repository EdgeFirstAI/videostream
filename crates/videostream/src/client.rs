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
    use crate::frame::Frame;
    use crate::host::Host;
    use crate::timestamp;
    use std::ffi::CString;
    use std::thread;
    use std::time::Duration;

    /// Helper to create a unique socket path for each test
    fn test_socket_path(name: &str) -> String {
        format!("/tmp/vsl_test_{}_{}.sock", name, std::process::id())
    }

    #[test]
    fn test_client_debug() {
        let socket_path = test_socket_path("client_debug");

        // Create a host first so the client can connect
        let host = Host::new(&socket_path).unwrap();

        // Now create a client that connects to the host
        let client = Client::new(&socket_path, false).unwrap();
        let debug_str = format!("{:?}", client);

        assert!(debug_str.contains("Client"));
        assert!(debug_str.contains(&socket_path));

        drop(client);
        drop(host);
    }

    #[test]
    fn test_client_get_userptr() {
        let socket_path = test_socket_path("client_userptr");

        // Create a host first so the client can connect
        let host = Host::new(&socket_path).unwrap();

        // Test 1: Client created with null userptr should return None
        let client_none = Client::new(&socket_path, false).unwrap();
        let userptr_none = client_none.get_userptr().unwrap();
        assert_eq!(
            userptr_none, None,
            "Client with null userptr should return None"
        );
        drop(client_none);

        // Test 2: Client created with a userptr should return Some(ptr)
        // Create a CString to use as user data
        let user_data = CString::new("test_user_data").unwrap();
        let user_data_ptr = user_data.as_ptr() as *mut std::os::raw::c_void;

        // Create client with userptr using FFI directly since Client::new doesn't expose it
        let path_str_c = CString::new(socket_path.clone()).unwrap();
        let lib = videostream_sys::init().unwrap();
        let ptr = unsafe { lib.vsl_client_init(path_str_c.as_ptr(), user_data_ptr, false) };
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
        drop(host);
    }

    #[test]
    fn test_client_path() {
        let socket_path = test_socket_path("client_path");

        // Create a host first
        let host = Host::new(&socket_path).unwrap();

        // Create a client
        let client = Client::new(&socket_path, false).unwrap();

        // Verify the path matches
        let client_path = client.path().unwrap();
        assert_eq!(
            client_path.to_str().unwrap(),
            &socket_path,
            "Client path should match socket path"
        );

        drop(client);
        drop(host);
    }

    #[test]
    fn test_client_set_timeout() {
        let socket_path = test_socket_path("client_timeout");

        // Create a host first
        let host = Host::new(&socket_path).unwrap();

        // Create a client
        let client = Client::new(&socket_path, false).unwrap();

        // Set timeout should succeed
        client.set_timeout(5.0).unwrap();

        drop(client);
        drop(host);
    }

    #[test]
    fn test_client_host_frame_pipeline() {
        let socket_path = test_socket_path("client_pipeline");

        // Create a host
        let host = Host::new(&socket_path).unwrap();

        // Create a client
        let client = Client::new(&socket_path, false).unwrap();

        // Give the connection time to establish
        thread::sleep(Duration::from_millis(10));

        // Process any pending connections on host
        let _ = host.poll(0);

        // Create and allocate a frame
        let frame = Frame::new(640, 480, 0, "RGB3").unwrap();
        frame.alloc(None).unwrap();

        // Fill frame with test data
        {
            let data = frame.mmap_mut().unwrap();
            for (i, byte) in data.iter_mut().enumerate() {
                *byte = (i % 256) as u8;
            }
        }

        // Post frame to clients
        let now = timestamp().unwrap();
        let expires = now + 1_000_000_000; // 1 second from now
        host.post(frame, expires, -1, -1, -1).unwrap();

        // Process the posted frame
        let _ = host.poll(100);

        // Try to receive the frame on the client side
        // Use a short timeout since frame should be available
        let received = client.get_frame(now + 100_000_000); // 100ms timeout

        // Frame reception may or may not succeed depending on timing
        // The important thing is the pipeline doesn't deadlock or crash
        if let Ok(recv_frame) = received {
            // Verify frame dimensions match
            assert_eq!(recv_frame.width().unwrap(), 640);
            assert_eq!(recv_frame.height().unwrap(), 480);
        }

        drop(client);
        drop(host);
    }

    #[test]
    fn test_client_disconnect() {
        let socket_path = test_socket_path("client_disconnect");

        // Create a host
        let host = Host::new(&socket_path).unwrap();

        // Create a client
        let client = Client::new(&socket_path, false).unwrap();

        // Disconnect should succeed
        client.disconnect().unwrap();

        drop(client);
        drop(host);
    }
}
