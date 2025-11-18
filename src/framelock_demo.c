// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stb_image_write.h"
#include "videostream.h"

#define LIBG2D_IMPLEMENTATION
#include "libg2d.h"

static enum g2d_format
g2d_from_fourcc(uint32_t fourcc)
{
    switch (fourcc) {
    case VSL_FOURCC('N', 'V', '1', '2'):
        return G2D_NV12;
    case VSL_FOURCC('I', '4', '2', '0'):
        return G2D_I420;
    case VSL_FOURCC('Y', 'V', '1', '2'):
        return G2D_YV12;
    case VSL_FOURCC('N', 'V', '2', '1'):
        return G2D_NV21;
    case VSL_FOURCC('Y', 'U', 'Y', 'V'):
        return G2D_YUYV;
    case VSL_FOURCC('Y', 'U', 'Y', '2'):
        return G2D_YUYV;
    case VSL_FOURCC('Y', 'V', 'Y', 'U'):
        return G2D_YUYV;
    case VSL_FOURCC('U', 'Y', 'V', 'Y'):
        return G2D_UYVY;
    case VSL_FOURCC('V', 'Y', 'U', 'Y'):
        return G2D_VYUY;
    case VSL_FOURCC('N', 'V', '1', '6'):
        return G2D_NV16;
    case VSL_FOURCC('N', 'V', '6', '1'):
        return G2D_NV61;
    }

    fprintf(stderr,
            "unsupported frame format %c%c%c%c\n",
            fourcc,
            fourcc >> 8,
            fourcc >> 16,
            fourcc >> 24);
    return 0;
}

const char* usage = "\
\n\
USAGE: \n\
    %s\n\
        -h, --help\n\
            Display help information\n\
        -c, --camera\n\
            The path of the camera host (default /tmp/camera.vsl)\n\
        -o, --out\n\
            The file to save the image to (default ./frame.jpg)\n\
\n\
";

int
main(int argc, char** argv)
{
    struct g2d* g2d        = NULL;
    void*       g2d_handle = NULL;
    int64_t     last_frame = 0;
    VSLClient*  client;

    // printf("Sleeping 3s\n");
    // struct timespec delay = {3, 0};
    // nanosleep(&delay, NULL);
    char* vsl_path = "/tmp/camera.vsl";
    char* file_path = "./frame.jpg";
    static struct option options[] =
        {{"help", no_argument, NULL, 'h'},
         {"camera", required_argument, NULL, 'c'},
         {"out", required_argument, NULL, 'o'}};

    for (int i = 0; i < 100; i++) {
        int opt = getopt_long(argc, argv, "hc:o:", options, NULL);
        if (opt == -1) break;
        switch (opt) {
        case 'h': {
            printf(usage, argv[0]);
            return EXIT_SUCCESS;
        }
        case 'c': {
            vsl_path = optarg;
            break;
        }
        case 'o': {
            file_path = optarg;
            break;
        }
        }
    }

    g2d = g2d_initialize(NULL, NULL);
    if (!g2d) {
        printf(
            "[WARNING] unable to intialize g2d, only RGB will be supported.\n");
    } else if (g2d->open(&g2d_handle)) {
        fprintf(stderr, "failed to open g2d library\n");
        return EXIT_FAILURE;
    }

    client = vsl_client_init(vsl_path, NULL, false);

    if (!client) {
        fprintf(stderr,
                "failed to connect to videostream host %s: %s\n",
                vsl_path,
                strerror(errno));
        return EXIT_FAILURE;
    }

    printf("connected to %s\n", vsl_path);
    for (int i = 0; i < 1; i++) {
        VSLFrame* frame = vsl_frame_wait(client, 0);
        if (!frame) {
            fprintf(stderr, "failed to acquire a frame: %s\n", strerror(errno));
            vsl_client_release(client);
            return EXIT_FAILURE;
        }

        int      width  = vsl_frame_width(frame);
        int      height = vsl_frame_height(frame);
        uint32_t fourcc = vsl_frame_fourcc(frame);
        intptr_t paddr  = vsl_frame_paddr(frame);

        printf("acquired video frame %dx%d format:%c%c%c%c DMA_fd:%d paddr:%lx "
               "frame_time: %ld\n",
               width,
               height,
               fourcc,
               fourcc >> 8,
               fourcc >> 16,
               fourcc >> 24,
               vsl_frame_handle(frame),
               paddr,
               vsl_frame_timestamp(frame) - last_frame);

        if (vsl_frame_trylock(frame)) {
            fprintf(stderr, "failed to lock frame: %s\n", strerror(errno));
            vsl_client_release(client);
            return EXIT_FAILURE;
        }

        printf("Locked frame serial:%ld DMA_fd:%d\n", vsl_frame_serial(frame), vsl_frame_handle(frame));

        printf("Getting 150 extra frames before saving locked frame\n");
        printf("\tacquired video frame serial: ");
        fflush(stdout);
        char cbuf[32];
        int n = 0;
        for (int j = 0; j < 30*5; j++) {
            VSLFrame* frame1 = vsl_frame_wait(client, 0);
            int serial  = vsl_frame_serial(frame1);
            for (int k = 0; k < n; k++) {
                printf("\b");
            }
            n = snprintf(cbuf, 32, "%d", serial);
            printf("%s", cbuf);
            fflush(stdout);
        }
        printf("\n");
        printf("Saving original locked frame serial: %ld\n", vsl_frame_serial(frame));
        if (fourcc == 0x33424752) {
            size_t         size;
            const uint8_t* buffer = vsl_frame_mmap(frame, &size);

            if (!buffer) {
                fprintf(stderr, "failed to mmap frame: %s\n", strerror(errno));
                vsl_frame_unlock(frame);
                vsl_client_release(client);
                return EXIT_FAILURE;
            }

            stbi_write_jpg(file_path, width, height, 3, buffer, 90);
            printf("saved frame to %s\n", file_path);
        } else if (g2d) {
            struct g2d_buf *dstbuf, *srcbuf;

            if (g2d->buf_from_fd) {
                srcbuf = g2d->buf_from_fd(vsl_frame_handle(frame));
                if (!srcbuf) {
                    printf("[ERROR] failed to query dmabuf for physical "
                           "address\n");
                    return EXIT_FAILURE;
                }

#ifndef NDEBUG
                printf("%s g2d size:%d vaddr:%p paddr:%x\n",
                       __FUNCTION__,
                       srcbuf->buf_size,
                       srcbuf->buf_vaddr,
                       srcbuf->buf_paddr
                       );
#endif
            } else {
                printf("[ERROR] g2d_buf_from_fd is required.\n");
                return EXIT_FAILURE;
            }

            dstbuf = g2d->alloc(width * height * 4, 1);
            if (!dstbuf) {
                printf("[ERROR] failed to allocate destination g2d buffer\n");
                return EXIT_FAILURE;
            }

            struct g2d_surface src, dst;
            memset(&src, 0, sizeof(src));
            memset(&dst, 0, sizeof(dst));

            src.planes[0] = srcbuf->buf_paddr;
            src.left      = 0;
            src.top       = 0;
            src.right     = width;
            src.bottom    = height;
            src.stride    = width;
            src.width     = width;
            src.height    = height;
            src.format    = g2d_from_fourcc(fourcc);

            dst.planes[0] = dstbuf->buf_paddr;
            dst.left      = 0;
            dst.top       = 0;
            dst.right     = width;
            dst.bottom    = height;
            dst.stride    = width;
            dst.width     = width;
            dst.height    = height;
            dst.format    = G2D_RGBX8888;

            if (g2d->blit(g2d_handle, &src, &dst)) {
                fprintf(stderr, "failed to blit video frame into tensor\n");
                return EXIT_FAILURE;
            }

            if (g2d->finish(g2d_handle)) {
                fprintf(stderr, "failed to finish video frame conversion\n");
                return EXIT_FAILURE;
            }

            g2d->cache_op(dstbuf, G2D_CACHE_INVALIDATE);
            uint8_t* buf = (uint8_t*) dstbuf->buf_vaddr;

            stbi_write_jpg(file_path, width, height, 4, buf, 90);
            printf("saved frame to %s\n", file_path);

            g2d->free(dstbuf);
            g2d->free(srcbuf);
        } else {
            fprintf(stderr, "Only RGB frames are supported.\n");
            return EXIT_FAILURE;
        }

        last_frame = vsl_frame_timestamp(frame);

        vsl_frame_munmap(frame);
        vsl_frame_unlock(frame);
        vsl_frame_release(frame);


    }

    vsl_client_release(client);
    printf("released client\n");

    return 0;
}
