// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * Comprehensive decoder benchmark test
 *
 * Tests sustained throughput with configurable warmup period.
 * Usage: benchmark_decoder [options]
 *   -b <backend>   Backend: auto, v4l2, hantro (default: auto)
 *   -w <seconds>   Warmup period in seconds (default: 2)
 *   -d <seconds>   Test duration in seconds (default: 30)
 *   -t <fps>       Target FPS (default: 30)
 *   -i <file>      Input H.264 file (default: /tmp/test.h264)
 *   -v             Verbose output
 *   -h             Show help
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "videostream.h"

// Configuration defaults
#define DEFAULT_WARMUP_SEC 2
#define DEFAULT_DURATION_SEC 30
#define DEFAULT_TARGET_FPS 30
#define DEFAULT_INPUT_FILE "/tmp/test.h264"

// Statistics tracking
typedef struct {
    uint64_t  frame_count;
    uint64_t  total_decode_us;
    uint64_t  min_decode_us;
    uint64_t  max_decode_us;
    uint64_t* frame_times_us; // Ring buffer for frame times
    size_t    frame_times_capacity;
    size_t    frame_times_head;
    uint64_t  dropped_frames; // Frames that exceeded target time
    uint64_t  start_time_us;
    uint64_t  end_time_us;
    uint64_t  loop_count; // How many times we looped through the file
} BenchmarkStats;

// Decoder state for streaming decode
typedef struct {
    VSLDecoder*    decoder;
    const uint8_t* data;
    size_t         data_len;
    size_t         offset;
    bool           verbose;
} DecoderState;

// Global for signal handler
static volatile bool g_running = true;

static void
signal_handler(int sig)
{
    (void) sig;
    g_running = false;
}

static uint64_t
get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000ULL + (uint64_t) ts.tv_nsec / 1000ULL;
}

static void
stats_init(BenchmarkStats* stats, size_t capacity)
{
    memset(stats, 0, sizeof(*stats));
    stats->min_decode_us        = UINT64_MAX;
    stats->frame_times_us       = malloc(capacity * sizeof(uint64_t));
    stats->frame_times_capacity = capacity;
    stats->frame_times_head     = 0;
}

static void
stats_free(BenchmarkStats* stats)
{
    free(stats->frame_times_us);
    stats->frame_times_us = NULL;
}

static void
stats_record_frame(BenchmarkStats* stats,
                   uint64_t        decode_us,
                   uint64_t        target_us)
{
    stats->frame_count++;
    stats->total_decode_us += decode_us;

    if (decode_us < stats->min_decode_us) { stats->min_decode_us = decode_us; }
    if (decode_us > stats->max_decode_us) { stats->max_decode_us = decode_us; }

    // Track dropped frames (exceeded target frame time)
    if (decode_us > target_us) { stats->dropped_frames++; }

    // Store in ring buffer for percentile calculations
    if (stats->frame_times_us && stats->frame_times_capacity > 0) {
        stats->frame_times_us[stats->frame_times_head] = decode_us;
        stats->frame_times_head =
            (stats->frame_times_head + 1) % stats->frame_times_capacity;
    }
}

static int
compare_uint64(const void* a, const void* b)
{
    uint64_t va = *(const uint64_t*) a;
    uint64_t vb = *(const uint64_t*) b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static void
stats_print(const BenchmarkStats* stats, int target_fps, bool verbose)
{
    if (stats->frame_count == 0) {
        printf("No frames decoded\n");
        return;
    }

    uint64_t duration_us  = stats->end_time_us - stats->start_time_us;
    double   duration_sec = (double) duration_us / 1000000.0;
    double   avg_fps      = (double) stats->frame_count / duration_sec;
    double   avg_decode_ms =
        (double) stats->total_decode_us / (double) stats->frame_count / 1000.0;
    double target_frame_ms = 1000.0 / (double) target_fps;

    printf("\n");
    printf("========================================\n");
    printf("       DECODER BENCHMARK RESULTS        \n");
    printf("========================================\n");
    printf("\n");
    printf("Duration:        %.2f seconds\n", duration_sec);
    printf("Frames decoded:  %lu\n", (unsigned long) stats->frame_count);
    printf("File loops:      %lu\n", (unsigned long) stats->loop_count);
    printf("Average FPS:     %.2f (target: %d)\n", avg_fps, target_fps);
    printf("\n");
    printf("Frame decode times:\n");
    printf("  Average:       %.3f ms\n", avg_decode_ms);
    printf("  Minimum:       %.3f ms\n",
           (double) stats->min_decode_us / 1000.0);
    printf("  Maximum:       %.3f ms\n",
           (double) stats->max_decode_us / 1000.0);
    printf("  Target:        %.3f ms (for %d FPS)\n",
           target_frame_ms,
           target_fps);
    printf("\n");

    // Calculate percentiles if we have frame time data
    if (stats->frame_times_us && stats->frame_count > 0) {
        size_t count = stats->frame_count < stats->frame_times_capacity
                           ? stats->frame_count
                           : stats->frame_times_capacity;

        // Copy and sort for percentile calculation
        uint64_t* sorted = malloc(count * sizeof(uint64_t));
        if (sorted) {
            memcpy(sorted, stats->frame_times_us, count * sizeof(uint64_t));
            qsort(sorted, count, sizeof(uint64_t), compare_uint64);

            size_t p50_idx = (size_t) (count * 0.50);
            size_t p90_idx = (size_t) (count * 0.90);
            size_t p95_idx = (size_t) (count * 0.95);
            size_t p99_idx = (size_t) (count * 0.99);

            printf("Percentiles:\n");
            printf("  P50:           %.3f ms\n",
                   (double) sorted[p50_idx] / 1000.0);
            printf("  P90:           %.3f ms\n",
                   (double) sorted[p90_idx] / 1000.0);
            printf("  P95:           %.3f ms\n",
                   (double) sorted[p95_idx] / 1000.0);
            printf("  P99:           %.3f ms\n",
                   (double) sorted[p99_idx] / 1000.0);
            printf("\n");

            free(sorted);
        }
    }

    double drop_pct =
        100.0 * (double) stats->dropped_frames / (double) stats->frame_count;
    printf("Dropped frames:  %lu (%.2f%% exceeded target frame time)\n",
           (unsigned long) stats->dropped_frames,
           drop_pct);
    printf("\n");

    // Pass/fail determination
    bool passed = (avg_fps >= (double) target_fps * 0.95) && (drop_pct < 5.0);
    printf("Result:          %s\n", passed ? "PASS" : "FAIL");
    printf("  - Average FPS >= 95%% of target: %s (%.1f%% achieved)\n",
           avg_fps >= target_fps * 0.95 ? "PASS" : "FAIL",
           100.0 * avg_fps / target_fps);
    printf("  - Dropped frames < 5%%: %s (%.1f%% dropped)\n",
           drop_pct < 5.0 ? "PASS" : "FAIL",
           drop_pct);
    printf("\n");

    if (verbose) {
        printf("Raw statistics:\n");
        printf("  start_time_us:     %lu\n",
               (unsigned long) stats->start_time_us);
        printf("  end_time_us:       %lu\n",
               (unsigned long) stats->end_time_us);
        printf("  total_decode_us:   %lu\n",
               (unsigned long) stats->total_decode_us);
    }
}

static void
print_usage(const char* prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  -b <backend>   Backend: auto, v4l2, hantro (default: auto)\n");
    printf("  -w <seconds>   Warmup period in seconds (default: %d)\n",
           DEFAULT_WARMUP_SEC);
    printf("  -d <seconds>   Test duration in seconds (default: %d)\n",
           DEFAULT_DURATION_SEC);
    printf("  -t <fps>       Target FPS (default: %d)\n", DEFAULT_TARGET_FPS);
    printf("  -i <file>      Input H.264 file (default: %s)\n",
           DEFAULT_INPUT_FILE);
    printf("  -v             Verbose output\n");
    printf("  -h             Show this help\n");
}

static VSLCodecBackend
parse_backend(const char* str)
{
    if (strcasecmp(str, "v4l2") == 0) {
        return VSL_CODEC_BACKEND_V4L2;
    } else if (strcasecmp(str, "hantro") == 0) {
        return VSL_CODEC_BACKEND_HANTRO;
    } else {
        return VSL_CODEC_BACKEND_AUTO;
    }
}

static const char*
backend_name(VSLCodecBackend backend)
{
    switch (backend) {
    case VSL_CODEC_BACKEND_V4L2:
        return "V4L2";
    case VSL_CODEC_BACKEND_HANTRO:
        return "Hantro";
    default:
        return "Auto";
    }
}

/**
 * Initialize decoder state for streaming decode
 */
static void
decoder_state_init(DecoderState*  state,
                   VSLDecoder*    decoder,
                   const uint8_t* data,
                   size_t         data_len,
                   bool           verbose)
{
    state->decoder  = decoder;
    state->data     = data;
    state->data_len = data_len;
    state->offset   = 0;
    state->verbose  = verbose;
}

/**
 * Reset decoder state to beginning of file (for looping)
 */
static void
decoder_state_reset(DecoderState* state)
{
    state->offset = 0;
}

/**
 * Decode the next frame from the stream
 *
 * Returns the decoded frame, or NULL if:
 *   - Error occurred
 *   - End of file reached (check state->offset >= state->data_len)
 */
static VSLFrame*
decode_next_frame(DecoderState* state)
{
    VSLFrame* frame = NULL;

    while (state->offset < state->data_len) {
        size_t bytes_used = 0;
        size_t remaining  = state->data_len - state->offset;

        VSLDecoderRetCode ret = vsl_decode_frame(state->decoder,
                                                 state->data + state->offset,
                                                 (unsigned int) remaining,
                                                 &bytes_used,
                                                 &frame);

        state->offset += bytes_used;

        if (ret == VSL_DEC_ERR) {
            if (state->verbose) {
                fprintf(stderr, "Decode error at offset %zu\n", state->offset);
            }
            return NULL;
        }

        if (frame != NULL) { return frame; }

        // No bytes consumed and no frame - avoid infinite loop
        if (bytes_used == 0) { break; }
    }

    return NULL;
}

int
main(int argc, char* argv[])
{
    // Configuration
    VSLCodecBackend backend      = VSL_CODEC_BACKEND_AUTO;
    int             warmup_sec   = DEFAULT_WARMUP_SEC;
    int             duration_sec = DEFAULT_DURATION_SEC;
    int             target_fps   = DEFAULT_TARGET_FPS;
    const char*     input_file   = DEFAULT_INPUT_FILE;
    bool            verbose      = false;

    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "b:w:d:t:i:vh")) != -1) {
        switch (opt) {
        case 'b':
            backend = parse_backend(optarg);
            break;
        case 'w':
            warmup_sec = atoi(optarg);
            if (warmup_sec < 0 || warmup_sec > 10) {
                fprintf(stderr, "Warmup must be 0-10 seconds\n");
                return 1;
            }
            break;
        case 'd':
            duration_sec = atoi(optarg);
            if (duration_sec < 1 || duration_sec > 300) {
                fprintf(stderr, "Duration must be 1-300 seconds\n");
                return 1;
            }
            break;
        case 't':
            target_fps = atoi(optarg);
            if (target_fps < 1 || target_fps > 120) {
                fprintf(stderr, "Target FPS must be 1-120\n");
                return 1;
            }
            break;
        case 'i':
            input_file = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set up signal handler for clean exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Load input file
    printf("Loading input file: %s\n", input_file);

    int fd = open(input_file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open input file: %s\n", strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "Failed to stat input file: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    size_t   file_size = (size_t) st.st_size;
    uint8_t* h264_data = malloc(file_size);
    if (!h264_data) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", file_size);
        close(fd);
        return 1;
    }

    ssize_t n = read(fd, h264_data, file_size);
    close(fd);
    if (n != (ssize_t) file_size) {
        fprintf(stderr, "Failed to read entire file\n");
        free(h264_data);
        return 1;
    }

    printf("Loaded %zu bytes of H.264 data\n", file_size);

    // Set backend via environment variable if specified
    if (backend != VSL_CODEC_BACKEND_AUTO) {
        const char* env_val =
            (backend == VSL_CODEC_BACKEND_V4L2) ? "v4l2" : "hantro";
        setenv("VSL_CODEC_BACKEND", env_val, 1);
        printf("Forcing backend: %s\n", env_val);
    }

    // Create decoder
    printf("Creating decoder (backend: %s)...\n", backend_name(backend));

    VSLDecoder* decoder = vsl_decoder_create(VSL_DEC_H264, target_fps);
    if (!decoder) {
        fprintf(stderr, "Failed to create decoder\n");
        free(h264_data);
        return 1;
    }

    printf("Decoder created successfully\n");

    // Initialize decoder state
    DecoderState dec_state;
    decoder_state_init(&dec_state, decoder, h264_data, file_size, verbose);

    // Initialize stats
    uint64_t target_frame_us = 1000000ULL / (uint64_t) target_fps;
    size_t   max_frames =
        (size_t) (duration_sec + warmup_sec + 5) * target_fps * 2;
    BenchmarkStats warmup_stats, test_stats;
    stats_init(&warmup_stats, (size_t) (warmup_sec * target_fps * 2));
    stats_init(&test_stats, max_frames);

    // Warmup phase
    if (warmup_sec > 0) {
        printf("\n--- WARMUP PHASE (%d seconds) ---\n", warmup_sec);
        uint64_t warmup_end_us     = get_time_us() + warmup_sec * 1000000ULL;
        warmup_stats.start_time_us = get_time_us();

        while (g_running && get_time_us() < warmup_end_us) {
            uint64_t frame_start = get_time_us();

            VSLFrame* frame = decode_next_frame(&dec_state);
            if (frame) {
                uint64_t frame_end = get_time_us();
                stats_record_frame(&warmup_stats,
                                   frame_end - frame_start,
                                   target_frame_us);
                vsl_frame_release(frame);

                if (verbose && warmup_stats.frame_count % 30 == 0) {
                    printf("  Warmup: %lu frames\n",
                           (unsigned long) warmup_stats.frame_count);
                }
            } else if (dec_state.offset >= dec_state.data_len) {
                // End of file - loop back
                decoder_state_reset(&dec_state);
                warmup_stats.loop_count++;
                if (verbose) {
                    printf("  Warmup: looping file (loop %lu)\n",
                           (unsigned long) warmup_stats.loop_count);
                }
            } else {
                // Decode failed
                if (verbose) { fprintf(stderr, "  Warmup decode failed\n"); }
            }
        }

        warmup_stats.end_time_us = get_time_us();
        printf("Warmup completed: %lu frames in %.2f seconds (%.1f FPS)\n",
               (unsigned long) warmup_stats.frame_count,
               (double) (warmup_stats.end_time_us -
                         warmup_stats.start_time_us) /
                   1000000.0,
               (double) warmup_stats.frame_count /
                   ((double) (warmup_stats.end_time_us -
                              warmup_stats.start_time_us) /
                    1000000.0));
    }

    // Reset for test phase
    decoder_state_reset(&dec_state);

    // Test phase - rate-limited to target FPS to simulate real camera input
    if (g_running) {
        printf("\n--- TEST PHASE (%d seconds, target %d FPS, rate-limited) "
               "---\n",
               duration_sec,
               target_fps);

        uint64_t test_end_us      = get_time_us() + duration_sec * 1000000ULL;
        test_stats.start_time_us  = get_time_us();
        uint64_t last_progress_us = test_stats.start_time_us;

        // Track frame pacing - next frame should start at this time
        uint64_t next_frame_us = test_stats.start_time_us;

        while (g_running && get_time_us() < test_end_us) {
            // Wait until next frame time (simulate camera frame rate)
            uint64_t now = get_time_us();
            if (now < next_frame_us) {
                uint64_t        sleep_us = next_frame_us - now;
                struct timespec ts = {.tv_sec = (time_t) (sleep_us / 1000000),
                                      .tv_nsec =
                                          (long) ((sleep_us % 1000000) * 1000)};
                nanosleep(&ts, NULL);
            }

            // Schedule next frame
            next_frame_us += target_frame_us;

            // If we've fallen behind, catch up (don't accumulate debt)
            now = get_time_us();
            if (now > next_frame_us + target_frame_us) { next_frame_us = now; }

            uint64_t frame_start = get_time_us();

            VSLFrame* frame = decode_next_frame(&dec_state);
            if (frame) {
                uint64_t frame_end = get_time_us();
                stats_record_frame(&test_stats,
                                   frame_end - frame_start,
                                   target_frame_us);
                vsl_frame_release(frame);
            } else if (dec_state.offset >= dec_state.data_len) {
                // End of file - loop back
                decoder_state_reset(&dec_state);
                test_stats.loop_count++;
                if (verbose) {
                    printf("  Test: looping file (loop %lu)\n",
                           (unsigned long) test_stats.loop_count);
                }
            } else {
                // Decode failed
                if (verbose) { fprintf(stderr, "  Test decode failed\n"); }
            }

            // Progress update every 5 seconds
            now = get_time_us();
            if (now - last_progress_us >= 5000000ULL) {
                double elapsed =
                    (double) (now - test_stats.start_time_us) / 1000000.0;
                double current_fps = (double) test_stats.frame_count / elapsed;
                printf("  Progress: %.0f sec, %lu frames, %.1f FPS, %lu "
                       "loops\n",
                       elapsed,
                       (unsigned long) test_stats.frame_count,
                       current_fps,
                       (unsigned long) test_stats.loop_count);
                last_progress_us = now;
            }
        }

        test_stats.end_time_us = get_time_us();
    }

    // Clean up decoder
    vsl_decoder_release(decoder);

    // Print results
    if (warmup_sec > 0 && verbose) {
        printf("\n--- WARMUP STATISTICS ---\n");
        stats_print(&warmup_stats, target_fps, verbose);
    }

    printf("\n--- TEST STATISTICS ---\n");
    stats_print(&test_stats, target_fps, verbose);

    // Cleanup
    stats_free(&warmup_stats);
    stats_free(&test_stats);
    free(h264_data);

    return 0;
}
