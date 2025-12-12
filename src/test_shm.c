// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// TESTING LAYER: 2 (Cross-Process IPC)
// REQUIREMENTS:
//   - Linux with POSIX shared memory (shm_open)
//   - No camera or VPU hardware required
// DESCRIPTION:
//   Tests POSIX shared memory fallback when DMA heap unavailable.
//   Validates frame allocation, sharing, and GStreamer integration.

/**
 * @file test_shm.c
 * @brief Testing basic functionalities of VSL with shared memory fallback
 * option
 *
 * Run following gstreamer pipeline to display the test pattern on wayland:
 * gst-launch-1.0 vslsrc path=/tmp/camhost.0 !
 * video/x-raw,width=800,height=600,format="BGRA" ! videoconvert ! waylandsink
 */

#include "videostream.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static int run = 1;

#define NANOS_PER_MILLI 1000000
#define DEFAULT_LIFESPAN 100

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

    // force creating shared memory
    const char* path = "/shm";

    // allocate memory for frame data
    if (-1 == vsl_frame_alloc(frame, path)) {
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
    static int radius = 100;
    // x pos of the circe
    static int x = 0;
    // y pos of the circle
    int y = height / 3;
    // movement step
    static int step = 5;
    // circle color
    uint32_t color = 0xffffa500;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {

            // Calculate the distance between the pixel and the center of the
            // circle
            float distance = sqrt(pow(col - x, 2) + pow(row - y, 2));

            if (distance <= radius) {
                // paint circle
                ptr[row * width + col] = color;
            } else {
                // paint bars
                ptr[row * width + col] = colorTable[col / barWidth];
            }
        }
    }

    x += step;
    if (x > width) { x = 0; }

    return frame;
}

int
main(void)
{
    signal(SIGINT, sig_handler);

    VSLHost* host = vsl_host_init("/tmp/camhost.0");
    if (!host) {
        fprintf(stderr,
                "failed to create videostream host: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    int width  = 800;
    int height = 600;

    while (run) {
        if (-1 == vsl_host_process(host)) {
            fprintf(stderr, "host process failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        VSLFrame* frame = get_test_frame(width, height);
        if (!frame) {
            fprintf(stderr,
                    "failed to create test frame: %s\n",
                    strerror(errno));
            return EXIT_FAILURE;
        }

        if (-1 == vsl_host_process(host)) {
            fprintf(stderr, "host process failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (-1 ==
            vsl_host_post(host,
                          frame,
                          vsl_timestamp() + DEFAULT_LIFESPAN * NANOS_PER_MILLI,
                          DEFAULT_LIFESPAN * NANOS_PER_MILLI,
                          0,
                          0)) {
            fprintf(stderr, "failed to post frame: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (-1 == vsl_host_process(host)) {
            fprintf(stderr, "host process failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        usleep(1000);
    }
    vsl_host_release(host);
}