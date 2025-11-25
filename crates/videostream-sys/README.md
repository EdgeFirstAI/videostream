# videostream-sys

[![crates.io](https://img.shields.io/crates/v/videostream-sys.svg)](https://crates.io/crates/videostream-sys)
[![Documentation](https://docs.rs/videostream-sys/badge.svg)](https://docs.rs/videostream-sys)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://github.com/EdgeFirstAI/videostream/blob/main/LICENSE)

Low-level FFI bindings for the [VideoStream Library](https://github.com/EdgeFirstAI/videostream) - unsafe bindings to `libvideostream`.

## Overview

`videostream-sys` provides raw, unsafe FFI bindings to the VideoStream C library (`libvideostream`), which offers:

- **Inter-process frame sharing** via DmaBuf and POSIX shared memory
- **V4L2 camera capture** with zero-copy DmaBuf export
- **Hardware H.264/H.265 encoding** using Hantro VPU (NXP i.MX8)
- **GStreamer integration** for multi-process video pipelines

This crate contains the low-level bindings. For safe, idiomatic Rust APIs, use the [`videostream`](https://crates.io/crates/videostream) crate instead.

## Platform Support

- **Linux**: x86_64, aarch64, armv7
- **NXP i.MX8M Plus**: Full hardware acceleration support
- **Generic ARM/x64**: Software fallback modes available

## Requirements

### Runtime Dependencies

The VideoStream C library must be installed on your system:

```bash
# Ubuntu/Debian
sudo apt-get install libvideostream1

# Or build from source
git clone https://github.com/EdgeFirstAI/videostream
cd videostream
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

### Build Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev
```

## Usage

This crate provides raw FFI bindings. **These are unsafe and require careful memory management.**

```rust
use videostream_sys::*;
use std::ffi::CString;

unsafe {
    // Initialize host (frame producer)
    let socket_path = CString::new("/tmp/vsl_socket").unwrap();
    let mut host: *mut vsl_host_t = std::ptr::null_mut();
    
    let ret = vsl_host_init(
        socket_path.as_ptr(),
        640,  // width
        480,  // height
        VSL_PIXEL_FORMAT_RGBX as i32,
        &mut host as *mut _,
    );
    
    if ret == 0 {
        // Use the host...
        
        // Clean up
        vsl_host_destroy(host);
    }
}
```

**For safe, idiomatic Rust usage, use the [`videostream`](https://crates.io/crates/videostream) crate instead.**

## Building

This crate uses `bindgen` to generate bindings from the C headers. The build script (`build.rs`) will:

1. Locate the installed `libvideostream` headers
2. Generate Rust bindings using `bindgen`
3. Link against the installed library

```bash
cargo build
```

## Features

Currently, this crate has no optional features. All bindings are included by default.

## Safety

⚠️ **All functions in this crate are `unsafe`.**

When using these bindings directly, you must ensure:

- Pointers are valid and properly aligned
- Memory lifetimes are correctly managed
- Thread safety is maintained (most functions are not thread-safe)
- Return values are checked for errors (negative errno values)

## Documentation

- [VideoStream C API Documentation](https://github.com/EdgeFirstAI/videostream)
- [Rust bindings (safe API)](https://docs.rs/videostream)
- [crates.io](https://crates.io/crates/videostream-sys)

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](https://github.com/EdgeFirstAI/videostream/blob/main/LICENSE) for details.

Copyright © 2025 Au-Zone Technologies
