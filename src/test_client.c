// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * @file test_client.c
 * @brief VideoStream Client Test - Frame Consumer
 *
 * This test demonstrates the core VideoStream C API for inter-process frame
 * sharing. The client connects to a host and receives shared frames.
 *
 * Usage:
 *   ./test_client [socket_path] [num_frames]
 *
 * Default socket: /tmp/videostream_test.sock
 * Default frames: 100 (0 = infinite)
 *
 * Requirements:
 *   - test_host must be running first
 *
 * Example:
 *   # Terminal 1: Start host
 *   ./test_host
 *
 *   # Terminal 2: Start client (receive 100 frames)
 *   ./test_client
 *
 *   # Terminal 3: Start another client (receive infinite frames)
 *   ./test_client /tmp/videostream_test.sock 0
 */

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <videostream.h>

#define DEFAULT_SOCKET_PATH "/tmp/videostream_test.sock"
#define DEFAULT_NUM_FRAMES 100
#define SEPARATOR "==========================================================================="

static volatile bool g_running = true;

static void
signal_handler(int sig)
{
    (void) sig;
    g_running = false;
}

/**
 * Calculate checksum of frame data (simple validation)
 */
static unsigned long
calculate_checksum(const void* data, size_t size)
{
    const unsigned char* bytes = (const unsigned char*) data;
    unsigned long        sum   = 0;

    for (size_t i = 0; i < size; i++) { sum += bytes[i]; }

    return sum;
}

/**
 * Print frame statistics
 */
static void
print_frame_stats(VSLFrame* frame, int frame_num, unsigned long checksum)
{
    int64_t serial    = vsl_frame_serial(frame);
    int64_t timestamp = vsl_frame_timestamp(frame);
    int64_t pts       = vsl_frame_pts(frame);
    int64_t duration  = vsl_frame_duration(frame);

    uint32_t fourcc        = vsl_frame_fourcc(frame);
    char     fourcc_str[5] = {(char) ((fourcc >> 0) & 0xFF),
                              (char) ((fourcc >> 8) & 0xFF),
                              (char) ((fourcc >> 16) & 0xFF),
                              (char) ((fourcc >> 24) & 0xFF),
                              0};

    printf("Frame #%d:\n", frame_num);
    printf("  Serial:    %ld\n", serial);
    printf("  Size:      %dx%d\n",
           vsl_frame_width(frame),
           vsl_frame_height(frame));
    printf("  Format:    %s (0x%08X)\n", fourcc_str, fourcc);
    printf("  Timestamp: %ld ns\n", timestamp);
    printf("  PTS:       %ld ns\n", pts);
    printf("  Duration:  %ld ns\n", duration);
    printf("  Checksum:  0x%08lX\n", checksum);
    printf("\n");
}

int
main(int argc, char* argv[])
{
    const char* socket_path      = DEFAULT_SOCKET_PATH;
    int         num_frames       = DEFAULT_NUM_FRAMES;
    VSLClient*  client           = NULL;
    int         ret              = 1;
    int         frame_count      = 0;
    int64_t     start_time       = 0;
    int64_t     first_frame_time = 0;

    // Parse command-line arguments
    if (argc > 1) { socket_path = argv[1]; }
    if (argc > 2) { num_frames = atoi(argv[2]); }

    printf("%s\n", SEPARATOR);
    printf("VideoStream Client Test - Frame Consumer\n");
    printf("%s\n", SEPARATOR);
    printf("Version:      %s\n", vsl_version());
    printf("Socket:       %s\n", socket_path);
    if (num_frames == 0) {
        printf("Target frames: infinite\n");
    } else {
        printf("Target frames: %d\n", num_frames);
    }
    printf("%s\n\n", SEPARATOR);

    // Set up signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Connect to host
    printf("Connecting to host at: %s\n", socket_path);
    client = vsl_client_init(socket_path, // socket path
                             NULL,        // userptr
                             true);       // auto-reconnect

    if (!client) {
        fprintf(stderr,
                "ERROR: Failed to connect to host: %s\n",
                strerror(errno));
        fprintf(stderr, "\n");
        fprintf(stderr, "Make sure test_host is running first:\n");
        fprintf(stderr, "  ./test_host %s\n", socket_path);
        fprintf(stderr, "\n");
        goto cleanup;
    }
    printf("✓ Connected to host\n");
    printf("  Path: %s\n\n", vsl_client_path(client));

    printf("%s\n", SEPARATOR);
    printf("Receiving frames...\n");
    printf("Press Ctrl+C to stop\n");
    printf("%s\n\n", SEPARATOR);

    start_time = vsl_timestamp();

    // Main loop: wait for frames
    while (g_running && (num_frames == 0 || frame_count < num_frames)) {
        VSLFrame* frame = vsl_frame_wait(client, 0);
        if (!frame) {
            if (errno == ETIMEDOUT) {
                fprintf(stderr, "WARNING: Timeout waiting for frame\n");
                continue;
            }
            fprintf(stderr,
                    "ERROR: Failed to receive frame: %s\n",
                    strerror(errno));
            break;
        }

        if (frame_count == 0) { first_frame_time = vsl_timestamp(); }
        frame_count++;

        // Lock frame for access
        if (vsl_frame_trylock(frame) != 0) {
            fprintf(stderr, "WARNING: Failed to lock frame %d\n", frame_count);
            vsl_frame_release(frame);
            continue;
        }

        // Map frame data
        size_t size = 0;
        void*  data = vsl_frame_mmap(frame, &size);
        if (!data) {
            fprintf(stderr, "WARNING: Failed to map frame %d\n", frame_count);
            vsl_frame_unlock(frame);
            vsl_frame_release(frame);
            continue;
        }

        // Calculate checksum for validation
        unsigned long checksum = calculate_checksum(data, size);

        // Print detailed stats for first frame and every 30th frame
        if (frame_count == 1 || frame_count % 30 == 0) {
            print_frame_stats(frame, frame_count, checksum);
        }

        vsl_frame_munmap(frame);
        vsl_frame_unlock(frame);
        vsl_frame_release(frame);
    }

    int64_t end_time         = vsl_timestamp();
    double  total_duration   = (end_time - start_time) / 1e9;
    double  receive_duration = (end_time - first_frame_time) / 1e9;

    printf("\n%s\n", SEPARATOR);
    printf("Statistics\n");
    printf("%s\n", SEPARATOR);
    printf("Frames received:  %d\n", frame_count);
    printf("Total time:       %.2f seconds\n", total_duration);
    printf("Receive time:     %.2f seconds\n", receive_duration);
    if (receive_duration > 0) {
        printf("Average FPS:      %.2f\n", frame_count / receive_duration);
    }
    printf("%s\n", SEPARATOR);

    ret = 0;

cleanup:
    if (client) { vsl_client_release(client); }

    return ret;
}
