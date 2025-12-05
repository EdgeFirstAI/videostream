// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use crate::Error;
use std::{
    ffi::{CStr, CString},
    io,
    os::unix::prelude::OsStrExt,
    path::{Path, PathBuf},
};
use videostream_sys as ffi;

/// The Host structure provides the frame sharing functionality.  Only a single
/// host can own frames while a host can have many Client subscribers to the
/// frames.
///
/// A host is created with a socket path which it will own exclusively and
/// allowing clients to connect in order to receive frames.
///
/// # Examples
///
/// ```no_run
/// use videostream::host::Host;
///
/// let host = Host::new("/tmp/video.sock")?;
/// println!("Host listening on: {:?}", host.path()?);
/// # Ok::<(), videostream::Error>(())
/// ```
pub struct Host {
    ptr: *mut ffi::VSLHost,
}

impl std::fmt::Debug for Host {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let path = self.path().unwrap_or_else(|_| PathBuf::from("<error>"));
        f.debug_struct("Host").field("path", &path).finish()
    }
}

impl Host {
    /// Creates a new Host and creates a socket at the specified path on which
    /// it will listen for client connections.
    pub fn new<P: AsRef<Path>>(path: P) -> Result<Self, Error> {
        let path_str_c = CString::new(path.as_ref().as_os_str().as_bytes())?;
        let ptr = vsl!(vsl_host_init(path_str_c.as_ptr()));
        if ptr.is_null() {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(Host { ptr })
    }

    pub fn path(&self) -> Result<PathBuf, Error> {
        let path_str_c = vsl!(vsl_host_path(self.ptr));
        if path_str_c.is_null() {
            return Err(Error::NullPointer);
        }

        let path_str = unsafe { CStr::from_ptr(path_str_c).to_str()? };
        Ok(PathBuf::from(path_str))
    }

    /// Polls the host's socket connections for activity.
    ///
    /// Waits for socket activity (new connections or client messages) using poll().
    /// Should be called in a loop before [`Host::process`]. The `wait` parameter
    /// controls timeout behavior:
    /// - `> 0`: Poll waits up to this duration in milliseconds
    /// - `= 0`: Returns immediately
    /// - `< 0`: Waits indefinitely
    ///
    /// # Arguments
    ///
    /// * `wait` - Timeout in milliseconds
    ///
    /// # Returns
    ///
    /// Returns the number of sockets with activity, 0 on timeout, or an error.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if the underlying poll() call fails.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::host::Host;
    ///
    /// let host = Host::new("/tmp/video.sock")?;
    /// loop {
    ///     match host.poll(1000) {
    ///         Ok(n) if n > 0 => {
    ///             host.process()?;
    ///         }
    ///         Ok(_) => {} // timeout
    ///         Err(e) => eprintln!("Poll error: {}", e),
    ///     }
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn poll(&self, wait: i64) -> Result<i32, Error> {
        let ret = vsl!(vsl_host_poll(self.ptr, wait));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(ret)
    }

    /// Processes host tasks: expires old frames and services one client connection.
    ///
    /// First expires frames past their lifetime, then services the first available
    /// connection (accepting new clients or processing client messages). Should be
    /// called in a loop, typically after [`Host::poll`] indicates activity.
    ///
    /// # Returns
    ///
    /// Returns `Ok(())` on success.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if processing fails. Common errors include `ETIMEDOUT`
    /// if no activity is available.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::host::Host;
    ///
    /// let host = Host::new("/tmp/video.sock")?;
    /// loop {
    ///     if host.poll(1000)? > 0 {
    ///         host.process()?;
    ///     }
    /// }
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn process(&self) -> Result<(), Error> {
        let ret = vsl!(vsl_host_process(self.ptr));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    /// Services a single client socket.
    ///
    /// Processes messages from a specific client socket. Does not accept new
    /// connections - use [`Host::process`] for that. Useful when you need to
    /// track errors for individual clients.
    ///
    /// # Arguments
    ///
    /// * `sock` - The client socket file descriptor to service
    ///
    /// # Returns
    ///
    /// Returns `Ok(())` on success.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] on failure. Common errors include `EPIPE` if the
    /// client has disconnected.
    pub fn service(&self, sock: i32) -> Result<(), Error> {
        let ret = vsl!(vsl_host_service(self.ptr, sock));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    /// Requests a copy of the sockets managed by the host.
    ///
    /// Returns socket file descriptors for the host's listening socket and all
    /// connected client sockets. The first socket is always the listening socket.
    /// The array should be refreshed frequently as sockets may become stale.
    ///
    /// Thread-safe: allows one thread to use sockets for messaging while another
    /// polls for reads.
    ///
    /// # Returns
    ///
    /// Returns a vector of socket file descriptors. The first entry is the
    /// listening socket, followed by client sockets.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if the operation fails.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::host::Host;
    ///
    /// let host = Host::new("/tmp/video.sock")?;
    /// let sockets = host.sockets()?;
    /// println!("Listening socket: {}", sockets[0]);
    /// println!("Number of clients: {}", sockets.len() - 1);
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn sockets(&self) -> Result<Vec<i32>, Error> {
        // First call to get the required size
        let mut max_sockets: usize = 0;
        let _ret = vsl!(vsl_host_sockets(
            self.ptr,
            0,
            std::ptr::null_mut(),
            &mut max_sockets as *mut usize
        ));

        if max_sockets == 0 {
            return Ok(Vec::new());
        }

        // Allocate buffer and get actual sockets
        let mut sockets = vec![0i32; max_sockets];
        let ret = vsl!(vsl_host_sockets(
            self.ptr,
            max_sockets,
            sockets.as_mut_ptr(),
            std::ptr::null_mut()
        ));

        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }

        Ok(sockets)
    }

    /// Posts a frame to all connected clients.
    ///
    /// Transfers ownership of the frame to the host. The frame is broadcast to all
    /// connected clients and will be automatically released when it expires. Do not
    /// call [`crate::frame::Frame::drop`] on frames posted to the host.
    ///
    /// # Arguments
    ///
    /// * `frame` - Frame to post (ownership transferred to host)
    /// * `expires` - Expiration time in nanoseconds (absolute, from [`crate::timestamp`])
    /// * `duration` - Frame duration in nanoseconds (-1 if unknown)
    /// * `pts` - Presentation timestamp in nanoseconds (-1 if unknown)
    /// * `dts` - Decode timestamp in nanoseconds (-1 if unknown)
    ///
    /// # Returns
    ///
    /// Returns `Ok(())` on success.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if posting fails.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use videostream::{host::Host, frame::Frame, timestamp};
    ///
    /// let host = Host::new("/tmp/video.sock")?;
    /// let frame = Frame::new(1920, 1080, 1920 * 2, "YUYV")?;
    /// frame.alloc(None)?;
    ///
    /// let now = timestamp()?;
    /// let expires = now + 1_000_000_000; // 1 second
    /// host.post(frame, expires, -1, -1, -1)?;
    /// # Ok::<(), videostream::Error>(())
    /// ```
    pub fn post(
        &self,
        frame: crate::frame::Frame,
        expires: i64,
        duration: i64,
        pts: i64,
        dts: i64,
    ) -> Result<(), Error> {
        let frame_ptr = frame.get_ptr();
        std::mem::forget(frame); // Transfer ownership to host

        let ret = vsl!(vsl_host_post(
            self.ptr, frame_ptr, expires, duration, pts, dts
        ));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }

    /// Drops a frame from the host.
    ///
    /// Removes the host association of the frame and returns ownership to the
    /// caller. Can be used to cancel a previously posted frame before it expires.
    ///
    /// # Arguments
    ///
    /// * `frame` - Frame to drop from host (must be owned by this host)
    ///
    /// # Returns
    ///
    /// Returns `Ok(())` on success.
    ///
    /// # Errors
    ///
    /// Returns [`Error::Io`] if the operation fails.
    pub fn drop_frame(&self, frame: &crate::frame::Frame) -> Result<(), Error> {
        let ret = vsl!(vsl_host_drop(self.ptr, frame.get_ptr()));
        if ret < 0 {
            let err = io::Error::last_os_error();
            return Err(err.into());
        }
        Ok(())
    }
}

impl Drop for Host {
    fn drop(&mut self) {
        if let Ok(lib) = ffi::init() {
            unsafe {
                lib.vsl_host_release(self.ptr);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn test_host() {
        let path = PathBuf::from("/tmp/test.vsl");
        let host = Host::new(&path).unwrap();
        assert_eq!(path, host.path().unwrap());
        assert!(path.exists());
        // Rust doesn't provide an is_socket but we at least confirm some things it is
        // not.
        assert!(!path.is_file());
        assert!(!path.is_dir());
        assert!(!path.is_symlink());

        // FIXME: currently the library will unlink old sockets, this should be
        // corrected along with adding proper cleanup and error handling when a
        // socket is already present.
        //
        // Creating a second host at the same path should raise an error.
        // let host2 = Host::new(&path);
        // assert!(host2.is_err());
    }

    #[test]
    fn test_host_sockets() {
        let path = PathBuf::from("/tmp/test_sockets.vsl");
        let host = Host::new(&path).unwrap();

        // Should have at least the listening socket
        let sockets = host.sockets().unwrap();
        assert!(
            !sockets.is_empty(),
            "Expected at least 1 socket (listening socket)"
        );

        // The first socket should be the listening socket and be a valid FD
        assert!(sockets[0] >= 0, "Listening socket FD should be >= 0");
    }

    #[test]
    fn test_host_poll_timeout() {
        let path = PathBuf::from("/tmp/test_poll.vsl");
        let host = Host::new(&path).unwrap();

        // Poll with immediate timeout should return 0 (no activity)
        let result = host.poll(0).unwrap();
        assert_eq!(result, 0, "Poll with 0 timeout should return 0");
    }

    #[test]
    fn test_host_debug() {
        let path = PathBuf::from("/tmp/test_debug.vsl");
        let host = Host::new(&path).unwrap();
        let debug_str = format!("{:?}", host);

        // Debug output should contain Host and path info
        assert!(debug_str.contains("Host"));
        assert!(debug_str.contains("test_debug.vsl"));
    }
}
