// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * @file test_host.c
 * @brief VideoStream Host Test - Frame Producer
 *
 * This test demonstrates the core VideoStream C API for inter-process frame
 * sharing. The host creates frames, allocates memory, and publishes them to
 * connected clients.
 *
 * Usage:
 *   ./test_host [socket_path]
 *
 * Default socket: /tmp/videostream_test.sock
 *
 * Requirements:
 *   - DMA heap access (/dev/dma_heap/system) OR run with sudo
 *   - User in 'video' group for DMA heap access
 *
 * Example:
 *   # Terminal 1: Start host
 *   ./test_host
 *
 *   # Terminal 2: Start client
 *   ./test_client
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <videostream.h>

#define DEFAULT_SOCKET_PATH "/tmp/videostream_test.sock"
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define FRAME_LIFESPAN_NS 1000000000LL // 1 second
#define FRAME_DURATION_NS 33333333LL   // ~30fps

static volatile bool g_running = true;

static void
signal_handler(int sig)
{
    (void) sig;
    g_running = false;
}

/**
 * Check if DMA heap is available and accessible
 */
static int
check_dma_heap_access(void)
{
    const char* dma_heap_path = "/dev/dma_heap/system";
    struct stat st;

    // Check if DMA heap device exists
    if (stat(dma_heap_path, &st) != 0) {
        if (errno == ENOENT) {
            fprintf(stderr,
                    "INFO: DMA heap not available (%s does not exist)\n",
                    dma_heap_path);
            fprintf(stderr,
                    "      This is normal on systems without DMA heap "
                    "support.\n");
            fprintf(stderr, "      Will use POSIX shared memory instead.\n\n");
            return 0; // Not an error - just use fallback
        }
        perror("stat(/dev/dma_heap/system)");
        return -1;
    }

    // Check if we have read/write permissions
    if (access(dma_heap_path, R_OK | W_OK) != 0) {
        fprintf(stderr,
                "ERROR: No access to DMA heap device: %s\n",
                dma_heap_path);
        fprintf(stderr, "       Current permissions: ");

        // Show current permissions
        printf("%c%c%c%c%c%c%c%c%c\n",
               (st.st_mode & S_IRUSR) ? 'r' : '-',
               (st.st_mode & S_IWUSR) ? 'w' : '-',
               (st.st_mode & S_IXUSR) ? 'x' : '-',
               (st.st_mode & S_IRGRP) ? 'r' : '-',
               (st.st_mode & S_IWGRP) ? 'w' : '-',
               (st.st_mode & S_IXGRP) ? 'x' : '-',
               (st.st_mode & S_IROTH) ? 'r' : '-',
               (st.st_mode & S_IWOTH) ? 'w' : '-',
               (st.st_mode & S_IXOTH) ? 'x' : '-');

        fprintf(stderr, "\n");
        fprintf(stderr, "SOLUTIONS:\n");
        fprintf(stderr, "  1. Add your user to the 'video' group:\n");
        fprintf(stderr, "     sudo usermod -a -G video $USER\n");
        fprintf(stderr, "     (then log out and log back in)\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "  2. Run with sudo:\n");
        fprintf(stderr, "     sudo ./test_host\n");
        fprintf(stderr, "\n");
        fprintf(stderr,
                "  3. Set permissions (not recommended for production):\n");
        fprintf(stderr, "     sudo chmod 666 %s\n", dma_heap_path);
        fprintf(stderr, "\n");

        return -1;
    }

    printf("✓ DMA heap access OK: %s\n", dma_heap_path);
    return 1; // DMA heap available and accessible
}

/**
 * Fill frame with test pattern (gradient)
 */
static void
fill_test_pattern(void* data, size_t size, int frame_number)
{
    unsigned char* pixels = (unsigned char*) data;

    // Simple gradient pattern that changes with frame number
    for (size_t i = 0; i < size; i++) {
        pixels[i] = (unsigned char) ((i + frame_number * 10) % 256);
    }
}

int
main(int argc, char* argv[])
{
    const char* socket_path = DEFAULT_SOCKET_PATH;
    VSLHost*    host        = NULL;
    VSLFrame*   frame       = NULL;
    int         ret         = 1;
    int         frame_count = 0;

    // Parse command-line arguments
    if (argc > 1) { socket_path = argv[1]; }

    printf("==================================================================="
           "==========\n");
    printf("VideoStream Host Test - Frame Producer\n");
    printf("==================================================================="
           "==========\n");
    printf("Version: %s\n", vsl_version());
    printf("Socket:  %s\n", socket_path);
    printf("Format:  %dx%d NV12\n", FRAME_WIDTH, FRAME_HEIGHT);
    printf("==================================================================="
           "==========\n\n");

    // Check DMA heap access
    printf("Checking system requirements...\n");
    int dma_status = check_dma_heap_access();
    if (dma_status < 0) { return 1; }
    printf("\n");

    // Set up signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create host on UNIX socket
    printf("Creating host on socket: %s\n", socket_path);
    host = vsl_host_init(socket_path);
    if (!host) {
        fprintf(stderr, "ERROR: Failed to create host: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("✓ Host created successfully\n");
    printf("  Path: %s\n\n", vsl_host_path(host));

    // Create a 1920x1080 NV12 frame
    printf("Creating frame: %dx%d NV12\n", FRAME_WIDTH, FRAME_HEIGHT);
    frame = vsl_frame_init(FRAME_WIDTH,                    // width
                           FRAME_HEIGHT,                   // height
                           0,                              // stride (0=auto)
                           VSL_FOURCC('N', 'V', '1', '2'), // format
                           NULL,                           // userptr
                           NULL);                          // cleanup callback

    if (!frame) {
        fprintf(stderr, "ERROR: Failed to create frame: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("✓ Frame created successfully\n");

    // Allocate backing memory (DmaBuf or shared memory)
    printf("Allocating frame memory...\n");
    if (dma_status > 0) { printf("  Attempting DMA heap allocation...\n"); }
    if (vsl_frame_alloc(frame, NULL) < 0) {
        fprintf(stderr,
                "ERROR: Failed to allocate frame: %s\n",
                strerror(errno));
        goto cleanup;
    }
    printf("✓ Frame allocated: %dx%d, %d bytes\n",
           vsl_frame_width(frame),
           vsl_frame_height(frame),
           vsl_frame_size(frame));

    const char* frame_path = vsl_frame_path(frame);
    if (frame_path) {
        printf("  Memory type: %s\n",
               strstr(frame_path, "/dev/") ? "DMA heap (zero-copy)"
                                           : "POSIX shared memory");
        printf("  Path: %s\n", frame_path);
    }
    printf("\n");

    // Map frame for writing
    void* data = vsl_frame_mmap(frame, NULL);
    if (!data) {
        fprintf(stderr, "ERROR: Failed to map frame: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("✓ Frame mapped for access\n\n");

    printf("==================================================================="
           "==========\n");
    printf("Waiting for clients to connect...\n");
    printf("Press Ctrl+C to stop\n");
    printf("==================================================================="
           "==========\n\n");

    // Main loop: publish frames to clients
    while (g_running) {
        // Fill with test pattern
        fill_test_pattern(data, vsl_frame_size(frame), frame_count);

        // Post frame to clients (expires in 1 second)
        int64_t now     = vsl_timestamp();
        int64_t expires = now + FRAME_LIFESPAN_NS;
        int64_t pts     = frame_count * FRAME_DURATION_NS;

        if (vsl_host_post(host, frame, expires, FRAME_DURATION_NS, pts, pts) <
            0) {
            fprintf(stderr,
                    "ERROR: Failed to post frame: %s\n",
                    strerror(errno));
            // Recreate frame for next iteration
            frame = vsl_frame_init(FRAME_WIDTH,
                                   FRAME_HEIGHT,
                                   0,
                                   VSL_FOURCC('N', 'V', '1', '2'),
                                   NULL,
                                   NULL);
            if (!frame) {
                fprintf(stderr, "ERROR: Failed to recreate frame\n");
                break;
            }
            if (vsl_frame_alloc(frame, NULL) < 0) {
                fprintf(stderr, "ERROR: Failed to reallocate frame\n");
                break;
            }
            data = vsl_frame_mmap(frame, NULL);
            if (!data) {
                fprintf(stderr, "ERROR: Failed to remap frame\n");
                break;
            }
        } else {
            frame_count++;
            if (frame_count % 30 == 0) {
                printf("Published %d frames (%.1f seconds)\n",
                       frame_count,
                       frame_count / 30.0);
            }

            // Recreate frame for next iteration (previous one was posted to
            // host)
            frame = vsl_frame_init(FRAME_WIDTH,
                                   FRAME_HEIGHT,
                                   0,
                                   VSL_FOURCC('N', 'V', '1', '2'),
                                   NULL,
                                   NULL);
            if (!frame) {
                fprintf(stderr, "ERROR: Failed to recreate frame\n");
                break;
            }
            if (vsl_frame_alloc(frame, NULL) < 0) {
                fprintf(stderr, "ERROR: Failed to reallocate frame\n");
                break;
            }
            data = vsl_frame_mmap(frame, NULL);
            if (!data) {
                fprintf(stderr, "ERROR: Failed to remap frame\n");
                break;
            }
        }

        // Process host events (accept clients, expire frames)
        if (vsl_host_poll(host, 100) > 0) { vsl_host_process(host); }

        // Limit to ~30fps
        usleep(33333);
    }

    printf("\n================================================================="
           "============\n");
    printf("Shutting down...\n");
    printf("Published %d total frames\n", frame_count);
    printf("==================================================================="
           "==========\n");

    ret = 0;

cleanup:
    if (frame) {
        vsl_frame_munmap(frame);
        vsl_frame_release(frame);
    }
    if (host) { vsl_host_release(host); }

    return ret;
}
