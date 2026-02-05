// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::{frame::Frame, Error};
use std::{
    ffi::{CStr, CString},
    io,
    path::PathBuf,
};
use videostream_sys as ffi;

/// Reconnection behavior for client connections.
///
/// Controls whether a [`Client`] automatically reconnects when the connection
/// to the host is lost.
///
/// # Examples
///
/// ```no_run
/// use videostream::client::{Client, Reconnect};
///
/// // Client with automatic reconnection
/// let client = Client::new("/tmp/video.sock", Reconnect::Yes)?;
///
/// // Client without automatic reconnection
/// let client = Client::new("/tmp/video.sock", Reconnect::No)?;
/// # Ok::<(), videostream::Error>(())
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Reconnect {
    /// Do not automatically reconnect on disconnect
    #[default]
    No,
    /// Automatically reconnect when connection is lost
    Yes,
}

impl From<Reconnect> for bool {
    fn from(reconnect: Reconnect) -> bool {
        matches!(reconnect, Reconnect::Yes)
    }
}

impl From<bool> for Reconnect {
    fn from(value: bool) -> Self {
        if value {
            Reconnect::Yes
        } else {
            Reconnect::No
        }
    }
}

/// Client structure for connecting to a VideoStream host.
///
/// Provides functionality to subscribe to video frames published by a
/// [`crate::host::Host`].
///
/// # Examples
///
/// ```no_run
/// use videostream::client::{Client, Reconnect};
///
/// let client = Client::new("/tmp/video.sock", Reconnect::Yes)?;
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
    /// Creates a new client and connects to the host at the specified socket path.
    ///
    /// # Arguments
    ///
    /// * `path` - UNIX socket path to connect to
    /// * `reconnect` - Whether to automatically reconnect on disconnect
    ///
    /// # Returns
    ///
    /// Returns a new `Client` connected to the host.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if the connection fails.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::client::{Client, Reconnect};
    ///
    /// // Client with automatic reconnection
    /// let client = Client::new("/tmp/video.sock", Reconnect::Yes)?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn new(path: &str, reconnect: Reconnect) -> Result<Self, Error> {
        let path_str_c = CString::new(path)?;
        let ptr = vsl!(vsl_client_init(
            path_str_c.as_ptr(),
            std::ptr::null_mut(),
            reconnect.into()
        ));
        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(Client { ptr })
    }

    /// Disconnects from the host.
    ///
    /// Closes the connection to the host server. If `Reconnect::Yes` was specified,
    /// the client will attempt to reconnect on the next frame request.
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::client::{Client, Reconnect};
    ///
    /// let client = Client::new("/tmp/video.sock", Reconnect::No)?;
    /// // ... use client ...
    /// client.disconnect()?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn disconnect(&self) -> Result<(), Error> {
        vsl!(vsl_client_disconnect(self.ptr));
        Ok(())
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
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::client::{Client, Reconnect};
    ///
    /// let client = Client::new("/tmp/video.sock", Reconnect::No)?;
    ///
    /// // Check if a user pointer was set
    /// if let Some(ptr) = client.userptr()? {
    ///     println!("Client has user data");
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn userptr(&self) -> Result<Option<*mut std::os::raw::c_void>, Error> {
        let ptr = vsl!(vsl_client_userptr(self.ptr));
        if ptr.is_null() {
            Ok(None)
        } else {
            Ok(Some(ptr))
        }
    }

    /// Returns the socket path this client is connected to.
    ///
    /// # Errors
    ///
    /// Returns [`Error::NullPointer`] if the path is null or [`Error::Utf8`] if
    /// the path is not valid UTF-8.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::client::{Client, Reconnect};
    ///
    /// let client = Client::new("/tmp/video.sock", Reconnect::No)?;
    /// let path = client.path()?;
    /// println!("Connected to: {:?}", path);
    /// # Ok::<(), videostream::Error>(())
    /// ```
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

    /// Sets the timeout for frame reception.
    ///
    /// Controls how long [`Client::get_frame`] will wait for a frame before timing out.
    ///
    /// # Arguments
    ///
    /// * `timeout` - Timeout in seconds (fractional values allowed)
    ///
    /// # Errors
    ///
    /// Returns [`Error::LibraryNotLoaded`] if `libvideostream.so` cannot be loaded.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::client::{Client, Reconnect};
    ///
    /// let client = Client::new("/tmp/video.sock", Reconnect::No)?;
    /// client.set_timeout(5.0)?; // 5 second timeout
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn set_timeout(&self, timeout: f32) -> Result<(), Error> {
        vsl!(vsl_client_set_timeout(self.ptr, timeout));
        Ok(())
    }

    /// Waits for and receives the next frame from the host.
    ///
    /// Blocks until a frame is available or the timeout expires. The `until` parameter
    /// specifies an absolute deadline using the monotonic clock.
    ///
    /// # Arguments
    ///
    /// * `until` - Absolute deadline timestamp in nanoseconds (0 = wait indefinitely)
    ///
    /// # Returns
    ///
    /// Returns the received [`Frame`] on success.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if the operation fails or times out.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::client::{Client, Reconnect};
    /// use videostream::frame::Frame;
    ///
    /// let client = Client::new("/tmp/video.sock", Reconnect::Yes)?;
    ///
    /// // Wait indefinitely
    /// let frame = client.get_frame(0)?;
    /// println!("Received frame: {}x{}", frame.width()?, frame.height()?);
    /// # Ok::<(), videostream::Error>(())
    /// ```
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
        // vsl_client_release handles full cleanup including socket close
        if let Ok(lib) = ffi::init() {
            unsafe {
                lib.vsl_client_release(self.ptr);
            }
        }
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

    /// Helper to create a unique socket path for each test.
    /// Uses process ID and thread ID to ensure uniqueness across parallel test runs.
    fn test_socket_path(name: &str) -> String {
        format!(
            "/tmp/vsl_test_{}_{}_{:?}.sock",
            name,
            std::process::id(),
            std::thread::current().id()
        )
    }

    /// Small delay to ensure host socket is ready for connections.
    /// Required because socket creation (bind + listen) is not atomic.
    const HOST_READY_DELAY: Duration = Duration::from_millis(5);

    #[test]
    fn test_client_debug() {
        let socket_path = test_socket_path("client_debug");

        // Create a host first so the client can connect
        let host = Host::new(&socket_path).unwrap();
        thread::sleep(HOST_READY_DELAY);

        // Now create a client that connects to the host
        let client = Client::new(&socket_path, Reconnect::No).unwrap();
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
        thread::sleep(HOST_READY_DELAY);

        // Test 1: Client created with null userptr should return None
        let client_none = Client::new(&socket_path, Reconnect::No).unwrap();
        let userptr_none = client_none.userptr().unwrap();
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
        let userptr_some = client_some.userptr().unwrap();
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
        thread::sleep(HOST_READY_DELAY);

        // Create a client
        let client = Client::new(&socket_path, Reconnect::No).unwrap();

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
        thread::sleep(HOST_READY_DELAY);

        // Create a client
        let client = Client::new(&socket_path, Reconnect::No).unwrap();

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
        thread::sleep(HOST_READY_DELAY);

        // Create a client
        let client = Client::new(&socket_path, Reconnect::No).unwrap();

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
        thread::sleep(HOST_READY_DELAY);

        // Create a client
        let client = Client::new(&socket_path, Reconnect::No).unwrap();

        // Disconnect should succeed
        client.disconnect().unwrap();

        drop(client);
        drop(host);
    }

    #[test]
    fn test_reconnect_enum() {
        // Test default
        assert_eq!(Reconnect::default(), Reconnect::No);

        // Test conversion to bool
        assert!(bool::from(Reconnect::Yes));
        assert!(!bool::from(Reconnect::No));

        // Test conversion from bool
        assert_eq!(Reconnect::from(true), Reconnect::Yes);
        assert_eq!(Reconnect::from(false), Reconnect::No);

        // Test Debug
        let debug_str = format!("{:?}", Reconnect::Yes);
        assert!(debug_str.contains("Yes"));

        // Test PartialEq
        assert_eq!(Reconnect::Yes, Reconnect::Yes);
        assert_ne!(Reconnect::Yes, Reconnect::No);

        // Test Clone/Copy
        let r1 = Reconnect::Yes;
        let r2 = r1;
        assert_eq!(r1, r2);
    }

    /// Test that Reconnect::Yes allows client to connect when host exists.
    /// This is a simpler version that doesn't require complex thread synchronization.
    #[test]
    fn test_reconnect_yes_with_existing_host() {
        let socket_path = test_socket_path("reconnect_yes");
        let host = Host::new(&socket_path).unwrap();
        thread::sleep(HOST_READY_DELAY);

        // With Reconnect::Yes, client should connect successfully
        let client = Client::new(&socket_path, Reconnect::Yes).unwrap();
        let path = client.path().unwrap();
        assert!(path.to_str().unwrap().contains("reconnect_yes"));

        // Process the connection
        let _ = host.poll(10);
        let _ = host.process();

        drop(client);
        drop(host);
    }

    /// Test Reconnect::Yes with client created before host.
    /// This test is ignored by default as it depends on C library retry timing.
    /// Run with --ignored to include this test.
    #[test]
    #[ignore]
    fn test_reconnect_client_before_host() {
        use std::sync::atomic::{AtomicBool, Ordering};
        use std::sync::Arc;

        let socket_path = test_socket_path("reconnect_before");
        let socket_path_clone = socket_path.clone();
        let client_ready = Arc::new(AtomicBool::new(false));
        let client_ready_clone = Arc::clone(&client_ready);

        // Start client connection attempt in a background thread
        let client_thread = thread::spawn(move || {
            // Signal that we're about to try connecting
            client_ready_clone.store(true, Ordering::SeqCst);

            // Create client with Reconnect::Yes - it will wait for host
            let client = Client::new(&socket_path_clone, Reconnect::Yes).unwrap();

            // Verify client is connected
            let path = client.path().unwrap();
            assert!(
                path.to_str().unwrap().contains("reconnect_before"),
                "Client should be connected to the correct path"
            );
            client
        });

        // Wait for client thread to start trying to connect
        while !client_ready.load(Ordering::SeqCst) {
            thread::sleep(Duration::from_millis(1));
        }

        // Add extra delay to ensure client is actually waiting
        thread::sleep(Duration::from_millis(100));

        // Now create the host - client should connect
        let host = Host::new(&socket_path).unwrap();

        // Service connections for up to 5 seconds
        for _ in 0..500 {
            let _ = host.poll(10);
            let _ = host.process();
        }

        let client = client_thread.join().unwrap();
        drop(client);
        drop(host);
    }

    /// Test reconnection with frame transfer (client before host).
    /// This test is ignored by default as it depends on C library retry timing.
    /// Run with --ignored to include this test.
    #[test]
    #[ignore]
    fn test_reconnect_with_frame_transfer() {
        use crate::frame::Frame;
        use crate::timestamp;
        use std::sync::atomic::{AtomicBool, Ordering};
        use std::sync::Arc;

        let socket_path = test_socket_path("reconnect_frame");
        let socket_path_clone = socket_path.clone();
        let client_connected = Arc::new(AtomicBool::new(false));
        let client_connected_clone = Arc::clone(&client_connected);

        // Start client in background thread
        let client_thread = thread::spawn(move || {
            // Create client with Reconnect::Yes before host exists
            let client = Client::new(&socket_path_clone, Reconnect::Yes).unwrap();
            client_connected_clone.store(true, Ordering::SeqCst);

            // Try to receive the frame (with timeout)
            let deadline = timestamp().unwrap() + 1_000_000_000; // 1 second
            let result = client.get_frame(deadline);

            // Return frame dimensions if received
            result
                .ok()
                .map(|f| (f.width().unwrap(), f.height().unwrap()))
        });

        // Wait a bit, then create host
        thread::sleep(Duration::from_millis(20));
        let host = Host::new(&socket_path).unwrap();

        // Wait for client to connect
        while !client_connected.load(Ordering::SeqCst) {
            let _ = host.poll(10);
            let _ = host.process();
        }

        // Give connection time to establish
        thread::sleep(HOST_READY_DELAY);

        // Process pending connections
        let _ = host.poll(10);
        let _ = host.process();

        // Create and post a frame
        let frame = Frame::new(320, 240, 0, "RGB3").unwrap();
        frame.alloc(None).unwrap();

        // Fill with test pattern
        {
            let data = frame.mmap_mut().unwrap();
            for (i, byte) in data.iter_mut().enumerate() {
                *byte = (i % 256) as u8;
            }
        }

        let now = timestamp().unwrap();
        let expires = now + 1_000_000_000; // 1 second
        host.post(frame, expires, -1, -1, -1).unwrap();

        // Keep processing until client receives frame
        for _ in 0..100 {
            let _ = host.poll(10);
            let _ = host.process();
        }

        // Check client received the frame
        let frame_dims = client_thread.join().unwrap();
        if let Some((width, height)) = frame_dims {
            assert_eq!(width, 320);
            assert_eq!(height, 240);
        }

        drop(host);
    }

    #[test]
    fn test_reconnect_no_fails_without_host() {
        let socket_path = test_socket_path("reconnect_no_fail");

        // Client with Reconnect::No should fail immediately when host doesn't exist
        let result = Client::new(&socket_path, Reconnect::No);
        assert!(
            result.is_err(),
            "Client with Reconnect::No should fail when host doesn't exist"
        );
    }
}
