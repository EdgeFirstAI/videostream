// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * @file test_client.c
 * @brief VideoStream Client Test - Frame Consumer
 *
 * This test demonstrates the core VideoStream C API for inter-process frame
 * sharing. The client connects to a host and receives shared frames, measuring
 * latency and throughput.
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
#define SEPARATOR                                                              \
    "========================================================================" \
    "==="

static volatile bool g_running = true;

static void
signal_handler(int sig)
{
    (void) sig;
    g_running = false;
}

/**
 * Statistics tracking structure
 */
struct frame_stats {
    int64_t latency_min;
    int64_t latency_max;
    int64_t latency_sum;
    int64_t interval_min;
    int64_t interval_max;
    int64_t interval_sum;
    int64_t prev_timestamp;
    int64_t first_timestamp;
    int64_t last_timestamp;
    int     frame_count;
    int     dropped_frames;
    int64_t prev_serial;
};

static void
stats_init(struct frame_stats* stats)
{
    stats->latency_min     = INT64_MAX;
    stats->latency_max     = 0;
    stats->latency_sum     = 0;
    stats->interval_min    = INT64_MAX;
    stats->interval_max    = 0;
    stats->interval_sum    = 0;
    stats->prev_timestamp  = 0;
    stats->first_timestamp = 0;
    stats->last_timestamp  = 0;
    stats->frame_count     = 0;
    stats->dropped_frames  = 0;
    stats->prev_serial     = -1;
}

static void
stats_update(struct frame_stats* stats, VSLFrame* frame)
{
    int64_t receive_time    = vsl_timestamp();
    int64_t frame_timestamp = vsl_frame_timestamp(frame);
    int64_t serial          = vsl_frame_serial(frame);

    // Calculate latency (time from frame creation to receive)
    int64_t latency = receive_time - frame_timestamp;
    if (latency < stats->latency_min) { stats->latency_min = latency; }
    if (latency > stats->latency_max) { stats->latency_max = latency; }
    stats->latency_sum += latency;

    // Track timestamps for FPS calculation
    if (stats->frame_count == 0) {
        stats->first_timestamp = frame_timestamp;
    } else {
        // Calculate inter-frame interval
        int64_t interval = frame_timestamp - stats->prev_timestamp;
        if (interval < stats->interval_min) { stats->interval_min = interval; }
        if (interval > stats->interval_max) { stats->interval_max = interval; }
        stats->interval_sum += interval;
    }

    // Check for dropped frames (gaps in serial numbers)
    if (stats->prev_serial >= 0 && serial > stats->prev_serial + 1) {
        stats->dropped_frames += (int) (serial - stats->prev_serial - 1);
    }

    stats->prev_timestamp = frame_timestamp;
    stats->last_timestamp = frame_timestamp;
    stats->prev_serial    = serial;
    stats->frame_count++;
}

static void
stats_print(const struct frame_stats* stats)
{
    if (stats->frame_count == 0) {
        printf("No frames received\n");
        return;
    }

    // Latency statistics (in microseconds for readability)
    double latency_min_us = stats->latency_min / 1000.0;
    double latency_max_us = stats->latency_max / 1000.0;
    double latency_avg_us = (stats->latency_sum / stats->frame_count) / 1000.0;

    // FPS based on frame timestamps
    int64_t total_duration = stats->last_timestamp - stats->first_timestamp;
    double  fps            = 0.0;
    if (total_duration > 0 && stats->frame_count > 1) {
        fps = (stats->frame_count - 1) * 1e9 / total_duration;
    }

    // Inter-frame interval statistics (in milliseconds)
    double interval_min_ms = 0.0;
    double interval_max_ms = 0.0;
    double interval_avg_ms = 0.0;
    if (stats->frame_count > 1) {
        interval_min_ms = stats->interval_min / 1e6;
        interval_max_ms = stats->interval_max / 1e6;
        interval_avg_ms = (stats->interval_sum / (stats->frame_count - 1)) / 1e6;
    }

    printf("Frames received:  %d\n", stats->frame_count);
    if (stats->dropped_frames > 0) {
        printf("Frames dropped:   %d\n", stats->dropped_frames);
    }
    printf("\n");
    printf("Latency (us):     min=%.1f  max=%.1f  avg=%.1f\n",
           latency_min_us,
           latency_max_us,
           latency_avg_us);
    printf("Interval (ms):    min=%.2f  max=%.2f  avg=%.2f\n",
           interval_min_ms,
           interval_max_ms,
           interval_avg_ms);
    printf("Throughput:       %.2f FPS\n", fps);
}

/**
 * Print frame information (first frame only)
 */
static void
print_frame_info(VSLFrame* frame)
{
    uint32_t fourcc        = vsl_frame_fourcc(frame);
    char     fourcc_str[5] = {(char) ((fourcc >> 0) & 0xFF),
                              (char) ((fourcc >> 8) & 0xFF),
                              (char) ((fourcc >> 16) & 0xFF),
                              (char) ((fourcc >> 24) & 0xFF),
                              0};

    printf("Frame format:\n");
    printf("  Size:      %dx%d\n",
           vsl_frame_width(frame),
           vsl_frame_height(frame));
    printf("  Format:    %s (0x%08X)\n", fourcc_str, fourcc);
    printf("\n");
}

int
main(int argc, char* argv[])
{
    const char*        socket_path = DEFAULT_SOCKET_PATH;
    int                num_frames  = DEFAULT_NUM_FRAMES;
    VSLClient*         client      = NULL;
    int                ret         = 1;
    struct frame_stats stats;

    stats_init(&stats);

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
    printf("Connected to host\n");
    printf("  Path: %s\n\n", vsl_client_path(client));

    printf("%s\n", SEPARATOR);
    printf("Receiving frames...\n");
    printf("Press Ctrl+C to stop\n");
    printf("%s\n\n", SEPARATOR);

    // Main loop: wait for frames
    while (g_running && (num_frames == 0 || stats.frame_count < num_frames)) {
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

        // Print frame info on first frame
        if (stats.frame_count == 0) { print_frame_info(frame); }

        // Update statistics (latency, interval, dropped frames)
        stats_update(&stats, frame);

        // Progress indicator every 30 frames
        if (stats.frame_count % 30 == 0) {
            printf("Received %d frames...\n", stats.frame_count);
        }

        vsl_frame_release(frame);
    }

    printf("\n%s\n", SEPARATOR);
    printf("Statistics\n");
    printf("%s\n", SEPARATOR);
    stats_print(&stats);
    printf("%s\n", SEPARATOR);

    ret = 0;

cleanup:
    if (client) { vsl_client_release(client); }

    return ret;
}
