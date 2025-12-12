// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// DMA-BUF based decoder output frame buffer allocation
//
// This file provides a replacement for VPU_DecGetMem that allocates
// decoder output frame buffers from /dev/dma_heap instead of VPU-specific
// memory. This enables zero-copy frame sharing across processes via
// dmabuf file descriptors.
//
// Key differences from VPU_DecGetMem:
// - Allocates from /dev/dma_heap/linux,cma (or linux,cma-uncached)
// - Returns dmabuf FDs that can be sent via SCM_RIGHTS
// - Uses DMA_BUF_IOCTL_PHYS to get physical address for VPU
// - Compatible with VPU frame buffer registration

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
#include "decoder_hantro.h"
#include "videostream.h"

/**
 * Allocate decoder frame buffers using DMA heap instead of VPU_DecGetMem
 *
 * This function creates multiple frame buffers with valid dmabuf FDs that can
 * be shared across processes. The VPU decoder will write decoded frames to the
 * physical addresses obtained from these dmabufs.
 *
 * @param bufNum Number of frame buffers to allocate (typically 5-10)
 * @param yStride Y plane stride in bytes
 * @param ySize Y plane size in bytes
 * @param uSize U/Cb plane size in bytes
 * @param vSize V/Cr plane size in bytes
 * @param mvSize Motion vector buffer size in bytes
 * @param frameBuf Output VpuFrameBuffer array (must be pre-allocated)
 * @param dmabuf_fds Output array to store dmabuf FDs (must be pre-allocated)
 * @param dmabuf_maps Output array to store mmap pointers (must be
 * pre-allocated)
 * @return 0 on success, -1 on failure
 */
int
vsl_decoder_alloc_frame_buffers_dmabuf(int             bufNum,
                                       int             yStride,
                                       int             ySize,
                                       int             uSize,
                                       int             vSize,
                                       int             mvSize,
                                       VpuFrameBuffer* frameBuf,
                                       int*            dmabuf_fds,
                                       void**          dmabuf_maps)
{
    if (!frameBuf || !dmabuf_fds || !dmabuf_maps) {
        errno = EINVAL;
        return -1;
    }

    int uStride = yStride / 2;

    // Try CMA uncached first (better for VPU coherency), fallback to CMA
    const char* heap_paths[] = {"/dev/dma_heap/linux,cma-uncached",
                                "/dev/dma_heap/linux,cma",
                                NULL};

    const char* selected_heap = NULL;

    // Find available heap
    for (int i = 0; heap_paths[i] != NULL; i++) {
        if (access(heap_paths[i], R_OK | W_OK) == 0) {
            selected_heap = heap_paths[i];
            break;
        }
    }

    if (!selected_heap) {
        fprintf(stderr,
                "%s: No accessible DMA heap found (tried cma-uncached, cma)\n",
                __FUNCTION__);
        return -1;
    }

    // Allocate buffers
    int i;
    for (i = 0; i < bufNum; i++) {
        // Calculate total buffer size for this frame
        size_t totalSize = ySize + uSize + vSize + mvSize;

        // Open DMA heap device
        int heap_fd = open(selected_heap, O_RDWR | O_CLOEXEC);
        if (heap_fd < 0) {
            fprintf(stderr,
                    "%s: Failed to open %s: %s\n",
                    __FUNCTION__,
                    selected_heap,
                    strerror(errno));
            goto cleanup_buffers;
        }

        // Allocate from DMA heap
        struct dma_heap_allocation_data heap_data = {
            .len      = totalSize,
            .fd_flags = O_RDWR | O_CLOEXEC,
        };

        if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data)) {
            fprintf(stderr,
                    "%s: DMA heap allocation failed (%s) for buffer %d: %s\n",
                    __FUNCTION__,
                    selected_heap,
                    i,
                    strerror(errno));
            close(heap_fd);
            goto cleanup_buffers;
        }

        close(heap_fd); // Done with heap device, we have the dmabuf FD now

        // Store dmabuf FD
        dmabuf_fds[i] = heap_data.fd;

        // Get physical address via DMA_BUF_IOCTL_PHYS
        struct dma_buf_phys dma_phys;
        if (ioctl(dmabuf_fds[i], DMA_BUF_IOCTL_PHYS, &dma_phys)) {
            fprintf(stderr,
                    "%s: DMA_BUF_IOCTL_PHYS failed for buffer %d: %s\n",
                    __FUNCTION__,
                    i,
                    strerror(errno));
            goto cleanup_buffers;
        }

        // mmap the buffer for CPU access
        void* map = mmap(NULL,
                         totalSize,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         dmabuf_fds[i],
                         0);

        if (map == MAP_FAILED) {
            fprintf(stderr,
                    "%s: mmap failed for buffer %d: %s\n",
                    __FUNCTION__,
                    i,
                    strerror(errno));
            goto cleanup_buffers;
        }

        dmabuf_maps[i] = map;

#ifndef NDEBUG
        printf("%s: buffer[%d] allocated dmabuf fd=%d paddr=0x%lx size=%zu "
               "from %s\n",
               __FUNCTION__,
               i,
               dmabuf_fds[i],
               dma_phys.phys,
               totalSize,
               selected_heap);
#endif

        // Fill VpuFrameBuffer structure with physical and virtual addresses
        unsigned char* ptr     = (unsigned char*) dma_phys.phys;
        unsigned char* ptrVirt = (unsigned char*) map;

        /* fill stride info */
        frameBuf[i].nStrideY = yStride;
        frameBuf[i].nStrideC = uStride;

        /* fill phy addr */
        frameBuf[i].pbufY     = ptr;
        frameBuf[i].pbufCb    = ptr + ySize;
        frameBuf[i].pbufCr    = ptr + ySize + uSize;
        frameBuf[i].pbufMvCol = ptr + ySize + uSize + vSize;

        /* fill virt addr */
        frameBuf[i].pbufVirtY     = ptrVirt;
        frameBuf[i].pbufVirtCb    = ptrVirt + ySize;
        frameBuf[i].pbufVirtCr    = ptrVirt + ySize + uSize;
        frameBuf[i].pbufVirtMvCol = ptrVirt + ySize + uSize + vSize;

        /* tile bottom (not used for most codecs) */
        frameBuf[i].pbufY_tilebot      = 0;
        frameBuf[i].pbufCb_tilebot     = 0;
        frameBuf[i].pbufVirtY_tilebot  = 0;
        frameBuf[i].pbufVirtCb_tilebot = 0;
    }

    return 0;

cleanup_buffers:
    // Clean up any successfully allocated buffers
    for (int j = 0; j < i; j++) {
        if (dmabuf_maps[j]) {
            size_t bufSize = ySize + uSize + vSize + mvSize;
            munmap(dmabuf_maps[j], bufSize);
            dmabuf_maps[j] = NULL;
        }
        if (dmabuf_fds[j] >= 0) {
            close(dmabuf_fds[j]);
            dmabuf_fds[j] = -1;
        }
    }
    return -1;
}

/**
 * Free decoder frame buffers allocated with DMA heap
 *
 * @param bufNum Number of frame buffers
 * @param ySize Y plane size in bytes
 * @param uSize U/Cb plane size in bytes
 * @param vSize V/Cr plane size in bytes
 * @param mvSize Motion vector buffer size in bytes
 * @param dmabuf_fds Array of dmabuf FDs
 * @param dmabuf_maps Array of mmap pointers
 */
void
vsl_decoder_free_frame_buffers_dmabuf(int    bufNum,
                                      int    ySize,
                                      int    uSize,
                                      int    vSize,
                                      int    mvSize,
                                      int*   dmabuf_fds,
                                      void** dmabuf_maps)
{
    if (!dmabuf_fds || !dmabuf_maps) { return; }

    size_t bufSize = ySize + uSize + vSize + mvSize;

    for (int i = 0; i < bufNum; i++) {
        if (dmabuf_maps[i]) {
            munmap(dmabuf_maps[i], bufSize);
            dmabuf_maps[i] = NULL;
        }
        if (dmabuf_fds[i] >= 0) {
            close(dmabuf_fds[i]);
            dmabuf_fds[i] = -1;
        }
    }

#ifndef NDEBUG
    printf("%s: freed %d decoder frame buffers\n", __FUNCTION__, bufNum);
#endif
}
