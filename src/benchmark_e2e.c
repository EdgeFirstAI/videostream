// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * End-to-End Encoder/Decoder Pipeline Benchmark
 *
 * Tests sustained throughput of the complete encode→decode pipeline.
 * Uses synthetic frames to isolate codec performance from camera I/O.
 *
 * Usage: benchmark_e2e [options]
 *   -b <backend>   Backend: auto, v4l2, hantro (default: auto)
 *   -c <codec>     Codec: h264, hevc (default: h264)
 *   -r <WxH>       Resolution (default: 1920x1080)
 *   -w <seconds>   Warmup period in seconds (default: 2)
 *   -d <seconds>   Test duration in seconds (default: 30)
 *   -t <fps>       Target FPS (default: 30)
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
#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

// Statistics tracking for a single stage
typedef struct {
    uint64_t  count;
    uint64_t  total_us;
    uint64_t  min_us;
    uint64_t  max_us;
    uint64_t* times_us;
    size_t    times_capacity;
    size_t    times_head;
} StageStats;

// End-to-end statistics
typedef struct {
    StageStats encode;
    StageStats decode;
    StageStats e2e; // Full encode+decode
    uint64_t   total_bytes_encoded;
    uint64_t   start_time_us;
    uint64_t   end_time_us;
    uint64_t   dropped_frames;
    uint64_t   target_frame_us;
} BenchmarkStats;

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
stage_stats_init(StageStats* stats, size_t capacity)
{
    memset(stats, 0, sizeof(*stats));
    stats->min_us         = UINT64_MAX;
    stats->times_us       = malloc(capacity * sizeof(uint64_t));
    stats->times_capacity = capacity;
}

static void
stage_stats_free(StageStats* stats)
{
    free(stats->times_us);
    stats->times_us = NULL;
}

static void
stage_stats_record(StageStats* stats, uint64_t time_us)
{
    stats->count++;
    stats->total_us += time_us;

    if (time_us < stats->min_us) { stats->min_us = time_us; }
    if (time_us > stats->max_us) { stats->max_us = time_us; }

    if (stats->times_us && stats->times_capacity > 0) {
        stats->times_us[stats->times_head] = time_us;
        stats->times_head = (stats->times_head + 1) % stats->times_capacity;
    }
}

static void
stats_init(BenchmarkStats* stats, size_t capacity, uint64_t target_frame_us)
{
    memset(stats, 0, sizeof(*stats));
    stage_stats_init(&stats->encode, capacity);
    stage_stats_init(&stats->decode, capacity);
    stage_stats_init(&stats->e2e, capacity);
    stats->target_frame_us = target_frame_us;
}

static void
stats_free(BenchmarkStats* stats)
{
    stage_stats_free(&stats->encode);
    stage_stats_free(&stats->decode);
    stage_stats_free(&stats->e2e);
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
print_stage_percentiles(const char* name, const StageStats* stats)
{
    if (stats->count == 0) return;

    size_t count = stats->count < stats->times_capacity ? stats->count
                                                        : stats->times_capacity;

    uint64_t* sorted = malloc(count * sizeof(uint64_t));
    if (!sorted) return;

    memcpy(sorted, stats->times_us, count * sizeof(uint64_t));
    qsort(sorted, count, sizeof(uint64_t), compare_uint64);

    size_t p50_idx = (size_t) (count * 0.50);
    size_t p90_idx = (size_t) (count * 0.90);
    size_t p95_idx = (size_t) (count * 0.95);
    size_t p99_idx = (size_t) (count * 0.99);

    printf("  %s Percentiles:\n", name);
    printf("    P50: %.3f ms\n", (double) sorted[p50_idx] / 1000.0);
    printf("    P90: %.3f ms\n", (double) sorted[p90_idx] / 1000.0);
    printf("    P95: %.3f ms\n", (double) sorted[p95_idx] / 1000.0);
    printf("    P99: %.3f ms\n", (double) sorted[p99_idx] / 1000.0);

    free(sorted);
}

static void
stats_print(const BenchmarkStats* stats, int target_fps, bool verbose)
{
    if (stats->e2e.count == 0) {
        printf("No frames processed\n");
        return;
    }

    uint64_t duration_us  = stats->end_time_us - stats->start_time_us;
    double   duration_sec = (double) duration_us / 1000000.0;
    double   avg_fps      = (double) stats->e2e.count / duration_sec;
    double   target_ms    = 1000.0 / (double) target_fps;

    printf("\n");
    printf("========================================\n");
    printf("     E2E PIPELINE BENCHMARK RESULTS     \n");
    printf("========================================\n");
    printf("\n");
    printf("Duration:          %.2f seconds\n", duration_sec);
    printf("Frames processed:  %lu\n", (unsigned long) stats->e2e.count);
    printf("Average FPS:       %.2f (target: %d)\n", avg_fps, target_fps);
    printf("Total encoded:     %.2f MB\n",
           (double) stats->total_bytes_encoded / (1024.0 * 1024.0));
    printf("Avg bitrate:       %.2f Mbps\n",
           (double) stats->total_bytes_encoded * 8.0 / duration_sec /
               1000000.0);
    printf("\n");

    printf("Encode stage:\n");
    printf("  Average:         %.3f ms\n",
           (double) stats->encode.total_us / (double) stats->encode.count /
               1000.0);
    printf("  Minimum:         %.3f ms\n",
           (double) stats->encode.min_us / 1000.0);
    printf("  Maximum:         %.3f ms\n",
           (double) stats->encode.max_us / 1000.0);
    printf("\n");

    printf("Decode stage:\n");
    printf("  Average:         %.3f ms\n",
           (double) stats->decode.total_us / (double) stats->decode.count /
               1000.0);
    printf("  Minimum:         %.3f ms\n",
           (double) stats->decode.min_us / 1000.0);
    printf("  Maximum:         %.3f ms\n",
           (double) stats->decode.max_us / 1000.0);
    printf("\n");

    printf("End-to-End (encode+decode):\n");
    printf("  Average:         %.3f ms\n",
           (double) stats->e2e.total_us / (double) stats->e2e.count / 1000.0);
    printf("  Minimum:         %.3f ms\n", (double) stats->e2e.min_us / 1000.0);
    printf("  Maximum:         %.3f ms\n", (double) stats->e2e.max_us / 1000.0);
    printf("  Target:          %.3f ms (for %d FPS)\n", target_ms, target_fps);
    printf("\n");

    if (stats->e2e.times_us) {
        print_stage_percentiles("Encode", &stats->encode);
        print_stage_percentiles("Decode", &stats->decode);
        print_stage_percentiles("E2E", &stats->e2e);
        printf("\n");
    }

    double drop_pct =
        100.0 * (double) stats->dropped_frames / (double) stats->e2e.count;
    printf("Dropped frames:    %lu (%.2f%% exceeded target frame time)\n",
           (unsigned long) stats->dropped_frames,
           drop_pct);
    printf("\n");

    // Pass/fail determination
    bool passed = (avg_fps >= (double) target_fps * 0.95) && (drop_pct < 5.0);
    printf("Result:            %s\n", passed ? "PASS" : "FAIL");
    printf("  - Average FPS >= 95%% of target: %s (%.1f%% achieved)\n",
           avg_fps >= target_fps * 0.95 ? "PASS" : "FAIL",
           100.0 * avg_fps / target_fps);
    printf("  - Dropped frames < 5%%: %s (%.1f%% dropped)\n",
           drop_pct < 5.0 ? "PASS" : "FAIL",
           drop_pct);
    printf("\n");

    if (verbose) {
        printf("Raw statistics:\n");
        printf("  start_time_us:       %lu\n",
               (unsigned long) stats->start_time_us);
        printf("  end_time_us:         %lu\n",
               (unsigned long) stats->end_time_us);
        printf("  encode_total_us:     %lu\n",
               (unsigned long) stats->encode.total_us);
        printf("  decode_total_us:     %lu\n",
               (unsigned long) stats->decode.total_us);
        printf("  e2e_total_us:        %lu\n",
               (unsigned long) stats->e2e.total_us);
    }
}

static void
print_usage(const char* prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  -b <backend>   Backend: auto, v4l2, hantro (default: auto)\n");
    printf("  -c <codec>     Codec: h264, hevc (default: h264)\n");
    printf("  -r <WxH>       Resolution (default: %dx%d)\n",
           DEFAULT_WIDTH,
           DEFAULT_HEIGHT);
    printf("  -w <seconds>   Warmup period in seconds (default: %d)\n",
           DEFAULT_WARMUP_SEC);
    printf("  -d <seconds>   Test duration in seconds (default: %d)\n",
           DEFAULT_DURATION_SEC);
    printf("  -t <fps>       Target FPS (default: %d)\n", DEFAULT_TARGET_FPS);
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
 * Create a synthetic BGRA test frame with moving color bars
 */
static VSLFrame*
create_test_frame(int width, int height, int frame_num)
{
    uint32_t  fourcc = VSL_FOURCC('B', 'G', 'R', 'A');
    int       stride = width * 4;
    VSLFrame* frame = vsl_frame_init(width, height, stride, fourcc, NULL, NULL);

    if (!frame) {
        fprintf(stderr, "vsl_frame_init failed\n");
        return NULL;
    }

    if (vsl_frame_alloc(frame, NULL) < 0) {
        fprintf(stderr, "vsl_frame_alloc failed: %s\n", strerror(errno));
        vsl_frame_release(frame);
        return NULL;
    }

    uint32_t* ptr = vsl_frame_mmap(frame, NULL);
    if (!ptr) {
        fprintf(stderr, "vsl_frame_mmap failed\n");
        vsl_frame_release(frame);
        return NULL;
    }

    // Color bars with moving offset for temporal variation
    uint32_t colorTable[8] = {0xffffffff,
                              0xfff9fb00,
                              0xff02feff,
                              0xff01ff00,
                              0xfffd00fb,
                              0xfffb0102,
                              0xff0301fc,
                              0xff000000};

    int barWidth = width / 8;
    int offset   = (frame_num * 10) % width; // Move bars horizontally

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int colorIdx           = ((col + offset) / barWidth) % 8;
            ptr[row * width + col] = colorTable[colorIdx];
        }
    }

    return frame;
}

int
main(int argc, char* argv[])
{
    // Configuration
    VSLCodecBackend backend       = VSL_CODEC_BACKEND_AUTO;
    int             warmup_sec    = DEFAULT_WARMUP_SEC;
    int             duration_sec  = DEFAULT_DURATION_SEC;
    int             target_fps    = DEFAULT_TARGET_FPS;
    int             width         = DEFAULT_WIDTH;
    int             height        = DEFAULT_HEIGHT;
    bool            verbose       = false;
    uint32_t        codec_fourcc  = VSL_FOURCC('H', '2', '6', '4');
    VSLDecoderCodec decoder_codec = VSL_DEC_H264;

    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "b:c:r:w:d:t:vh")) != -1) {
        switch (opt) {
        case 'b':
            backend = parse_backend(optarg);
            break;
        case 'c':
            if (strcasecmp(optarg, "hevc") == 0 ||
                strcasecmp(optarg, "h265") == 0) {
                codec_fourcc  = VSL_FOURCC('H', 'E', 'V', 'C');
                decoder_codec = VSL_DEC_HEVC;
            } else {
                codec_fourcc  = VSL_FOURCC('H', '2', '6', '4');
                decoder_codec = VSL_DEC_H264;
            }
            break;
        case 'r':
            if (sscanf(optarg, "%dx%d", &width, &height) != 2) {
                fprintf(stderr, "Invalid resolution format. Use WxH\n");
                return 1;
            }
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

    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("E2E Pipeline Benchmark\n");
    printf("  Resolution:  %dx%d\n", width, height);
    printf("  Codec:       %s\n",
           decoder_codec == VSL_DEC_HEVC ? "HEVC" : "H.264");
    printf("  Backend:     %s\n", backend_name(backend));
    printf("  Target FPS:  %d\n", target_fps);
    printf("  Warmup:      %d sec\n", warmup_sec);
    printf("  Duration:    %d sec\n", duration_sec);
    printf("\n");

    // Create encoder
    printf("Creating encoder...\n");
    VSLEncoder* encoder = vsl_encoder_create(VSL_ENCODE_PROFILE_25000_KBPS,
                                             codec_fourcc,
                                             target_fps);
    if (!encoder) {
        fprintf(stderr, "Failed to create encoder\n");
        return 1;
    }
    printf("Encoder created successfully\n");

    // Create decoder
    printf("Creating decoder...\n");
    VSLDecoder* decoder = vsl_decoder_create(decoder_codec, target_fps);
    if (!decoder) {
        fprintf(stderr, "Failed to create decoder\n");
        vsl_encoder_release(encoder);
        return 1;
    }
    printf("Decoder created successfully\n");

    // Initialize stats
    uint64_t target_frame_us = 1000000ULL / (uint64_t) target_fps;
    size_t   max_frames =
        (size_t) (duration_sec + warmup_sec + 5) * target_fps * 2;
    BenchmarkStats warmup_stats, test_stats;
    stats_init(&warmup_stats,
               (size_t) (warmup_sec * target_fps * 2),
               target_frame_us);
    stats_init(&test_stats, max_frames, target_frame_us);

    int frame_num = 0;

    // Warmup phase
    if (warmup_sec > 0 && g_running) {
        printf("\n--- WARMUP PHASE (%d seconds) ---\n", warmup_sec);
        uint64_t warmup_end_us     = get_time_us() + warmup_sec * 1000000ULL;
        warmup_stats.start_time_us = get_time_us();

        while (g_running && get_time_us() < warmup_end_us) {
            uint64_t e2e_start = get_time_us();

            // Create synthetic input frame
            VSLFrame* input = create_test_frame(width, height, frame_num++);
            if (!input) continue;

            // Create output frame for encoded data
            VSLFrame* encoded = vsl_encoder_new_output_frame(encoder,
                                                             width,
                                                             height,
                                                             0,
                                                             e2e_start,
                                                             e2e_start);
            if (!encoded) {
                vsl_frame_release(input);
                continue;
            }

            // Encode
            uint64_t encode_start = get_time_us();
            int enc_ret = vsl_encode_frame(encoder, input, encoded, NULL, NULL);
            uint64_t encode_end = get_time_us();

            vsl_frame_release(input);

            if (enc_ret < 0) {
                vsl_frame_release(encoded);
                continue;
            }

            stage_stats_record(&warmup_stats.encode, encode_end - encode_start);
            warmup_stats.total_bytes_encoded += vsl_frame_size(encoded);

            // Decode the encoded data
            VSLFrame* decoded    = NULL;
            size_t    bytes_used = 0;
            void*     enc_data   = vsl_frame_mmap(encoded, NULL);
            int       enc_size   = vsl_frame_size(encoded);

            uint64_t decode_start = get_time_us();
            vsl_decode_frame(decoder,
                             enc_data,
                             enc_size,
                             &bytes_used,
                             &decoded);
            uint64_t decode_end = get_time_us();

            vsl_frame_release(encoded);

            if (decoded) {
                stage_stats_record(&warmup_stats.decode,
                                   decode_end - decode_start);
                uint64_t e2e_end = get_time_us();
                stage_stats_record(&warmup_stats.e2e, e2e_end - e2e_start);

                if (e2e_end - e2e_start > target_frame_us) {
                    warmup_stats.dropped_frames++;
                }

                vsl_frame_release(decoded);

                if (verbose && warmup_stats.e2e.count % 30 == 0) {
                    printf("  Warmup: %lu frames\n",
                           (unsigned long) warmup_stats.e2e.count);
                }
            }
        }

        warmup_stats.end_time_us = get_time_us();
        printf("Warmup completed: %lu frames in %.2f seconds (%.1f FPS)\n",
               (unsigned long) warmup_stats.e2e.count,
               (double) (warmup_stats.end_time_us -
                         warmup_stats.start_time_us) /
                   1000000.0,
               (double) warmup_stats.e2e.count /
                   ((double) (warmup_stats.end_time_us -
                              warmup_stats.start_time_us) /
                    1000000.0));
    }

    // Test phase
    if (g_running) {
        printf("\n--- TEST PHASE (%d seconds, target %d FPS) ---\n",
               duration_sec,
               target_fps);

        uint64_t test_end_us      = get_time_us() + duration_sec * 1000000ULL;
        test_stats.start_time_us  = get_time_us();
        uint64_t last_progress_us = test_stats.start_time_us;
        uint64_t next_frame_us    = test_stats.start_time_us;

        while (g_running && get_time_us() < test_end_us) {
            // Rate limiting
            uint64_t now = get_time_us();
            if (now < next_frame_us) {
                uint64_t        sleep_us = next_frame_us - now;
                struct timespec ts = {.tv_sec = (time_t) (sleep_us / 1000000),
                                      .tv_nsec =
                                          (long) ((sleep_us % 1000000) * 1000)};
                nanosleep(&ts, NULL);
            }
            next_frame_us += target_frame_us;

            // Catch up if behind
            now = get_time_us();
            if (now > next_frame_us + target_frame_us) { next_frame_us = now; }

            uint64_t e2e_start = get_time_us();

            // Create synthetic input frame
            VSLFrame* input = create_test_frame(width, height, frame_num++);
            if (!input) continue;

            // Create output frame for encoded data
            VSLFrame* encoded = vsl_encoder_new_output_frame(encoder,
                                                             width,
                                                             height,
                                                             0,
                                                             e2e_start,
                                                             e2e_start);
            if (!encoded) {
                vsl_frame_release(input);
                continue;
            }

            // Encode
            uint64_t encode_start = get_time_us();
            int enc_ret = vsl_encode_frame(encoder, input, encoded, NULL, NULL);
            uint64_t encode_end = get_time_us();

            vsl_frame_release(input);

            if (enc_ret < 0) {
                vsl_frame_release(encoded);
                continue;
            }

            stage_stats_record(&test_stats.encode, encode_end - encode_start);
            test_stats.total_bytes_encoded += vsl_frame_size(encoded);

            // Decode the encoded data
            VSLFrame* decoded    = NULL;
            size_t    bytes_used = 0;
            void*     enc_data   = vsl_frame_mmap(encoded, NULL);
            int       enc_size   = vsl_frame_size(encoded);

            uint64_t decode_start = get_time_us();
            vsl_decode_frame(decoder,
                             enc_data,
                             enc_size,
                             &bytes_used,
                             &decoded);
            uint64_t decode_end = get_time_us();

            vsl_frame_release(encoded);

            if (decoded) {
                stage_stats_record(&test_stats.decode,
                                   decode_end - decode_start);
                uint64_t e2e_end = get_time_us();
                stage_stats_record(&test_stats.e2e, e2e_end - e2e_start);

                if (e2e_end - e2e_start > target_frame_us) {
                    test_stats.dropped_frames++;
                }

                vsl_frame_release(decoded);
            }

            // Progress update every 5 seconds
            now = get_time_us();
            if (now - last_progress_us >= 5000000ULL) {
                double elapsed =
                    (double) (now - test_stats.start_time_us) / 1000000.0;
                double current_fps = (double) test_stats.e2e.count / elapsed;
                printf("  Progress: %.0f sec, %lu frames, %.1f FPS\n",
                       elapsed,
                       (unsigned long) test_stats.e2e.count,
                       current_fps);
                last_progress_us = now;
            }
        }

        test_stats.end_time_us = get_time_us();
    }

    // Cleanup
    vsl_decoder_release(decoder);
    vsl_encoder_release(encoder);

    // Print results
    if (warmup_sec > 0 && verbose) {
        printf("\n--- WARMUP STATISTICS ---\n");
        stats_print(&warmup_stats, target_fps, verbose);
    }

    printf("\n--- TEST STATISTICS ---\n");
    stats_print(&test_stats, target_fps, verbose);

    stats_free(&warmup_stats);
    stats_free(&test_stats);

    return 0;
}
