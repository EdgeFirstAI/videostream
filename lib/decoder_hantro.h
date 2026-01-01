// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef HANTRO_DECODER_H
#define HANTRO_DECODER_H

#include "codec_backend.h"
#include "videostream.h"
#include "vpu_wrapper.h"

/**
 * Hantro VPU decoder internal state.
 *
 * Uses libcodec.so via vpu_wrapper for hardware decoding.
 * This structure is used when VSL_CODEC_BACKEND_HANTRO is selected.
 */
struct vsl_decoder_hantro {
    // Backend type - MUST be first field for dispatch logic
    VSLCodecBackend backend;

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

/**
 * Create a Hantro/libcodec.so-based decoder instance.
 *
 * @param codec Codec type (VSL_DEC_H264 or VSL_DEC_HEVC)
 * @param fps Frame rate hint for buffer management
 * @return Decoder instance or NULL on failure
 */
VSLDecoder*
vsl_decoder_create_hantro(uint32_t codec, int fps);

/**
 * Release Hantro decoder and all associated resources.
 *
 * @param decoder Decoder instance to release
 * @return 0 on success, -1 on error
 */
int
vsl_decoder_release_hantro(VSLDecoder* decoder);

/**
 * Decode a frame using Hantro VPU.
 *
 * @param decoder Decoder instance
 * @param data Compressed frame data
 * @param data_length Length of compressed data
 * @param bytes_used Output: bytes consumed from data
 * @param output_frame Output: decoded frame (if available)
 * @return VSLDecoderRetCode status
 */
VSLDecoderRetCode
vsl_decode_frame_hantro(VSLDecoder*  decoder,
                        const void*  data,
                        unsigned int data_length,
                        size_t*      bytes_used,
                        VSLFrame**   output_frame);

/**
 * Get decoded frame width.
 */
int
vsl_decoder_width_hantro(const VSLDecoder* decoder);

/**
 * Get decoded frame height.
 */
int
vsl_decoder_height_hantro(const VSLDecoder* decoder);

/**
 * Get crop region.
 */
VSLRect
vsl_decoder_crop_hantro(const VSLDecoder* decoder);

#endif // HANTRO_DECODER_H