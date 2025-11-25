# videostream

[![crates.io](https://img.shields.io/crates/v/videostream.svg)](https://crates.io/crates/videostream)
[![Documentation](https://docs.rs/videostream/badge.svg)](https://docs.rs/videostream)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://github.com/EdgeFirstAI/videostream/blob/main/LICENSE)

Safe Rust bindings for the [VideoStream Library](https://github.com/EdgeFirstAI/videostream) - zero-copy video frame management and distribution for embedded Linux.

## Overview

VideoStream provides inter-process video frame sharing with zero-copy transfers, making it ideal for embedded vision applications that need to distribute camera frames to multiple consumers efficiently.

### Key Features

- **Zero-Copy Frame Sharing** - Uses Linux DmaBuf and POSIX shared memory
- **Multi-Consumer Support** - One producer, multiple concurrent consumers
- **Thread-Safe** - Safe concurrent access to frame pools
- **Reference Counting** - Automatic memory management
- **Hardware Acceleration** - V4L2 camera capture and Hantro VPU encoding
- **GStreamer Integration** - Pipeline-based video processing

### Use Cases

- **Multi-Process Vision Pipelines** - Share camera frames between detection, tracking, and display
- **Edge AI Applications** - Distribute frames to multiple inference engines
- **Video Recording + Analysis** - Simultaneously record and analyze camera streams
- **Low-Latency Streaming** - Hardware-accelerated encoding with minimal overhead

## Platform Support

- **NXP i.MX8M Plus** - Full hardware acceleration (G2D, VPU, DmaBuf)
- **Generic ARM64/ARMv7** - Software fallback with shared memory
- **x86_64** - Development and testing support

## Requirements

### Runtime Dependencies

The VideoStream C library must be installed:

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

## Quick Start

Add to your `Cargo.toml`:

```toml
[dependencies]
videostream = "1.5"
```

### Host (Frame Producer)

```rust
use videostream::{Host, PixelFormat};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Create a host to share 1920x1080 RGB frames
    let mut host = Host::new(
        "/tmp/videostream.sock",
        1920,
        1080,
        PixelFormat::RGBX,
    )?;

    // Simulate frame production
    loop {
        // Get writable access to next frame buffer
        let mut frame = host.get_write_buffer()?;
        
        // Write frame data (e.g., from camera)
        let data = frame.data_mut();
        // ... fill data ...
        
        // Publish frame to all connected clients
        host.publish_frame(frame)?;
        
        std::thread::sleep(std::time::Duration::from_millis(33)); // ~30 fps
    }
}
```

### Client (Frame Consumer)

```rust
use videostream::Client;
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Connect to the host
    let mut client = Client::connect("/tmp/videostream.sock")?;

    loop {
        // Wait for next frame (with timeout)
        match client.wait_for_frame(Duration::from_secs(1))? {
            Some(frame) => {
                // Access frame data (zero-copy)
                let data = frame.data();
                let (width, height) = frame.dimensions();
                
                println!("Received {}x{} frame ({} bytes)", 
                         width, height, data.len());
                
                // Process frame...
            }
            None => {
                println!("No frame received (timeout)");
            }
        }
    }
}
```

## Architecture

```
┌─────────────┐
│    Host     │  Frame Producer (1)
│  (Producer) │  - Allocates frame pool
└──────┬──────┘  - Publishes frames via IPC
       │
       │ UNIX Socket + DmaBuf FDs
       │
       ├─────────┬─────────┬─────────┐
       │         │         │         │
   ┌───▼───┐ ┌──▼────┐ ┌──▼────┐ ┌──▼────┐
   │Client │ │Client │ │Client │ │Client │  Frame Consumers (N)
   │   #1  │ │   #2  │ │   #3  │ │  ...  │  - Zero-copy access
   └───────┘ └───────┘ └───────┘ └───────┘  - Thread-safe
```

## Performance

Benchmarks on NXP i.MX8M Plus (1080p@30fps):

- **Frame Latency**: <3ms (DmaBuf), <10ms (shared memory)
- **CPU Overhead**: <2% per client (frame distribution)
- **Memory**: ~100KB per client (no frame copies)
- **Throughput**: 60fps sustained with 3 concurrent clients

## Safety

This crate provides safe wrappers around the unsafe FFI bindings in [`videostream-sys`](https://crates.io/crates/videostream-sys):

- ✅ Memory safety through RAII and lifetimes
- ✅ Thread safety with interior mutability patterns
- ✅ Null pointer checks and error handling
- ✅ Automatic resource cleanup (Drop implementations)

## Examples

See the [examples directory](https://github.com/EdgeFirstAI/videostream/tree/main/crates/videostream/examples) for complete applications:

- `host_basic.rs` - Simple frame producer
- `client_basic.rs` - Simple frame consumer
- `camera_capture.rs` - V4L2 camera integration
- `multi_client.rs` - Multiple consumers

## Documentation

- [API Documentation](https://docs.rs/videostream)
- [VideoStream Library](https://github.com/EdgeFirstAI/videostream)
- [EdgeFirst Perception](https://doc.edgefirst.ai/test/perception/)

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](https://github.com/EdgeFirstAI/videostream/blob/main/CONTRIBUTING.md).

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](https://github.com/EdgeFirstAI/videostream/blob/main/LICENSE) for details.

Copyright © 2025 Au-Zone Technologies
