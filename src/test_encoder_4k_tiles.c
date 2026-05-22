// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// TESTING LAYER: 3 (Hardware Integration)
// REQUIREMENTS:
//   - i.MX 8M Plus VPU encoder (/dev/video0)
//   - DMA heap (/dev/dma_heap/linux,cma)
// DESCRIPTION:
//   Tests VPU H.264 encoder with 4K resolution using tiled encoding.
//   Validates high-resolution encoding performance.

#include "videostream.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static uint32_t outputFourcc = VSL_FOURCC('H', '2', '6', '4');
static int      inWidth      = 3840;
static int      inHeight     = 2160;
static int      outWidth     = 1920;
static int      outHeight    = 1080;
static int      fps          = 30;

static int   run        = 1;
static char* hostPath   = NULL;
static int   max_frames = 0; // 0 = unbounded

void
sig_handler(int signum)
{
    (void) signum;
    run = 0;
}

VSLFrame*
get_test_frame(int width, int height)
{
    // RGB frame is easiest
    uint32_t fourcc = VSL_FOURCC('B', 'G', 'R', 'A');
    int      stride = width * 4;

    VSLFrame* frame = vsl_frame_init(width, height, stride, fourcc, NULL, NULL);

    if (!frame) {

        fprintf(stderr, "%s: vsl_frame_init failed\n", __FUNCTION__);
        return NULL;
    }

    // allocate memory for frame data
    if (-1 == vsl_frame_alloc(frame, NULL)) {
        fprintf(stderr,
                "%s: vsl_frame_alloc failed: %d\n",
                __FUNCTION__,
                errno);
        return NULL;
    }

    uint32_t* ptr = vsl_frame_mmap(frame, NULL);

    if (!ptr) {

        fprintf(stderr, "%s: vsl_frame_mmap failed\n", __FUNCTION__);
        return NULL;
    }

    // color bars
    uint32_t colorTable[8] = {0xffffffff,
                              0xfff9fb00,
                              0xff02feff,
                              0xff01ff00,
                              0xfffd00fb,
                              0xfffb0102,
                              0xff0301fc,
                              0xff000000};

    int barWidth = width / 8;

    // int radius of the circle
    static int radius = 250;
    // x pos of the circe
    static int x1 = 0;
    static int x2 = -1;
    if (x2 == -1) { x2 = width / 2; }
    // y pos of the circle
    int y = height / 2;
    // movement step
    static int step = 20;
    // circle color
    uint32_t color = 0xffffa500;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {

            // Calculate the distance between the pixel and the center of the
            // circle
            float distance1 = sqrt(pow(col - x1, 2) + pow(row - y, 2));
            float distance2 = sqrt(pow(col - x2, 2) + pow(row - y, 2));

            if (distance1 <= radius) {
                // paint circle
                ptr[row * width + col] = color;
            } else if (distance2 <= radius) {
                ptr[row * width + col] = color;
            } else {
                // paint bars
                ptr[row * width + col] = colorTable[col / barWidth];
            }
        }
    }

    x1 += step;
    x2 += step;
    if (x1 > width) { x1 = 0; }
    if (x2 > width) { x2 = 0; }

    return frame;
}

typedef struct {
    VSLRect*    cropRegion;
    VSLEncoder* encoder;
    VSLFrame*   source;
    int         fd;
} EncoderArgs;

void*
encodeAndSave(void* arg)
{
    EncoderArgs* args = arg;

    VSLFrame* encoded_frame =
        vsl_encoder_new_output_frame(args->encoder,
                                     args->cropRegion->width,
                                     args->cropRegion->height,
                                     vsl_frame_duration(args->source),
                                     vsl_frame_pts(args->source),
                                     vsl_frame_dts(args->source));
    if (!encoded_frame) {
        fprintf(stderr, "failed to obtain new encode frame\n");
        run = 0;
        pthread_exit(NULL);
    }

    int ret = vsl_encode_frame(args->encoder,
                               args->source,
                               encoded_frame,
                               args->cropRegion,
                               NULL);
    if (ret == -1) {
        fprintf(stderr, "failed to encode frame\n");
        run = 0;
        pthread_exit(NULL);
    }
    printf("encoded frame size: %d\n", vsl_frame_size(encoded_frame));
    ssize_t bytesWritten = write(args->fd,
                                 vsl_frame_mmap(encoded_frame, NULL),
                                 vsl_frame_size(encoded_frame));
    if (bytesWritten != vsl_frame_size(encoded_frame)) {
        printf("Write error\n");
    }

    vsl_frame_release(encoded_frame);
    pthread_exit(NULL);
}

static void
usage_and_exit(const char* prog, int status)
{
    fprintf(stderr,
            "Usage: %s [--host <socket>] [--frames N]\n"
            "  --host, -h <socket>   VSL host socket to read source frames "
            "from\n"
            "  --frames, -f N        Run for N frames then exit (N > 0)\n",
            prog);
    exit(status);
}

void
parseArguments(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--host") == 0 || strcmp(argv[i], "-h") == 0) &&
            i + 1 < argc && argv[i + 1][0] != '-') {
            hostPath = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--frames") == 0 ||
                   strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: missing value for %s\n", argv[0], argv[i]);
                usage_and_exit(argv[0], EXIT_FAILURE);
            }
            errno     = 0;
            char* end = NULL;
            long  val = strtol(argv[i + 1], &end, 10);
            if (errno != 0 || end == argv[i + 1] || *end != '\0' || val <= 0 ||
                val > INT_MAX) {
                fprintf(stderr,
                        "Invalid --frames value '%s': expected positive "
                        "integer\n",
                        argv[i + 1]);
                usage_and_exit(argv[0], EXIT_FAILURE);
            }
            max_frames = (int) val;
            i++;
        }
    }
}

void
checkHostPath()
{
    if (hostPath != NULL) {
        if (access(hostPath, F_OK) != 0) {
            printf("Host path does not exist: %s\n", hostPath);
            exit(-1);
        }
    } else {
        printf("No host path provided - using generated input frames\n");
    }
}

VSLFrame*
getInputFrame(VSLClient* client)
{
    VSLFrame* in_frame;

    if (hostPath) {
        if (!client) { exit(EXIT_FAILURE); }

        in_frame = vsl_frame_wait(client, 0);
        if (!in_frame) {
            fprintf(stderr,
                    "failed to acquire a in_frame: %s\n",
                    strerror(errno));
            exit(EXIT_FAILURE);
        }

        int      width  = vsl_frame_width(in_frame);
        int      height = vsl_frame_height(in_frame);
        uint32_t fourcc = vsl_frame_fourcc(in_frame);

        printf("acquired video frame %dx%d format:%c%c%c%c\n",
               width,
               height,
               fourcc,
               fourcc >> 8,
               fourcc >> 16,
               fourcc >> 24);

        if (vsl_frame_trylock(in_frame)) {
            // Lock race with the VSL host's buffer recycle is a known
            // pre-existing IPC issue; skip the frame rather than abort so
            // the test still produces a usable long-form capture.
            fprintf(stderr,
                    "skip frame %ld: trylock failed (%s)\n",
                    vsl_frame_serial(in_frame),
                    strerror(errno));
            vsl_frame_release(in_frame);
            return NULL;
        }

        // mmap frame here, later on mmap would be done four times in async
        // procedures causing error
        if (!vsl_frame_mmap(in_frame, NULL)) {
            fprintf(stderr,
                    "failed to mmap frame %ld: %s\n",
                    vsl_frame_serial(in_frame),
                    strerror(errno));
            vsl_frame_release(in_frame);
            exit(EXIT_FAILURE);
        }

        printf("locked frame %ld\n", vsl_frame_serial(in_frame));
    } else {
        in_frame = get_test_frame(inWidth, inHeight);
        if (!in_frame) {
            fprintf(stderr, "failed to obtain new input frame\n");
            exit(EXIT_FAILURE);
        }
    }

    return in_frame;
}

int
main(int argc, char* argv[])
{

    signal(SIGINT, sig_handler);

    parseArguments(argc, argv);

    checkHostPath();

    int tiles_fd[4];

    tiles_fd[0] = open("/tmp/vslencodedvideo_tile1.h264",
                       O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                       0644);

    tiles_fd[1] = open("/tmp/vslencodedvideo_tile2.h264",
                       O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                       0644);

    tiles_fd[2] = open("/tmp/vslencodedvideo_tile3.h264",
                       O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                       0644);

    tiles_fd[3] = open("/tmp/vslencodedvideo_tile4.h264",
                       O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                       0644);

    VSLEncoder* encoders[4];

    for (int i = 0; i < 4; i++) {
        encoders[i] =
            vsl_encoder_create(VSL_ENCODE_PROFILE_AUTO, outputFourcc, fps);
        if (!encoders[i]) {
            fprintf(stderr, "failed to create encoder instance.\n");
            return EXIT_FAILURE;
        }
    }

    // Set crop info for each frame
    VSLRect cropRegions[4] = {
        {.width = outWidth, .height = outHeight, .x = 0, .y = 0},
        {.width = outWidth, .height = outHeight, .x = outWidth, .y = 0},
        {.width = outWidth, .height = outHeight, .x = 0, .y = outHeight},
        {.width = outWidth, .height = outHeight, .x = outWidth, .y = outHeight},
    };

    VSLClient* client = NULL;

    if (hostPath) {
        client = vsl_client_init(hostPath, NULL, false);
        if (!client) {
            fprintf(stderr,
                    "failed to connect to videostream host %s: "
                    "%s\n",
                    hostPath,
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    int frame_count   = 0;
    int skipped_count = 0;
    while (run && (max_frames == 0 || frame_count < max_frames)) {
        VSLFrame* in_frame = getInputFrame(client);
        if (!in_frame) {
            // Transient skip (e.g. VSL lock race) — keep the loop alive, but
            // back off briefly so a sustained run of failures doesn't spin
            // the CPU at 100% and flood stderr.
            skipped_count++;
            usleep(2000); // 2 ms
            continue;
        }

        pthread_t   threads[4];
        EncoderArgs args[4];

        for (int i = 0; i < 4; i++) {
            args[i].encoder    = encoders[i];
            args[i].fd         = tiles_fd[i];
            args[i].cropRegion = &cropRegions[i];
            args[i].source     = in_frame;

            int result =
                pthread_create(&threads[i], NULL, encodeAndSave, &args[i]);
            if (result != 0) {
                printf("Error creating thread. Exiting.\n");
                return -1;
            }
        }

        for (int i = 0; i < 4; i++) { pthread_join(threads[i], NULL); }

        vsl_frame_release(in_frame);
        frame_count++;
        if (frame_count % 30 == 0) {
            fprintf(stderr, "tile encoder: %d frames\n", frame_count);
        }
    }
    fprintf(stderr,
            "tile encoder: completed %d frames (%d skipped on lock race)\n",
            frame_count,
            skipped_count);

    for (int i = 0; i < 4; i++) { close(tiles_fd[i]); }
}