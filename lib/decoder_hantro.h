// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef HANTRO_DECODER_H
#define HANTRO_DECODER_H

#include "videostream.h"
#include "vpu_wrapper.h"

struct vsl_decoder {
    int             outWidth;
    int             outHeight;
    int             fps;
    VSLDecoderCodec inputCodec;
    uint32_t        outputFourcc;
    VpuDecHandle    handle;
    VSLRect         cropRegion;

    // Internal memory
    VpuMemDesc     phyMem;
    unsigned char* virtMem;

    // DMA heap frame buffers (for cross-process sharing)
    int    frameBufCount;  // Number of frame buffers allocated
    int*   frameBufFds;    // Array of dmabuf file descriptors
    void** frameBufMaps;   // Array of mmap pointers
    int    frameBufYSize;  // Y plane size (for cleanup)
    int    frameBufUSize;  // U/Cb plane size (for cleanup)
    int    frameBufVSize;  // V/Cr plane size (for cleanup)
    int    frameBufMvSize; // Motion vector size (for cleanup)
};

// DMA heap frame buffer allocation (decoder_hantro_dmabuf.c)
int
vsl_decoder_alloc_frame_buffers_dmabuf(int             bufNum,
                                       int             yStride,
                                       int             ySize,
                                       int             uSize,
                                       int             vSize,
                                       int             mvSize,
                                       VpuFrameBuffer* frameBuf,
                                       int*            dmabuf_fds,
                                       void**          dmabuf_maps);

void
vsl_decoder_free_frame_buffers_dmabuf(int    bufNum,
                                      int    ySize,
                                      int    uSize,
                                      int    vSize,
                                      int    mvSize,
                                      int*   dmabuf_fds,
                                      void** dmabuf_maps);

#endif // HANTRO_DECODER_H