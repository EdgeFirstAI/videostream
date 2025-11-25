GStreamer
=========

VideoStream provides **vslsink** and **vslsrc** GStreamer plugins for zero-copy, 
multi-process video pipeline integration. These plugins enable sharing camera frames 
and video streams between independent processes with minimal CPU overhead.

Overview
--------

The VideoStream GStreamer plugins implement the Host/Client IPC architecture:

- **vslsink**: Producer element that publishes GStreamer buffers as VideoStream frames
- **vslsrc**: Consumer element that receives VideoStream frames as GStreamer buffers

**Key Features**

- Zero-copy frame sharing via DmaBuf (Linux)
- Automatic fallback to POSIX shared memory
- Multi-client support (one producer, many consumers)
- Thread-safe frame lifecycle management
- Automatic reconnection on client disconnect
- Configurable frame lifespans
- PTS/DTS timestamp preservation

Architecture
------------

VideoStream enables multi-process GStreamer pipelines by passing DmaBuf file descriptors 
between processes via UNIX domain sockets:

.. mermaid::

   graph TB
       subgraph Producer["Process 1: Producer"]
           V4L2[v4l2src] -->|DmaBuf| VSLSink[vslsink]
           VSLSink -->|UNIX Socket| Socket1[UNIX Socket]
       end
       
       subgraph Consumer1["Process 2: Consumer 1"]
           Socket2[UNIX Socket] -->|Frame Events| VSLSrc1[vslsrc]
           VSLSrc1 -->|DmaBuf| Display[waylandsink]
       end
       
       subgraph Consumer2["Process 3: Consumer 2"]
           Socket3[UNIX Socket] -->|Frame Events| VSLSrc2[vslsrc]
           VSLSrc2 -->|DmaBuf| Encoder[x264enc]
           Encoder --> File[filesink]
       end
       
       Socket1 -.->|IPC /tmp/camera| Socket2
       Socket1 -.->|IPC /tmp/camera| Socket3

**Data Flow**

1. Producer pipeline captures frames (e.g., v4l2src)
2. vslsink receives GStreamer buffers with DmaBuf memory
3. vslsink registers frames with VSL Host and broadcasts to UNIX socket
4. vslsrc clients connect to socket and receive frame notifications
5. vslsrc locks frames, wraps DmaBuf FDs as GStreamer buffers
6. Consumer pipelines process frames (display, encode, analyze)
7. vslsrc unlocks frames when buffers are released
8. Host drops expired frames when all clients unlock

Installation
------------

**Build GStreamer Plugins**

The plugins are built automatically when GStreamer development files are detected:

.. code-block:: bash

   # Install GStreamer development packages
   sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev

   # Build VideoStream with GStreamer support
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_GSTREAMER=ON
   cmake --build build -j$(nproc)

   # Install plugins to system directory
   sudo cmake --install build

**Verify Installation**

.. code-block:: bash

   # Check if plugins are registered
   gst-inspect-1.0 vslsink
   gst-inspect-1.0 vslsrc

   # Expected output
   Factory Details:
     Rank                     none (0)
     Long-name                VideoStream Sink
     Klass                    Sink/Video
     Description              DMA-enabled cross-process GStreamer pipeline

**Manual Plugin Path** (if not installed system-wide)

.. code-block:: bash

   export GST_PLUGIN_PATH=/path/to/videostream/build:$GST_PLUGIN_PATH

vslsink Element
---------------

**Element Type**: Video Sink (``GstVideoSink``)

**Purpose**: Publishes GStreamer video buffers as VideoStream frames for consumption 
by other processes.

**Properties**

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Property
     - Type
     - Default
     - Description
   * - ``path``
     - String
     - ``/tmp/<name>.<tid>``
     - UNIX socket path for IPC. Auto-generated if not specified.
   * - ``lifespan``
     - Int64 (ms)
     - ``100``
     - Frame lifespan in milliseconds. Frames expire after this duration.

**Supported Formats**

vslsink accepts the following raw video formats:

- **Planar YUV**: NV12, YV12, I420
- **Packed YUV**: YUY2, YUYV, UYVY
- **RGB**: RGB, BGR, RGBA, RGBx, BGRA, BGRx

**Requirements**

- Input buffers **must use DmaBuf memory** (``GstDmaBufAllocator``)
- vslsink will reject buffers without DmaBuf backing
- Most hardware sources (v4l2src, waylandsrc) provide DmaBuf naturally

**Example Usage**

.. code-block:: bash

   # Basic usage with auto-generated path
   gst-launch-1.0 v4l2src ! video/x-raw,format=NV12 ! vslsink

   # Specify custom socket path
   gst-launch-1.0 v4l2src ! video/x-raw,format=NV12 ! \\
       vslsink path=/tmp/my_camera

   # Increase frame lifespan to 500ms
   gst-launch-1.0 v4l2src ! video/x-raw,format=NV12 ! \\
       vslsink path=/tmp/camera lifespan=500

**Internal Behavior**

- Creates VSL Host on ``start()``
- Launches background GstTask for client servicing (``vsl_host_poll()``, ``vsl_host_service()``)
- On each frame:
  1. Extracts DmaBuf FD from GstBuffer
  2. Registers frame with ``vsl_frame_register()``
  3. Attaches GstMemory reference to frame userdata
  4. Frame cleanup callback unrefs GstMemory when expired
- Processes host events with 1000ms poll timeout
- Stops task and releases host on ``stop()``

vslsrc Element
--------------

**Element Type**: Push Source (``GstPushSrc``)

**Purpose**: Receives VideoStream frames from a producer and provides them as 
GStreamer buffers.

**Properties**

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Property
     - Type
     - Default
     - Description
   * - ``path``
     - String
     - ``/tmp/<name>.<tid>``
     - UNIX socket path to connect to host. Must match vslsink path.
   * - ``timeout``
     - Float (seconds)
     - ``0.0``
     - Socket timeout in seconds. 0 = infinite wait.
   * - ``dts``
     - Boolean
     - ``FALSE``
     - Set DTS timestamp from frame metadata.
   * - ``pts``
     - Boolean
     - ``FALSE``
     - Set PTS timestamp from frame metadata.
   * - ``reconnect``
     - Boolean
     - ``FALSE``
     - Automatically reconnect if connection lost.

**Supported Formats**

vslsrc auto-negotiates caps based on received frame metadata:

- Detects format from FourCC code
- Configures width, height, framerate from first frame
- Outputs DmaBuf-backed GstBuffers (``GstDmaBufMemory``)

**Example Usage**

.. code-block:: bash

   # Basic usage - connect to host
   gst-launch-1.0 vslsrc path=/tmp/camera ! autovideosink

   # Enable reconnection
   gst-launch-1.0 vslsrc path=/tmp/camera reconnect=true ! autovideosink

   # Set socket timeout to 5 seconds
   gst-launch-1.0 vslsrc path=/tmp/camera timeout=5.0 ! autovideosink

   # Preserve PTS timestamps from producer
   gst-launch-1.0 vslsrc path=/tmp/camera pts=true ! autovideosink

**Internal Behavior**

- Creates VSL Client on ``start()``
- Configures live source with ``gst_base_src_set_live(TRUE)``
- On each frame request (``create()``):
  1. Calls ``vsl_frame_wait()`` to lock next frame
  2. Extracts frame metadata (width, height, fourcc, timestamp)
  3. Negotiates caps if format changed
  4. Wraps DmaBuf FD in GstDmaBufMemory
  5. Creates GstBuffer with memory and metadata
  6. Attaches frame unlock callback to buffer dispose
- Unlocks frame with ``vsl_frame_unlock()`` when buffer finalized
- Disconnects and releases client on ``stop()``

Complete Pipeline Examples
--------------------------

Camera Sharing Pipeline
~~~~~~~~~~~~~~~~~~~~~~~

Share camera between display and recording processes:

.. mermaid::

   graph TB
       subgraph "Process 1: Camera Producer"
           Camera[v4l2src<br/>device=/dev/video0] -->|1920x1080 NV12| Caps1[capsfilter]
           Caps1 --> Sink[vslsink<br/>path=/tmp/camera]
       end
       
       subgraph "Process 2: Display Consumer"
           Src1[vslsrc<br/>path=/tmp/camera] --> Convert1[videoconvert]
           Convert1 --> Display[waylandsink]
       end
       
       subgraph "Process 3: Recording Consumer"
           Src2[vslsrc<br/>path=/tmp/camera] --> Encoder[x264enc]
           Encoder --> Muxer[mp4mux]
           Muxer --> File[filesink<br/>location=output.mp4]
       end

**Terminal 1: Producer**

.. code-block:: bash

   gst-launch-1.0 v4l2src device=/dev/video0 ! \\
       video/x-raw,width=1920,height=1080,format=NV12,framerate=30/1 ! \\
       vslsink path=/tmp/camera lifespan=200

**Terminal 2: Display**

.. code-block:: bash

   gst-launch-1.0 vslsrc path=/tmp/camera reconnect=true ! \\
       videoconvert ! \\
       waylandsink

**Terminal 3: Recording**

.. code-block:: bash

   gst-launch-1.0 vslsrc path=/tmp/camera reconnect=true ! \\
       x264enc tune=zerolatency bitrate=5000 ! \\
       mp4mux ! \\
       filesink location=camera_recording.mp4

Test Pattern Pipeline
~~~~~~~~~~~~~~~~~~~~~

Test multi-client setup with videotestsrc:

.. mermaid::

   graph LR
       Test[videotestsrc<br/>pattern=ball] --> Caps[video/x-raw<br/>640x480 NV12]
       Caps --> Sink[vslsink<br/>path=/tmp/test]
       Sink -.IPC.-> Src1[vslsrc]
       Sink -.IPC.-> Src2[vslsrc]
       Src1 --> Out1[autovideosink]
       Src2 --> Out2[xvimagesink]

**Terminal 1: Producer**

.. code-block:: bash

   gst-launch-1.0 videotestsrc pattern=ball ! \\
       video/x-raw,width=640,height=480,format=NV12,framerate=30/1 ! \\
       vslsink path=/tmp/test

**Terminal 2: Consumer 1**

.. code-block:: bash

   gst-launch-1.0 vslsrc path=/tmp/test ! autovideosink

**Terminal 3: Consumer 2**

.. code-block:: bash

   gst-launch-1.0 vslsrc path=/tmp/test ! xvimagesink

RTSP Streaming Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~

Stream camera over RTSP to multiple clients:

.. mermaid::

   graph TB
       subgraph "Process 1: Camera Source"
           V4L2[v4l2src] --> Caps[video/x-raw<br/>1920x1080 NV12]
           Caps --> VSLSink[vslsink<br/>path=/tmp/camera]
       end
       
       subgraph "Process 2: RTSP Server"
           VSLSrc[vslsrc<br/>path=/tmp/camera] --> Encode[x264enc]
           Encode --> RTP[rtph264pay]
           RTP --> RTSP[rtspsink<br/>service=8554]
       end
       
       subgraph "Clients"
           RTSP -.RTSP Stream.-> Client1[VLC Player]
           RTSP -.RTSP Stream.-> Client2[Browser]
       end

**Terminal 1: Camera Capture**

.. code-block:: bash

   gst-launch-1.0 v4l2src device=/dev/video0 ! \\
       video/x-raw,width=1920,height=1080,format=NV12 ! \\
       vslsink path=/tmp/camera

**Terminal 2: RTSP Server**

.. code-block:: bash

   gst-launch-1.0 vslsrc path=/tmp/camera ! \\
       x264enc tune=zerolatency bitrate=8000 ! \\
       rtph264pay config-interval=1 pt=96 ! \\
       udpsink host=127.0.0.1 port=8554

**Client Playback**

.. code-block:: bash

   # VLC
   vlc rtsp://127.0.0.1:8554/stream

   # GStreamer
   gst-launch-1.0 rtspsrc location=rtsp://127.0.0.1:8554/stream ! \\
       rtph264depay ! h264parse ! avdec_h264 ! autovideosink

Hardware Encoding Pipeline (i.MX8)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use Hantro VPU for hardware encoding on NXP i.MX8:

.. mermaid::

   graph LR
       Camera[v4l2src] --> Caps[video/x-raw<br/>NV12]
       Caps --> VSLSink[vslsink<br/>path=/tmp/camera]
       VSLSink -.IPC.-> VSLSrc[vslsrc]
       VSLSrc --> VPU[vpuenc_h264]
       VPU --> Mux[mp4mux]
       Mux --> File[filesink]

**Terminal 1: Camera**

.. code-block:: bash

   gst-launch-1.0 v4l2src device=/dev/video0 ! \\
       video/x-raw,width=1920,height=1080,format=NV12 ! \\
       vslsink path=/tmp/camera

**Terminal 2: Hardware Encode**

.. code-block:: bash

   gst-launch-1.0 vslsrc path=/tmp/camera ! \\
       vpuenc_h264 bitrate=10000 gop-size=30 ! \\
       h264parse ! \\
       mp4mux ! \\
       filesink location=hardware_encoded.mp4

Timestamp Handling
------------------

VideoStream preserves frame timing information through the pipeline:

**Frame Metadata**

- **Serial**: Buffer offset (``GST_BUFFER_OFFSET``)
- **PTS**: Presentation timestamp (``GST_BUFFER_PTS``)
- **DTS**: Decode timestamp (``GST_BUFFER_DTS``)
- **Duration**: Frame duration (``GST_BUFFER_DURATION``)

**vslsink Behavior**

Extracts timestamps from input buffers and stores in frame metadata:

.. code-block:: c

   int64_t duration = GST_BUFFER_DURATION(buffer);
   int64_t pts = GST_BUFFER_PTS(buffer);
   int64_t dts = GST_BUFFER_DTS(buffer);
   int64_t serial = GST_BUFFER_OFFSET(buffer);
   
   vsl_frame_register(..., duration, pts, dts, ...);

**vslsrc Behavior**

By default, vslsrc uses ``gst_base_src_set_do_timestamp(TRUE)`` to generate 
timestamps based on pipeline clock. Enable ``pts`` or ``dts`` properties to 
use frame metadata timestamps instead:

.. code-block:: bash

   # Use PTS from producer
   gst-launch-1.0 vslsrc path=/tmp/camera pts=true ! ...

   # Use DTS from producer
   gst-launch-1.0 vslsrc path=/tmp/camera dts=true ! ...

Debugging
---------

**Enable Debug Logging**

.. code-block:: bash

   # Show all VideoStream plugin messages
   GST_DEBUG=vslsink:5,vslsrc:5 gst-launch-1.0 ...

   # Show frame-level traces
   GST_DEBUG=vslsink:6,vslsrc:6 gst-launch-1.0 ...

**Common Log Messages**

.. code-block:: text

   # vslsink
   vslsink:INFO creating vsl host on /tmp/camera
   vslsink:LOG dmabuf fd:42 size:3133440 offset:0 1920x1080 format=NV12
   vslsink:TRACE frame 1234 broadcast

   # vslsrc
   vslsrc:INFO creating vsl client to /tmp/camera
   vslsrc:LOG received frame serial:1234 1920x1080 NV12
   vslsrc:TRACE frame 1234 locked

**Inspect Element Details**

.. code-block:: bash

   # Show vslsink properties and pads
   gst-inspect-1.0 vslsink

   # Show vslsrc properties and pads
   gst-inspect-1.0 vslsrc

**Monitor Frame Flow**

.. code-block:: bash

   # Use fpsdisplaysink to monitor framerate
   gst-launch-1.0 vslsrc path=/tmp/camera ! \\
       fpsdisplaysink video-sink=autovideosink text-overlay=false

Troubleshooting
---------------

**Error: "vslsink requires dmabuf enabled memory"**

**Cause**: Input buffers don't use DmaBuf allocator.

**Solution**: Ensure upstream element provides DmaBuf memory. Most hardware sources 
(v4l2src) do this automatically. For software sources, use:

.. code-block:: bash

   # Force DmaBuf allocation (requires v4l2 plugin)
   gst-launch-1.0 videotestsrc ! \\
       video/x-raw,format=NV12 ! \\
       v4l2convert ! \\
       vslsink

**Error: "failed to initialize vsl client"**

**Cause**: Cannot connect to host socket.

**Solutions**:

1. Verify producer is running and socket exists:

   .. code-block:: bash

      ls -l /tmp/camera

2. Check socket path matches between producer and consumer
3. Enable reconnection:

   .. code-block:: bash

      gst-launch-1.0 vslsrc path=/tmp/camera reconnect=true ! ...

**Error: "failed to lock frame - timeout"**

**Cause**: No frames received within timeout period.

**Solutions**:

1. Increase socket timeout:

   .. code-block:: bash

      gst-launch-1.0 vslsrc path=/tmp/camera timeout=10.0 ! ...

2. Check producer is sending frames:

   .. code-block:: bash

      GST_DEBUG=vslsink:5 gst-launch-1.0 ...

3. Verify producer hasn't stopped or crashed

**Warning: "cannot adjust path once set"**

**Cause**: Attempted to change ``path`` property after element started.

**Solution**: Set path before transitioning to PAUSED/PLAYING state:

.. code-block:: bash

   # Correct
   gst-launch-1.0 vslsink path=/tmp/camera ! ...
   
   # Incorrect - path set dynamically during playback

**Performance Issues**

1. **High CPU usage**: Ensure DmaBuf zero-copy is working (check with ``GST_DEBUG=vslsink:5``)
2. **Frame drops**: Increase lifespan or reduce producer framerate
3. **Latency**: Reduce lifespan, ensure real-time scheduling

Best Practices
--------------

**Producer (vslsink)**

- Use DmaBuf-enabled sources (v4l2src, waylandsrc, hardware decoders)
- Set explicit caps for deterministic format negotiation
- Configure lifespan based on client processing time (100-500ms typical)
- Use unique socket paths for multiple producers

**Consumer (vslsrc)**

- Enable ``reconnect=true`` for robust client applications
- Set reasonable timeouts for production systems
- Use ``pts=true`` when timestamp accuracy matters (encoding, sync)
- Process frames quickly to avoid blocking other clients

**Multi-Client Systems**

- Ensure all clients unlock frames promptly
- Monitor frame expiry (increase lifespan if clients drop frames)
- Use separate pipelines per client (separate processes)
- Consider client priority (display > recording > analytics)

**Production Deployment**

- Use systemd for automatic restart
- Monitor socket files (``/tmp`` may be cleaned on boot)
- Log pipeline errors for debugging
- Test reconnection scenarios

Performance Characteristics
---------------------------

**Measured on NXP i.MX8M Plus (Quad Cortex-A53 @ 1.8GHz)**

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Configuration
     - Latency
     - CPU Usage
     - Throughput
   * - 1080p30 NV12, 1 client
     - <3ms
     - <1%
     - 60MB/s
   * - 1080p30 NV12, 3 clients
     - <5ms
     - <2%
     - 180MB/s
   * - 4K30 NV12, 1 client
     - <5ms
     - <2%
     - 240MB/s
   * - 720p60 NV12, 2 clients
     - <4ms
     - 1.5%
     - 120MB/s

**Notes**:

- Latency: Time from vslsink frame registration to vslsrc buffer output
- CPU usage: Combined producer + all consumers
- Zero-copy via DmaBuf eliminates memory bandwidth bottleneck
- Throughput limited by IPC overhead, not memory copies

API Reference
-------------

For detailed C API documentation used internally by GStreamer plugins, see:

- :doc:`capi` - Complete C API reference
- ``include/videostream.h`` - Public API header

For GStreamer plugin source code:

- ``gst/vslsink.c`` - vslsink implementation
- ``gst/vslsrc.c`` - vslsrc implementation

License
-------

GStreamer plugins are licensed under Apache License 2.0, same as VideoStream Library.

Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
