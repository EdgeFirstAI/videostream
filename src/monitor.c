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

int
main(int argc, char** argv)
{
    struct g2d* g2d        = NULL;
    void*       g2d_handle = NULL;
    int64_t     last_frame = 0;
    VSLClient*  client;

    g2d = g2d_initialize(NULL, NULL);
    if (!g2d) {
        printf(
            "[WARNING] unable to inialize g2d, only RGB will be supported.\n");
    }

    if (g2d->open(&g2d_handle)) {
        fprintf(stderr, "failed to open g2d library\n");
        return EXIT_FAILURE;
    }

    client = vsl_client_init("/tmp/camhost.0", NULL, false);

    if (!client) {
        fprintf(stderr,
                "failed to connect to videostream host /tmp/camhost.0: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    printf("connected to /tmp/camhost.0\n");

    for (int i = 0; i < 300; i++) {
        // Wait for a frame 5 seconds from now.
        // VSLFrame* frame = vsl_frame_wait(client, vsl_timestamp() + 5 * 1000);
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

        printf("acquired video frame %dx%d format:%c%c%c%c paddr:%lx "
               "frame_time: %ld\n",
               width,
               height,
               fourcc,
               fourcc >> 8,
               fourcc >> 16,
               fourcc >> 24,
               paddr,
               vsl_frame_timestamp(frame) - last_frame);

        if (vsl_frame_trylock(frame)) {
            fprintf(stderr, "failed to lock frame: %s\n", strerror(errno));
            vsl_client_release(client);
            return EXIT_FAILURE;
        }

        if (fourcc == 0x33424752) {
            size_t         size;
            const uint8_t* buffer = vsl_frame_mmap(frame, &size);

            if (!buffer) {
                fprintf(stderr, "failed to mmap frame: %s\n", strerror(errno));
                vsl_frame_unlock(frame);
                vsl_client_release(client);
                return EXIT_FAILURE;
            }

            stbi_write_jpg("/tmp/frame.jpg", width, height, 3, buffer, 90);
            printf("saved frame to /tmp/frame.jpg\n");
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
                       srcbuf->buf_paddr);
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

            stbi_write_jpg("/tmp/frame.jpg", width, height, 4, buf, 90);
            printf("saved frame to /tmp/frame.jpg\n");

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

        struct timespec delay = {2, 0};
        nanosleep(&delay, NULL);
    }

    vsl_client_release(client);
    printf("released client\n");

    return 0;
}
