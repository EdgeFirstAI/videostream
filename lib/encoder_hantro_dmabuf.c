// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// DMA-BUF based encoder output frame allocation
//
// This file provides a replacement for VPU_EncGetMem that allocates
// encoder output buffers from /dev/dma_heap instead of VPU-specific
// memory. This enables zero-copy frame sharing across processes via
// dmabuf file descriptors.
//
// Key differences from VPU_EncGetMem:
// - Allocates from /dev/dma_heap/linux,cma (or linux,cma-uncached)
// - Returns dmabuf FD that can be sent via SCM_RIGHTS
// - Uses DMA_BUF_IOCTL_PHYS to get physical address for VPU
// - Compatible with vsl_frame_alloc() infrastructure

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../ext/include/dma-buf.h"
#include "../ext/include/dma-heap.h"
#include "encoder_hantro.h"
#include "frame.h"
#include "videostream.h"

/**
 * Allocate encoder output frame using DMA heap instead of VPU_EncGetMem
 *
 * This function creates a frame with a valid dmabuf FD that can be shared
 * across processes. The VPU encoder will write encoded data to the physical
 * address obtained from this dmabuf.
 *
 * @param encoder Encoder instance (for fourcc)
 * @param width Frame width
 * @param height Frame height
 * @param duration Frame duration in ns
 * @param pts Presentation timestamp
 * @param dts Decode timestamp
 * @return VSLFrame* with valid handle (dmabuf FD) or NULL on failure
 */
VSL_API
VSLFrame*
vsl_encoder_new_output_frame_dmabuf(const VSLEncoder* encoder,
                                    int               width,
                                    int               height,
                                    int64_t           duration,
                                    int64_t           pts,
                                    int64_t           dts)
{
    if (!encoder) {
        errno = EINVAL;
        return NULL;
    }

    const struct vsl_encoder_hantro* enc =
        (const struct vsl_encoder_hantro*) encoder;

    // Allocate 1MB for encoder output (same as VPU_EncGetMem)
    // This is sufficient for 1080p H.264/HEVC keyframes at reasonable bitrates
    const size_t output_size = 1024 * 1024;

    // Create frame with encoder output fourcc (H264 or HEVC)
    VSLFrame* frame =
        vsl_frame_init(width,
                       height,
                       -1, // stride not relevant for encoded frames
                       enc->output_fourcc,
                       NULL,  // no userptr needed
                       NULL); // no cleanup callback needed
    if (!frame) {
        fprintf(stderr, "%s: vsl_frame_init failed\n", __FUNCTION__);
        return NULL;
    }

    // Override the size before allocation (encoded frames don't use stride
    // calculation)
    frame->info.size = output_size;

    // Allocate DMA heap buffer
    // Try CMA uncached first (better for VPU coherency), fallback to CMA
    const char* heap_paths[] = {"/dev/dma_heap/linux,cma-uncached",
                                "/dev/dma_heap/linux,cma",
                                NULL};

    int         heap_fd       = -1;
    const char* selected_heap = NULL;

    for (int i = 0; heap_paths[i] != NULL; i++) {
        if (access(heap_paths[i], R_OK | W_OK) == 0) {
            heap_fd = open(heap_paths[i], O_RDWR | O_CLOEXEC);
            if (heap_fd >= 0) {
                selected_heap = heap_paths[i];
                break;
            }
        }
    }

    if (heap_fd < 0) {
        fprintf(stderr,
                "%s: No accessible DMA heap found (tried cma-uncached, cma)\n",
                __FUNCTION__);
        vsl_frame_release(frame);
        return NULL;
    }

    struct dma_heap_allocation_data heap_data = {
        .len      = output_size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data)) {
        fprintf(stderr,
                "%s: DMA heap allocation failed (%s): %s\n",
                __FUNCTION__,
                selected_heap,
                strerror(errno));
        close(heap_fd);
        vsl_frame_release(frame);
        return NULL;
    }

    close(heap_fd); // Done with heap device, we have the dmabuf FD now

    // Store dmabuf FD
    frame->handle    = heap_data.fd;
    frame->allocator = VSL_FRAME_ALLOCATOR_DMAHEAP;
    frame->path      = strdup(selected_heap);

    // Get physical address via DMA_BUF_IOCTL_PHYS
    struct dma_buf_phys dma_phys;
    if (ioctl(frame->handle, DMA_BUF_IOCTL_PHYS, &dma_phys)) {
        fprintf(stderr,
                "%s: DMA_BUF_IOCTL_PHYS failed: %s\n",
                __FUNCTION__,
                strerror(errno));
        // Close handle and clear it to prevent double-close in
        // vsl_frame_release
        close(frame->handle);
        frame->handle = -1;
        free(frame->path);
        frame->path = NULL;
        vsl_frame_release(frame);
        return NULL;
    }

    frame->info.paddr = dma_phys.phys;

#ifndef NDEBUG
    printf("%s: allocated dmabuf fd=%d paddr=0x%lx size=%zu from %s\n",
           __FUNCTION__,
           frame->handle,
           frame->info.paddr,
           output_size,
           selected_heap);
#endif

    // mmap the buffer for CPU access
    void* map = mmap(NULL,
                     output_size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     frame->handle,
                     0);

    if (map == MAP_FAILED) {
        fprintf(stderr, "%s: mmap failed: %s\n", __FUNCTION__, strerror(errno));
        // Close handle and clear it to prevent double-close in
        // vsl_frame_release
        close(frame->handle);
        frame->handle = -1;
        free(frame->path);
        frame->path = NULL;
        vsl_frame_release(frame);
        return NULL;
    }

    frame->map     = map;
    frame->mapsize = output_size;

    // Set frame metadata
    frame->info.duration = duration;
    frame->info.pts      = pts;
    frame->info.dts      = dts;
    frame->info.offset   = 0;

    return frame;
}
