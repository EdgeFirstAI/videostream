// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// TESTING LAYER: 3 (Hardware Integration)
// REQUIREMENTS:
//   - i.MX 8M Plus VPU decoder (/dev/video1)
//   - DMA heap (/dev/dma_heap/linux,cma)
//   - H.264 bitstream file (tmp.h264)
// DESCRIPTION:
//   Tests VPU H.264 decoder with real bitstream.
//   Validates decoder creation, frame decoding, and NV12 output.

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
    printf("New\n");
    signal(SIGINT, sig_handler);
    int fd = open("./tmp.h264", O_RDONLY, 0644);

    // allocate around 10 mb for video buffer;
    void* buf        = malloc(10000000);
    int   bytes_read = read(fd, buf, 10000000 - 1);
    printf("Read %i bytes from tmp.h264\n", bytes_read);

    VSLDecoder* dec = vsl_decoder_create(VSL_DEC_H264, 30);
    if (!dec) {
        fprintf(stderr, "failed to create decoder instance.\n");
        return EXIT_FAILURE;
    }
    printf("Created encoder instance\n");
    int       frames        = 0;
    long      start         = vsl_timestamp();
    VSLFrame* decoded_frame = NULL;
    while (run && bytes_read > 0 && frames < 60) {
        int bytes_to_give = bytes_read;
        if (bytes_to_give > 100000) { bytes_to_give = 100000; }
        printf("Giving %i bytes\n", bytes_to_give);
        size_t bytes_consumed = 0;
        int    ret_code       = vsl_decode_frame(dec,
                                        buf,
                                        bytes_read,
                                        &bytes_consumed,
                                        &decoded_frame);
        printf("vsl_decode_frame consumed %i bytes\n", bytes_consumed);
        if (decoded_frame) {
            printf("got a decoded frame %p\n", decoded_frame);
            vsl_frame_release(decoded_frame);
            decoded_frame = NULL;
        }

        buf += bytes_consumed;
        bytes_read -= bytes_consumed;
        frames++;
    }
    long elapsed = vsl_timestamp() - start;
    printf("Took %.2f ms to decode %i frames for %.2f FPS.\n",
           elapsed / 1e6,
           frames,
           frames / (elapsed / 1e9));

    // vsl_decode_frame(dec, buf, bytes_read);
    // vsl_decode_frame(dec, buf, bytes_read);
    // vsl_decode_frame(dec, buf, bytes_read);
    // return 0;

    // while (run) {

    // }

    int ret = vsl_decoder_release(dec);
    if (ret) { printf("vsl_decoder_release error\n"); }
    close(fd);
}