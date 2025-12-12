// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// TESTING LAYER: 3 (Hardware Integration)
// REQUIREMENTS:
//   - i.MX 8M Plus VPU encoder (/dev/video0)
//   - DMA heap (/dev/dma_heap/linux,cma)
// DESCRIPTION:
//   Tests VPU H.264/HEVC encoder with synthetic frames.
//   Validates encoder creation, frame encoding, and bitstream output.

#include "videostream.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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
    static int radius = 150;
    // x pos of the circe
    static int x = 0;
    // y pos of the circle
    int y = height / 3;
    // movement step
    static int step = 10;
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

    int fd = open("/tmp/vslencodedvideo.hevc",
                  O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
                  0644);

    uint32_t    outputFourcc = VSL_FOURCC('H', 'E', 'V', 'C');
    VSLEncoder* enc =
        vsl_encoder_create(VSL_ENCODE_PROFILE_AUTO, outputFourcc, 30);
    if (!enc) {
        fprintf(stderr, "failed to create encoder instance.\n");
        return EXIT_FAILURE;
    }

    while (run) {
        VSLFrame* in_frame = get_test_frame(1920, 1080);
        if (!in_frame) {
            fprintf(stderr, "failed to obtain new input frame\n");
            return EXIT_FAILURE;
        }

        // create new frame for encoder output, allocates encoder EWL memory
        VSLFrame* encoded_frame =
            vsl_encoder_new_output_frame(enc,
                                         1920,
                                         1080,
                                         vsl_frame_duration(in_frame),
                                         vsl_frame_pts(in_frame),
                                         vsl_frame_dts(in_frame));

        if (!encoded_frame) {
            fprintf(stderr, "failed to obtain new encode frame\n");
            return EXIT_FAILURE;
        }

        int ret = vsl_encode_frame(enc, in_frame, encoded_frame, NULL, NULL);
        if (ret == -1) {
            fprintf(stderr, "failed to encode frame\n");
            return EXIT_FAILURE;
        }

        printf("encoded frame size: %d\n", vsl_frame_size(encoded_frame));

        ssize_t bytesWritten = write(fd,
                                     vsl_frame_mmap(encoded_frame, NULL),
                                     vsl_frame_size(encoded_frame));
        if (bytesWritten != vsl_frame_size(encoded_frame)) {
            printf("Write error\n");
        }

        vsl_frame_release(in_frame);
        vsl_frame_release(encoded_frame);
    }

    close(fd);
}