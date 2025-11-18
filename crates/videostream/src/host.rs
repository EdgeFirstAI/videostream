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
pub struct Host {
    ptr: *mut ffi::VSLHost,
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

    pub fn poll(&self) {}

    pub fn process(&self) {}

    pub fn sockets(&self) {}
}

impl Drop for Host {
    fn drop(&mut self) {
        if let Ok(lib) = ffi::init() {
            unsafe { lib.vsl_host_release(self.ptr); }
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
}
