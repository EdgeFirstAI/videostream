// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSL_DECODER_V4L2_H
#define VSL_DECODER_V4L2_H

#include "codec_backend.h"
#include "videostream.h"

#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Maximum number of OUTPUT queue buffers (compressed input).
 * Small count is fine since compressed data is small.
 */
#define VSL_V4L2_DEC_OUTPUT_BUFFERS 4

/**
 * Maximum number of CAPTURE queue buffers (decoded frames).
 * Larger count needed for B-frame reordering and pipeline depth.
 */
#define VSL_V4L2_DEC_CAPTURE_BUFFERS 8

/**
 * Default size for OUTPUT buffers (compressed data).
 * 2MB should be sufficient for most H.264/HEVC frames.
 */
#define VSL_V4L2_DEC_OUTPUT_BUF_SIZE (2 * 1024 * 1024)

/**
 * Poll timeout in milliseconds for V4L2 operations.
 */
#define VSL_V4L2_POLL_TIMEOUT_MS 100

/**
 * V4L2 OUTPUT queue buffer info (compressed input).
 */
struct vsl_v4l2_output_buffer {
    void*  mmap_ptr;  // mmap'd buffer pointer
    size_t mmap_size; // mmap'd buffer size
    bool   queued;    // true if buffer is queued to driver
};

/**
 * V4L2 CAPTURE queue buffer info (decoded frames).
 */
struct vsl_v4l2_capture_buffer {
    int       dmabuf_fd; // DMA-BUF file descriptor
    VSLFrame* frame;     // Associated VSLFrame
    bool      queued;    // true if buffer is queued to driver
};

/**
 * V4L2 decoder internal state.
 *
 * Uses V4L2 mem2mem interface with:
 * - OUTPUT queue: MMAP buffers for compressed input
 * - CAPTURE queue: DMABUF buffers for decoded frames (zero-copy)
 */
struct vsl_decoder_v4l2 {
    // Backend type - MUST be first field for dispatch logic
    VSLCodecBackend backend;

    int fd; // V4L2 device file descriptor

    // Codec configuration
    VSLDecoderCodec codec;      // H.264 or HEVC
    int             fps;        // Frame rate hint
    uint32_t        out_fourcc; // Output pixel format (NV12)

    // Decoded frame dimensions (set after INIT_OK)
    int     width;
    int     height;
    VSLRect crop_region;

    // OUTPUT queue (compressed data input)
    struct {
        int                           count;
        struct vsl_v4l2_output_buffer buffers[VSL_V4L2_DEC_OUTPUT_BUFFERS];
        struct v4l2_plane             planes[1]; // Single plane for compressed
    } output;

    // CAPTURE queue (decoded frame output)
    struct {
        int                            count;
        struct vsl_v4l2_capture_buffer buffers[VSL_V4L2_DEC_CAPTURE_BUFFERS];
        size_t                         plane_sizes[2]; // Y and UV plane sizes
        int                            stride;
    } capture;

    // State flags
    bool initialized;           // Initial info received
    bool output_streaming;      // OUTPUT STREAMON called
    bool streaming;             // Both queues streaming
    bool source_change_pending; // Resolution change detected

    // Statistics
    uint64_t frames_decoded;
    uint64_t total_decode_time_us;
};

/**
 * Create a V4L2-based decoder instance.
 *
 * @param codec Codec type (VSL_DEC_H264 or VSL_DEC_HEVC)
 * @param fps Frame rate hint for buffer management
 * @return Decoder instance or NULL on failure
 */
VSLDecoder*
vsl_decoder_create_v4l2(uint32_t codec, int fps);

/**
 * Release V4L2 decoder and all associated resources.
 *
 * @param decoder Decoder instance to release
 * @return 0 on success, -1 on error
 */
int
vsl_decoder_release_v4l2(VSLDecoder* decoder);

/**
 * Decode a frame using V4L2 mem2mem interface.
 *
 * @param decoder Decoder instance
 * @param data Compressed frame data
 * @param data_length Length of compressed data
 * @param bytes_used Output: bytes consumed from data
 * @param output_frame Output: decoded frame (if available)
 * @return VSLDecoderRetCode status
 */
VSLDecoderRetCode
vsl_decode_frame_v4l2(VSLDecoder*  decoder,
                      const void*  data,
                      unsigned int data_length,
                      size_t*      bytes_used,
                      VSLFrame**   output_frame);

/**
 * Get decoded frame width.
 */
int
vsl_decoder_width_v4l2(const VSLDecoder* decoder);

/**
 * Get decoded frame height.
 */
int
vsl_decoder_height_v4l2(const VSLDecoder* decoder);

/**
 * Get crop region.
 */
VSLRect
vsl_decoder_crop_v4l2(const VSLDecoder* decoder);

#endif // VSL_DECODER_V4L2_H
