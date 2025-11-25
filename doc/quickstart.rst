Quick Start
===========

This Quick Start guide demonstrates how to use VideoStream Library for V4L2
camera capture, hardware encoding, and inter-process frame sharing on embedded
Linux platforms. The examples are tested on NXP i.MX 8M Plus EVK, but the
library supports various platforms including ARM64, ARMv7, and x86_64.

For platform-specific requirements and additional platform support, please
contact support@au-zone.com.

Installation
------------

VideoStream Library can be installed from source or via platform-specific
packages. For detailed build instructions, refer to the main README.md.

**Build from Source:**

.. code-block:: bash

   git clone https://github.com/EdgeFirstAI/videostream.git
   cd videostream
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   sudo cmake --install build

**Prerequisites:**

- CMake 3.10+
- GCC/Clang with C11 support
- Linux kernel 5.15+
- GStreamer 1.4+ development libraries (required only when building GStreamer plugins)

Basic C API Example
-------------------

This example demonstrates the core VideoStream C API for inter-process frame
sharing. A host process creates and shares frames with a client process using
zero-copy DmaBuf or POSIX shared memory.

**Host Process** (Producer)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The host creates frames, allocates memory, and publishes them to connected clients:

.. code-block:: c

   #include <videostream.h>
   #include <stdio.h>
   #include <unistd.h>

   int main() {
       // Create host on UNIX socket
       VSLHost* host = vsl_host_init("/tmp/videostream.sock");
       if (!host) {
           perror("Failed to create host");
           return 1;
       }

       printf("Host started on %s\n", vsl_host_path(host));

       // Create a 1920x1080 NV12 frame
       VSLFrame* frame = vsl_frame_init(
           1920,                          // width
           1080,                          // height
           0,                             // stride (0=auto)
           VSL_FOURCC('N','V','1','2'),   // format
           NULL,                          // userptr
           NULL);                         // cleanup callback

       if (!frame) {
           perror("Failed to create frame");
           vsl_host_release(host);
           return 1;
       }

       // Allocate backing memory (DmaBuf or shared memory)
       if (vsl_frame_alloc(frame, NULL) < 0) {
           perror("Failed to allocate frame");
           vsl_frame_release(frame);
           vsl_host_release(host);
           return 1;
       }

       printf("Frame allocated: %dx%d, %zu bytes\n",
              vsl_frame_width(frame),
              vsl_frame_height(frame),
              vsl_frame_size(frame));

       // Map frame for writing
       void* data = vsl_frame_mmap(frame, NULL);
       if (!data) {
           perror("Failed to map frame");
           vsl_frame_release(frame);
           vsl_host_release(host);
           return 1;
       }

       // Fill with test pattern (e.g., gradient)
       // ... your frame generation code here ...

       vsl_frame_munmap(frame);

       // Post frame to clients (expires in 1 second)
       int64_t now = vsl_timestamp();
       int64_t expires = now + 1000000000LL;  // +1 second

       if (vsl_host_post(host, frame, expires, -1, -1, -1) < 0) {
           perror("Failed to post frame");
           vsl_frame_release(frame);
           vsl_host_release(host);
           return 1;
       }

       printf("Frame posted, waiting for clients...\n");

       // Process host events (accept clients, expire frames)
       while (1) {
           if (vsl_host_poll(host, 100) > 0) {
               vsl_host_process(host);
           }
       }

       vsl_host_release(host);
       return 0;
   }

**Client Process** (Consumer)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The client connects to the host and receives shared frames:

.. code-block:: c

   #include <videostream.h>
   #include <stdio.h>
   #include <unistd.h>

   int main() {
       // Connect to host
       VSLClient* client = vsl_client_init(
           "/tmp/videostream.sock",  // socket path
           NULL,                     // userptr
           true);                    // auto-reconnect

       if (!client) {
           perror("Failed to connect to host");
           return 1;
       }

       printf("Connected to %s\n", vsl_client_path(client));

       // Wait for frames
       while (1) {
           VSLFrame* frame = vsl_frame_wait(client, 0);
           if (!frame) {
               perror("Failed to receive frame");
               break;
           }

           printf("Received frame %ld: %dx%d, fourcc=%c%c%c%c\n",
                  vsl_frame_serial(frame),
                  vsl_frame_width(frame),
                  vsl_frame_height(frame),
                  (vsl_frame_fourcc(frame) >> 0) & 0xFF,
                  (vsl_frame_fourcc(frame) >> 8) & 0xFF,
                  (vsl_frame_fourcc(frame) >> 16) & 0xFF,
                  (vsl_frame_fourcc(frame) >> 24) & 0xFF);

           // Lock frame for access
           if (vsl_frame_trylock(frame) == 0) {
               // Map frame data
               void* data = vsl_frame_mmap(frame, NULL);
               if (data) {
                   // Process frame data...
                   // ... your processing code here ...

                   vsl_frame_munmap(frame);
               }

               vsl_frame_unlock(frame);
           }

           vsl_frame_release(frame);
       }

       vsl_client_release(client);
       return 0;
   }

**Compilation:**

.. code-block:: bash

   # Host
   gcc -o host host.c -lvideostream

   # Client
   gcc -o client client.c -lvideostream

   # Run (in separate terminals)
   ./host
   ./client

**Production-Ready Examples:**

For complete, production-ready implementations with DMA heap permission checking,
signal handling, error recovery, and statistics tracking, see the comprehensive
examples in the source repository:

- ``src/test_host.c`` - Frame producer with command-line arguments, test pattern generation, and detailed diagnostics
- ``src/test_client.c`` - Frame consumer with checksum validation, FPS statistics, and robust error handling
- ``scripts/test_host_client.sh`` - Integration test script demonstrating host/client coordination

These examples can be built and run using:

.. code-block:: bash

   # Build test executables
   cmake --build build --target vsl-test-host vsl-test-client

   # Run integration test (requires DMA heap access or sudo)
   make test-ipc

   # Or run individually
   ./build/vsl-test-host /tmp/videostream_test.sock
   ./build/vsl-test-client /tmp/videostream_test.sock 30

GStreamer Integration
---------------------

For multi-process video pipelines using GStreamer, see the :doc:`gstreamer`
documentation which covers the ``vslsink`` and ``vslsrc`` plugins.
