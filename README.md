# EdgeFirst VideoStream Library

[![Build Status](https://img.shields.io/github/actions/workflow/status/EdgeFirstAI/videostream/test.yml?branch=main)](https://github.com/EdgeFirstAI/videostream/actions)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=EdgeFirstAI_videostream&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=EdgeFirstAI_videostream)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=EdgeFirstAI_videostream&metric=coverage)](https://sonarcloud.io/summary/new_code?id=EdgeFirstAI_videostream)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=EdgeFirstAI_videostream&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=EdgeFirstAI_videostream)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![EdgeFirst Perception](https://img.shields.io/badge/EdgeFirst-Perception-green)](https://doc.edgefirst.ai/test/perception/)

**Video utility library for embedded Linux** providing inter-process frame sharing, V4L2 camera capture, and hardware-accelerated H.264 encoding.

VideoStream delivers essential video I/O capabilities for embedded vision applications, including camera access, codec integration, and optional inter-process communication.

---

## Overview

VideoStream Library provides three core capabilities for embedded Linux video applications:

### 1. Video4Linux2 Camera Capture

- **V4L2 device interface** for camera frame acquisition
- **DmaBuf export** for zero-copy capture from camera drivers (required for DMA operation)
- **Format negotiation** and capability detection
- **Multi-planar format support** (YUV420, NV12, YUYV, etc.)
- **Camera controls** (exposure, gain, white balance, etc.)
- **Hardware compatibility** with MIPI CSI-2, USB cameras, and other V4L2 devices

### 2. Hardware H.264/H.265 Encoding

- **Hantro VPU integration** for hardware-accelerated encoding on NXP i.MX8 platforms
- **Zero-copy input** from DmaBuf or camera buffers
- **H.264/H.265 codec support** with configurable bitrate profiles
- **Low-power encoding** minimizes CPU load
- **Direct DmaBuf encoding** for efficient pipeline integration

### 3. Inter-Process Frame Sharing

- **Zero-copy buffer sharing** via Linux DmaBuf and POSIX shared memory
- **UNIX domain socket IPC** with file descriptor passing (SCM_RIGHTS)
- **Thread-safe reference counting** for multi-consumer frame access
- **Host/Client architecture** - one producer, multiple consumers
- **GStreamer plugins** (vslsink, vslsrc) for multi-process pipeline integration
- **Configurable frame lifespans** with automatic expiry and cleanup

Originally developed as part of the DeepView project, VideoStream is now used by EdgeFirst Perception and available as a standalone library for embedded vision applications.

---

## Features

### Inter-Process Frame Sharing

- **Zero-Copy Transfer** - DmaBuf and POSIX shared memory for efficient frame sharing
- **UNIX Socket IPC** - File descriptor passing via SCM_RIGHTS ancillary data
- **Multi-Consumer Support** - Multiple clients can access same frame pool concurrently
- **Reference Counting** - Automatic frame lifecycle management prevents memory leaks
- **GStreamer Plugins** - vslsink (producer) and vslsrc (consumer) for pipeline integration
- **Timeout Management** - Configurable frame lifespans and client timeouts

### V4L2 Camera Capture

- **Standard V4L2 Interface** - Compatible with any Video4Linux2 camera driver
- **DmaBuf Export** - Zero-copy frame acquisition via VIDIOC_EXPBUF (required for DMA operation)
- **Format Support** - YUV (420, 422, 444), RGB variants, Bayer patterns
- **Multi-Planar** - Native support for multi-planar formats (NV12, NV21, etc.)
- **Camera Controls** - Exposure, gain, white balance, and other V4L2 controls
- **DMA-Capable Buffers** - Uses DmaBuf for hardware accelerator compatibility (VPU, NPU)

### Hardware H.264/H.265 Encoding

- **Hantro VPU** - Hardware-accelerated encoding on NXP i.MX8 platforms
- **Codec Support** - H.264 (AVC) and H.265 (HEVC) with profile/level configuration
- **Rate Control** - CBR, VBR, and fixed QP encoding modes
- **Zero-Copy Input** - Direct encoding from camera DmaBuf or shared memory
- **Low Latency** - Hardware encoding minimizes CPU load and latency
- **Multi-Stream** - Support for concurrent encoding streams

### Platform Support

- **NXP i.MX8M Plus** - Full support (G2D, VPU, V4L2 DmaBuf)
- **NXP i.MX8M** - DmaBuf and basic hardware acceleration
- **Generic ARM64/ARMv7** - POSIX shared memory fallback
- **x86_64** - Development and testing (software encoding)

---

## Quick Start

### Installation

#### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install -y \
    build-essential cmake pkg-config \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev

# Fedora/RHEL
sudo dnf install -y \
    gcc gcc-c++ cmake \
    gstreamer1-devel \
    gstreamer1-plugins-base-devel
```

#### Build from Source

```bash
git clone https://github.com/EdgeFirstAI/videostream.git
cd videostream
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

#### Cross-Compile for ARM64 (NXP Yocto SDK)

```bash
source /opt/fsl-imx-xwayland/5.4-zeus/environment-setup-aarch64-poky-linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

### Usage Examples

#### Example 1: VideoStream CLI (Recommended)

The `videostream` CLI provides the easiest way to work with cameras and video streaming:

```bash
# Display hardware capabilities (camera, VPU encoder/decoder)
videostream info

# Record H.264 video from camera (60 frames)
videostream record output.h264 --frames 60 --device /dev/video3

# Convert H.264 to MP4 container
videostream convert output.h264 output.mp4

# Stream camera frames to VSL socket (with H.264 encoding)
videostream stream /tmp/camera.sock --encode --codec h264 &

# Receive frames and display metrics
videostream receive /tmp/camera.sock --frames 100 --json
```

For detailed CLI usage, run `videostream --help` or `videostream <command> --help`.

---

#### Example 2: V4L2 Camera Capture (C API)

```c
#include <videostream.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    // Open V4L2 camera device
    vsl_camera* camera = vsl_camera_open_device("/dev/video0");
    if (!camera) {
        fprintf(stderr, "Failed to open camera\n");
        return 1;
    }

    // Configure camera for 1920x1080 NV12 capture
    int width = 1920;
    int height = 1080;
    int buf_count = 4;
    uint32_t fourcc = VSL_FOURCC('N', 'V', '1', '2');

    if (vsl_camera_init_device(camera, &width, &height, &buf_count, &fourcc)) {
        fprintf(stderr, "Failed to init camera\n");
        vsl_camera_close_device(camera);
        return 1;
    }

    printf("Camera initialized: %dx%d, buffers=%d\n", width, height, buf_count);

    // Start streaming
    vsl_camera_start_capturing(camera);

    // Capture frames
    for (int i = 0; i < 100; i++) {
        vsl_camera_buffer* buffer = vsl_camera_get_data(camera);
        if (!buffer) {
            continue;
        }

        printf("Captured frame %d, size=%u bytes\n",
               i, vsl_camera_buffer_length(buffer));

        // Process frame data
        void* data = vsl_camera_buffer_mmap(buffer);
        // ... process frame ...

        // Return buffer to queue
        vsl_camera_release_buffer(camera, buffer);
    }

    vsl_camera_stop_capturing(camera);
    vsl_camera_uninit_device(camera);
    vsl_camera_close_device(camera);
    return 0;
}
```

#### Example 2: Hardware H.264 Encoding (C API)

```c
#include <videostream.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    // Create encoder instance for H.264
    uint32_t h264_fourcc = VSL_FOURCC('H', '2', '6', '4');
    VSLEncoder* encoder = vsl_encoder_create(VSL_ENCODE_PROFILE_5000_KBPS,
                                              h264_fourcc,
                                              30);  // 30 fps
    if (!encoder) {
        fprintf(stderr, "Failed to create encoder\n");
        return 1;
    }

    // Create source and destination frames
    VSLFrame* source = vsl_frame_init(1920, 1080, 1920,
                                      VSL_FOURCC('N', 'V', '1', '2'),
                                      NULL, NULL);
    vsl_frame_alloc(source, NULL);  // Auto dmabuf/shm allocation

    // Encode frames
    for (int i = 0; i < 100; i++) {
        // Create output frame for encoded data
        VSLFrame* dest = vsl_encoder_new_output_frame(encoder,
                                                       1920, 1080,
                                                       33333333LL,  // duration (ns)
                                                       i * 33333333LL,  // pts
                                                       i * 33333333LL); // dts

        // Encode frame
        int keyframe = 0;
        int ret = vsl_encode_frame(encoder, source, dest, NULL, &keyframe);

        if (ret == 0) {
            printf("Encoded frame %d: %d bytes%s\n",
                   i, vsl_frame_size(dest),
                   keyframe ? " (keyframe)" : "");

            // Access encoded data
            void* data = vsl_frame_mmap(dest, NULL);
            // ... write to file or network ...
            vsl_frame_munmap(dest);
        }

        vsl_frame_release(dest);
    }

    vsl_frame_release(source);
    vsl_encoder_release(encoder);
    return 0;
}
```

#### Example 3: Inter-Process Frame Sharing with GStreamer

This example demonstrates sharing camera frames between processes using vslsink and vslsrc.

**Terminal 1 - Producer (Camera Host):**

```bash
# Capture from V4L2 camera and share frames via VideoStream
# The path can be explicit or auto-generated from element name
gst-launch-1.0 v4l2src device=/dev/video0 ! \
    video/x-raw,width=1920,height=1080,format=NV12,framerate=30/1 ! \
    vslsink path=/tmp/vsl_camera lifespan=100
```

**Socket Path Convention:**

- Filesystem paths start with `/` (e.g., `/tmp/vsl_camera`)
- Abstract sockets don't start with `/` (e.g., `vsl_camera`)
- If no path specified, vslsink auto-generates: `/tmp/<element_name>.<thread_id>`

**Terminal 2 - Consumer 1 (Display):**

```bash
# Receive frames and display
gst-launch-1.0 vslsrc path=/tmp/vsl_camera ! autovideosink
```

**Terminal 3 - Consumer 2 (Recording):**

```bash
# Simultaneously record to file (both consumers share same frames)
gst-launch-1.0 vslsrc path=/tmp/vsl_camera ! \
    x264enc ! mp4mux ! filesink location=recording.mp4
```

#### Example 4: Frame Sharing with Native C API

```c
#include <videostream.h>
#include <stdio.h>

// Client application accessing shared frames
int main(int argc, char *argv[])
{
    // Connect to frame pool host
    VSLClient* client = vsl_client_init("/tmp/vsl_camera", NULL, true);
    if (!client) {
        fprintf(stderr, "Failed to connect to frame pool\n");
        return 1;
    }

    printf("Connected to %s\n", vsl_client_path(client));

    while (1) {
        // Wait for next frame (blocks until frame available)
        // Pass 0 for "until" to get next available frame
        VSLFrame* frame = vsl_frame_wait(client, 0);
        if (!frame) {
            fprintf(stderr, "Failed to receive frame: %s\n", strerror(errno));
            continue;
        }

        // Lock frame for access
        if (vsl_frame_trylock(frame) < 0) {
            vsl_frame_release(frame);
            continue;  // Frame no longer valid
        }

        // Access frame metadata
        printf("Frame %ld: %dx%d, format=0x%x, pts=%ld\n",
               vsl_frame_serial(frame),
               vsl_frame_width(frame),
               vsl_frame_height(frame),
               vsl_frame_fourcc(frame),
               vsl_frame_pts(frame));

        // Map frame memory
        size_t size;
        void* data = vsl_frame_mmap(frame, &size);
        if (data) {
            // Process frame (e.g., run computer vision algorithm)
            // process_image(data, vsl_frame_width(frame), vsl_frame_height(frame));

            vsl_frame_munmap(frame);
        }

        // Unlock and release frame
        vsl_frame_unlock(frame);
        vsl_frame_release(frame);
    }

    vsl_client_release(client);
    return 0;
}
```

---

## How VideoStream Fits into EdgeFirst Perception

VideoStream is a **utility library** used by EdgeFirst Perception's camera service. The **edgefirst-camera** node leverages VideoStream's V4L2 camera capture and hardware encoding features to acquire and compress video streams before publishing to the Zenoh middleware.

### EdgeFirst Perception Architecture

```mermaid
graph TB
    subgraph "Sensor Layer"
        CAM[Camera<br/>ISP + Codec]
        SENS[Radar/LiDAR<br/>NavSat/IMU]
    end

    subgraph "Processing Layer"
        VIS[Vision Model<br/>Inference]
        FUS[Fusion Model<br/>Inference]
    end

    subgraph "Output Layer"
        REC[Recorder<br/>MCAP]
        WEB[Web UI<br/>HTTPS]
    end

    subgraph "Middleware"
        ZEN[ZENOH<br/>Middleware]
    end

    subgraph "User Applications"
        APPS[ROS2, Foxglove]
    end

    CAM --> VIS
    CAM --> FUS
    SENS --> FUS
    VIS --> REC
    FUS --> WEB
    VIS --> ZEN
    FUS --> ZEN
    REC --> ZEN
    WEB --> ZEN
    ZEN --> APPS

    style CAM fill:#e1f5ff
    style VIS fill:#fff4e1
    style FUS fill:#fff4e1
    style REC fill:#e8f5e9
    style WEB fill:#e8f5e9
    style ZEN fill:#f3e5f5
    style APPS fill:#fce4ec
```

**VideoStream's Role in edgefirst-camera:**

```mermaid
graph TB
    subgraph EFC["edgefirst-camera Service"]
        CAMDEV["/dev/video0<br/>Camera Device"]

        subgraph VSL["VideoStream Library"]
            V4L2[V4L2 Camera<br/>Capture API]
            ENC[Hardware Encoder<br/>H.264/H.265 API]
        end

        ZENOH_PUB[Zenoh Publisher<br/>not part of VSL]
    end

    ZENOH[Zenoh Middleware]
    PERC[Perception Pipeline]

    CAMDEV --> V4L2
    V4L2 -->|DmaBuf Raw Frames| ZENOH_PUB
    V4L2 -->|DmaBuf| ENC
    ENC -->|H.264 Stream| ZENOH_PUB
    ZENOH_PUB --> ZENOH
    ZENOH --> PERC

    style V4L2 fill:#bbdefb
    style ENC fill:#bbdefb
    style ZENOH_PUB fill:#c5e1a5
    style ZENOH fill:#f3e5f5
```

### VideoStream's Role in EdgeFirst Perception

**Primary Use: Camera and Codec Utilities in edgefirst-camera**

VideoStream provides low-level camera and codec APIs used by the edgefirst-camera service:

1. **V4L2 Camera Capture** (`vsl_camera_*` API)
   - Acquires frames from camera devices (MIPI CSI-2, USB cameras)
   - Exports DmaBuf file descriptors for zero-copy operation
   - Required for DMA-capable hardware (VPU, NPU)

2. **Hardware H.264/H.265 Encoding** (`vsl_encoder_*` API)
   - Compresses frames using VPU hardware acceleration
   - Accepts DmaBuf input for zero-copy encoding
   - Produces H.264/H.265 streams for network transmission

3. **Data Flow**:
   - VideoStream captures frames and provides them to edgefirst-camera
   - edgefirst-camera publishes raw DMA camera frames directly over Zenoh (using `pidfd_getfd`)
   - Optionally, VideoStream encodes frames to H.264/H.265 for efficient streaming
   - Zenoh publisher (part of edgefirst-camera, **not VSL**) handles network distribution
   - Zenoh middleware distributes to perception pipeline

**VideoStream Does NOT Handle:**

- Network transport (handled by Zenoh middleware)
- Inter-service messaging (handled by Zenoh)
- Frame sharing between perception services (EdgeFirst uses Zenoh + `pidfd_getfd`)
- Publishing frames to Zenoh (handled by edgefirst-camera service)

**Optional Feature: Inter-Process Frame Sharing via UNIX Sockets**

VideoStream includes an optional IPC mechanism for non-EdgeFirst applications:

- **UNIX Domain Socket + FD Passing**: Share frames between local processes using SCM_RIGHTS
- **GStreamer Plugins**: vslsink (producer) and vslsrc (consumer) for multi-process pipelines
- **Use Case**: Standalone applications requiring local frame sharing WITHOUT network transport
- **Note**: EdgeFirst Perception services use Zenoh for messaging, not VSL's IPC mechanism

**Standalone Use Cases**

VideoStream can be used independently for:

- V4L2 camera capture applications
- Hardware-accelerated encoding applications
- GStreamer-based vision systems
- Multi-process video pipelines
- Embedded video I/O without perception middleware

---

## Architecture Highlights

VideoStream is organized into three main functional areas:

### 1. Frame Sharing Architecture

- **Host/Client Model**: One process hosts frame pool, multiple clients consume
- **Threading**: Multi-threaded host with GStreamer task thread for client servicing
- **IPC Protocol**: UNIX domain sockets with file descriptor passing (SCM_RIGHTS)
- **Memory Management**: DmaBuf zero-copy or POSIX shared memory fallback

### 2. V4L2 Camera Interface

- **Direct kernel interface** via ioctl() calls
- **Buffer management**: MMAP, USERPTR, or DmaBuf buffer types
- **Format negotiation**: Automatic capability detection and format selection
- **Camera controls**: Full access to V4L2 control interface

### 3. Hardware Encoding Pipeline

- **VPU wrapper**: Abstraction layer for Hantro VPU hardware
- **Zero-copy encoding**: Direct encoding from DmaBuf or camera buffers
- **Buffer recycling**: Efficient frame buffer reuse for low latency
- **Bitrate control**: Configurable CBR/VBR encoding modes

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed threading diagrams, data flow, and internal design.

---

## Integration with EdgeFirst Perception

### For edgefirst-camera Developers

VideoStream provides the camera I/O and encoding foundation:

```c
// Typical edgefirst-camera integration pattern

// 1. Open camera via VideoStream V4L2 interface
int camera = v4l2_open_device("/dev/video0");
v4l2_configure(camera, 1920, 1080, V4L2_PIX_FMT_NV12, 30);

// 2. Initialize hardware encoder
VPUEncoder *encoder = vpu_encoder_create();
vpu_encoder_configure(encoder, &encode_params);

// 3. Capture and encode loop
while (running) {
    // Capture frame from camera
    struct v4l2_buffer buffer;
    v4l2_capture_frame(camera, &buffer);

    // Encode frame using hardware VPU
    uint8_t *h264_data;
    size_t h264_size;
    vpu_encoder_encode_frame(encoder, buffer.data, &h264_data, &h264_size);

    // Publish to Zenoh middleware
    zenoh_publish("/camera/h264", h264_data, h264_size);

    // Release resources
    vpu_encoder_release_output(encoder, h264_data);
    v4l2_release_frame(camera, &buffer);
}
```

### For Application Developers

**When to use VideoStream:**

- Need V4L2 camera capture with DmaBuf export for DMA operation
- Require hardware H.264/H.265 encoding (NXP i.MX8 platforms)
- Building standalone applications needing camera + codec APIs
- Building multi-process vision pipelines with GStreamer
- Need local inter-process frame sharing without network transport

**When to use alternatives:**

- **EdgeFirst Perception middleware messaging**: For distributed perception services (uses Zenoh + `pidfd_getfd`)
- **GStreamer native elements**: For standard video processing pipelines
- **Direct V4L2 API**: For simple camera access without VideoStream abstractions

## Documentation

- 📖 **[Architecture Guide](ARCHITECTURE.md)** - Threading model, data flow, internal design
- 🔧 **[API Reference](https://doc.edgefirst.ai/test/perception/videostream/api/)** - Complete C/C++ API documentation
- 🏗️ **[EdgeFirst Perception](https://doc.edgefirst.ai/test/perception/)** - Full perception middleware docs
- 📝 **[Contributing Guide](CONTRIBUTING.md)** - Development setup and guidelines
- 🔒 **[Security Policy](SECURITY.md)** - Vulnerability reporting and security practices

## Contributing

We welcome contributions, especially for:

- Additional camera driver support and V4L2 controls
- Software encoding fallbacks (libx264, openh264)
- Platform support (Raspberry Pi, NVIDIA Jetson)
- Performance optimizations for 4K/8K capture and encoding

**Get Started:**

1. Check [CONTRIBUTING.md](CONTRIBUTING.md) for development setup
2. Check [GitHub Issues](https://github.com/EdgeFirstAI/videostream/issues) for tasks
3. Review [ARCHITECTURE.md](ARCHITECTURE.md) to understand internals
4. Follow [Code of Conduct](CODE_OF_CONDUCT.md)

---

## Support

### Community Resources

- 📒 **[Documentation](https://doc.edgefirst.ai/test/perception/videostream/)**
- 💬 **[GitHub Discussions](https://github.com/EdgeFirstAI/videostream/discussions)**
- 🐛 **[Issue Tracker](https://github.com/EdgeFirstAI/videostream/issues)**

### EdgeFirst Ecosystem

- **[EdgeFirst Perception](https://doc.edgefirst.ai/test/perception/)** - Complete perception middleware
- **[edgefirst-camera](https://doc.edgefirst.ai/test/perception/camera/)** - Camera service using VideoStream
- **[EdgeFirst Studio](https://edgefirst.studio)** - MLOps platform for edge AI deployment
- **[EdgeFirst Modules](https://www.edgefirst.ai/edgefirstmodules)** - Maivin, Raivin hardware platforms

### Commercial Support

Au-Zone Technologies offers professional services:

- Camera driver development and V4L2 integration
- Custom hardware encoding implementation
- Performance optimization for specific platforms
- Production support with SLAs

📧 **Contact**: [support@au-zone.com](mailto:support@au-zone.com)

---

## Security

**Report vulnerabilities**: [support@au-zone.com](mailto:support@au-zone.com) (Subject: "Security Vulnerability - VideoStream")

See [SECURITY.md](SECURITY.md) for our security policy and responsible disclosure process.

---

## License

Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.

---

**Part of the Au-Zone EdgeFirst ecosystem** | [au-zone.com](https://au-zone.com)
