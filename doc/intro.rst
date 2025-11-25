Introduction
============

**EdgeFirst VideoStream Library** is a high-performance video I/O library for embedded Linux applications, 
providing zero-copy frame sharing, V4L2 camera capture, and hardware-accelerated H.264/H.265 encoding.

Originally developed as part of the DeepView project, VideoStream is now the foundation for 
EdgeFirst Perception's video pipeline and is available as a standalone library for embedded vision applications.

Version: **1.5.4**

Core Capabilities
-----------------

VideoStream delivers three essential capabilities for embedded video applications:

1. **Inter-Process Frame Sharing**

   - Zero-copy buffer sharing via Linux DmaBuf and POSIX shared memory
   - Host/Client architecture supporting one producer, multiple consumers
   - Thread-safe reference counting for multi-consumer frame access
   - UNIX domain socket IPC with file descriptor passing (SCM_RIGHTS)
   - Configurable frame lifespans with automatic expiry
   - GStreamer plugins (vslsink, vslsrc) for multi-process pipelines

2. **Video4Linux2 Camera Capture**

   - V4L2 device interface for camera frame acquisition
   - DmaBuf export for zero-copy capture (required for hardware acceleration)
   - Format negotiation and capability detection
   - Multi-planar format support (YUV420, NV12, YUYV, RGB, etc.)
   - Camera controls (exposure, gain, white balance, mirroring)
   - Compatible with MIPI CSI-2, USB cameras, and other V4L2 devices

3. **Hardware H.264/H.265 Encoding**

   - Hantro VPU integration for hardware-accelerated encoding on NXP i.MX8
   - Zero-copy input from DmaBuf or camera buffers
   - Configurable bitrate profiles (5-100 Mbps)
   - Low-power encoding with minimal CPU load
   - Direct DmaBuf encoding for efficient pipeline integration

Key Features
------------

**Performance Optimized for Embedded**

- **Zero-copy operation**: DmaBuf file descriptor passing eliminates memory copies
- **Low latency**: <3ms frame distribution overhead
- **Minimal CPU usage**: <2% CPU for frame sharing at 1080p@30fps
- **Hardware acceleration**: Leverages G2D and Hantro VPU on NXP i.MX8M Plus
- **Multi-client support**: 1080p@60fps with 3 clients, 4K@30fps with 1 client

**Robust and Reliable**

- **Thread-safe**: Recursive mutexes protect concurrent access
- **Reference counting**: Automatic frame lifecycle management
- **Error handling**: Comprehensive errno-based error reporting
- **Resource cleanup**: Automatic frame expiry prevents memory leaks
- **Reconnection support**: Clients automatically reconnect to hosts

**Developer Friendly**

- **Simple C API**: Clean, well-documented interfaces
- **Rust bindings**: Safe, idiomatic Rust wrappers
- **GStreamer plugins**: No-code/low-code integration
- **Extensive examples**: Command-line tools, C samples, Python tests
- **Cross-platform**: x86_64 (development), ARM64/ARMv7 (production)

Supported Platforms
-------------------

**Hardware Platforms**

- **NXP i.MX8M Plus**: Full support (G2D, VPU, DmaBuf)
- **NXP i.MX8M**: DmaBuf + basic acceleration
- **Generic ARM64/ARMv7**: POSIX shared memory fallback
- **x86_64**: Development and testing (shared memory mode)

**Operating Systems**

- Linux kernel 4.14+ (5.6+ recommended for DmaBuf heap)
- Tested on Yocto, Ubuntu, Debian

**Dependencies**

- GStreamer 1.4+ (for plugins, optional)
- GLib 2.0+
- pthread
- Linux kernel with V4L2, DmaBuf support

Architecture Overview
---------------------

VideoStream uses a **Host/Client IPC architecture** for frame sharing:

.. mermaid::

   graph LR
       Camera[V4L2 Camera] -->|DmaBuf FD| Host[VSL Host]
       Host -->|UNIX Socket| Client1[VSL Client 1]
       Host -->|UNIX Socket| Client2[VSL Client 2]
       Host -->|UNIX Socket| Client3[VSL Client 3]
       Client1 -->|Zero-copy| App1[Application 1]
       Client2 -->|Zero-copy| App2[Application 2]
       Client3 -->|Zero-copy| App3[Application 3]

**Host (Producer)**

- Creates UNIX domain socket (e.g., ``/tmp/camera``)
- Registers frames with DmaBuf file descriptors
- Broadcasts frame availability to all connected clients
- Manages frame lifecycle and expiry

**Client (Consumer)**

- Connects to host via UNIX socket
- Receives frame metadata and DmaBuf FDs via SCM_RIGHTS
- Maps frames for read-only access
- Unlocks frames when processing complete

**Frame Lifecycle**

1. Host registers frame with ``vsl_frame_register()``
2. Frame broadcast to all clients via IPC
3. Clients lock frame with ``vsl_frame_wait()`` or ``vsl_frame_trylock()``
4. Clients access frame data (zero-copy via mmap)
5. Clients unlock with ``vsl_frame_unlock()``
6. Frame automatically released when expired or all clients unlock

GStreamer Integration
---------------------

VideoStream includes **vslsink** and **vslsrc** GStreamer plugins for seamless 
multi-process pipeline integration. These plugins expose the frame sharing 
mechanism through GStreamer's standard interface.

**Quick Example: Share Camera Between Processes**

.. code-block:: bash

   # Terminal 1: Producer - Capture and share camera
   gst-launch-1.0 v4l2src device=/dev/video0 ! \\
       video/x-raw,width=1920,height=1080,format=NV12 ! \\
       vslsink path=/tmp/camera

   # Terminal 2: Consumer - Display shared frames
   gst-launch-1.0 vslsrc path=/tmp/camera ! autovideosink

Under the hood, VideoStream automatically:

- Uses DmaBuf for zero-copy when available
- Falls back to POSIX shared memory on unsupported platforms
- Handles frame synchronization and lifecycle
- Manages client connections and disconnections

See :doc:`gstreamer` for detailed GStreamer documentation, examples, and best practices.

Programming Interfaces
----------------------

**C API** - Primary interface with comprehensive functionality

- Host API: ``vsl_host_init()``, ``vsl_frame_register()``, ``vsl_host_post()``
- Client API: ``vsl_client_init()``, ``vsl_frame_wait()``, ``vsl_frame_unlock()``
- Frame API: ``vsl_frame_alloc()``, ``vsl_frame_mmap()``, ``vsl_frame_sync()``
- Camera API: ``vsl_camera_open_device()``, ``vsl_camera_get_data()``
- Encoder API: ``vsl_encoder_create()``, ``vsl_encode_frame()``
- Decoder API: ``vsl_decoder_create()``, ``vsl_decode_frame()``

See :doc:`capi` for complete C API reference.

**Rust API** - Safe, idiomatic bindings

.. code-block:: rust

   use videostream::{Host, Frame};

   // Create host
   let host = Host::new("/tmp/camera")?;

   // Register and share frames
   let frame = Frame::register(&host, fd, width, height, fourcc, size)?;
   host.post(&frame)?;

See :doc:`Rust documentation <https://docs.rs/videostream>` for Rust API details.

**GStreamer Plugins** - No-code/low-code integration

- ``vslsink``: Publishes GStreamer buffers as VideoStream frames
- ``vslsrc``: Receives VideoStream frames as GStreamer buffers

See :doc:`gstreamer` for plugin documentation and examples.

Use Cases
---------

**Multi-Process Video Pipelines**

Share camera frames between independent processes for parallel processing:

- Object detection in one process
- Recording in another process
- Display preview in third process

**Edge AI Applications**

Integrate with EdgeFirst Perception for:

- Real-time inference on video streams
- Multi-model inference pipelines
- Low-latency video analytics

**Embedded Vision Systems**

Direct hardware integration for:

- Industrial inspection cameras
- Automotive vision systems
- Security and surveillance
- Robotics and drones

**Development Workflows**

Test and debug video applications:

- Separate UI from processing logic
- Record and replay test data
- Isolate component failures

Getting Started
---------------

**Installation**

.. code-block:: bash

   # Build from source
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   cmake --install build

   # Install Python bindings
   venv/bin/pip install .

**Quick Test**

.. code-block:: bash

   # Terminal 1: Share test pattern
   gst-launch-1.0 videotestsrc ! \\
       video/x-raw,width=640,height=480,format=NV12 ! \\
       vslsink path=/tmp/test

   # Terminal 2: Display shared frames
   gst-launch-1.0 vslsrc path=/tmp/test ! autovideosink

See :doc:`quickstart` for detailed setup instructions and examples.

Support and Resources
---------------------

**Documentation**

- EdgeFirst Docs: https://doc.edgefirst.ai/test/perception/videostream/
- API Reference: :doc:`capi`, :doc:`Rust docs <https://docs.rs/videostream>`
- GStreamer Guide: :doc:`gstreamer`
- C Samples: :doc:`csamples`

**Commercial Support**

VideoStream is commercially supported by Au-Zone Technologies:

- Email: support@au-zone.com
- GitHub: https://github.com/EdgeFirstAI/videostream
- Issues: https://github.com/EdgeFirstAI/videostream/issues

**License**

Apache License 2.0 - See LICENSE file for details.

**Contributing**

Contributions welcome! See CONTRIBUTING.md for guidelines.

