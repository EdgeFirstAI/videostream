// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSL_ENCODER_V4L2_H
#define VSL_ENCODER_V4L2_H

#include "codec_backend.h"
#include "videostream.h"

#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Maximum number of OUTPUT queue buffers (raw input frames).
 * Needs enough depth for pipeline latency.
 */
#define VSL_V4L2_ENC_OUTPUT_BUFFERS 4

/**
 * Maximum number of CAPTURE queue buffers (compressed output).
 * Small count is fine since compressed data is retrieved quickly.
 */
#define VSL_V4L2_ENC_CAPTURE_BUFFERS 4

/**
 * Default size for CAPTURE buffers (compressed data).
 * 2MB should be sufficient for most H.264/HEVC frames.
 */
#define VSL_V4L2_ENC_CAPTURE_BUF_SIZE (2 * 1024 * 1024)

/**
 * Poll timeout in milliseconds for V4L2 operations.
 */
#define VSL_V4L2_ENC_POLL_TIMEOUT_MS 100

/**
 * Maximum number of planes for OUTPUT queue (1 for packed, 2 for NV12).
 */
#define VSL_V4L2_ENC_MAX_PLANES 2

/**
 * V4L2 OUTPUT queue buffer info (raw input frames).
 */
struct vsl_v4l2_enc_output_buffer {
    int    dmabuf_fd; // DMA-BUF file descriptor (imported from source frame)
    bool   queued;    // true if buffer is queued to driver
    size_t plane_sizes[VSL_V4L2_ENC_MAX_PLANES]; // Plane sizes
};

/**
 * V4L2 CAPTURE queue buffer info (compressed output).
 */
struct vsl_v4l2_enc_capture_buffer {
    void*  mmap_ptr;  // mmap'd buffer pointer
    size_t mmap_size; // mmap'd buffer size
    bool   queued;    // true if buffer is queued to driver
};

/**
 * V4L2 encoder internal state.
 *
 * Uses V4L2 mem2mem interface with:
 * - OUTPUT queue: DMABUF buffers for raw input (zero-copy from camera)
 * - CAPTURE queue: MMAP buffers for compressed output
 */
struct vsl_encoder_v4l2 {
    // Backend type - MUST be first field for dispatch logic
    VSLCodecBackend backend;

    int fd; // V4L2 device file descriptor

    // Encoder configuration
    VSLEncoderProfile profile;       // Bitrate profile
    uint32_t          output_fourcc; // Output codec (H264/HEVC)
    int               fps;           // Frame rate

    // Input frame dimensions
    int      width;
    int      height;
    int      stride;
    uint32_t input_fourcc;     // Input pixel format (NV12, BGRA, etc.)
    uint32_t v4l2_input_fmt;   // V4L2 pixel format for OUTPUT queue
    int      num_input_planes; // Number of planes (1 for packed, 2 for NV12)

    // OUTPUT queue (raw input frames)
    struct {
        int                               count;
        struct vsl_v4l2_enc_output_buffer buffers[VSL_V4L2_ENC_OUTPUT_BUFFERS];
        struct v4l2_plane                 planes[VSL_V4L2_ENC_MAX_PLANES];
    } output;

    // CAPTURE queue (compressed output)
    struct {
        int count;
        struct vsl_v4l2_enc_capture_buffer
                          buffers[VSL_V4L2_ENC_CAPTURE_BUFFERS];
        struct v4l2_plane planes[1]; // Single plane for compressed
    } capture;

    // State flags
    bool initialized; // Encoder initialized with first frame
    bool streaming;   // Both queues streaming

    // Statistics
    uint64_t frames_encoded;
    uint64_t total_encode_time_us;
};

/**
 * Create a V4L2-based encoder instance.
 *
 * @param profile Bitrate profile for encoding quality
 * @param output_fourcc Codec type (H264 or HEVC fourcc)
 * @param fps Frame rate for encoding
 * @return Encoder instance or NULL on failure
 */
VSLEncoder*
vsl_encoder_create_v4l2(VSLEncoderProfile profile,
                        uint32_t          output_fourcc,
                        int               fps);

/**
 * Release V4L2 encoder and all associated resources.
 *
 * @param encoder Encoder instance to release
 */
void
vsl_encoder_release_v4l2(VSLEncoder* encoder);

/**
 * Encode a frame using V4L2 mem2mem interface.
 *
 * @param encoder Encoder instance
 * @param source Source frame (raw NV12 data with DMA-BUF)
 * @param destination Pre-allocated destination frame for encoded data
 * @param crop_region Optional crop region (can be NULL)
 * @param keyframe Output: 1 if encoded frame is a keyframe, 0 otherwise
 * @return Encoded size in bytes, or -1 on error
 */
int
vsl_encode_frame_v4l2(VSLEncoder*    encoder,
                      VSLFrame*      source,
                      VSLFrame*      destination,
                      const VSLRect* crop_region,
                      int*           keyframe);

/**
 * Create an output frame suitable for V4L2 encoder.
 *
 * @param encoder Encoder instance
 * @param width Frame width
 * @param height Frame height
 * @param duration Frame duration
 * @param pts Presentation timestamp
 * @param dts Decode timestamp
 * @return New frame or NULL on error
 */
VSLFrame*
vsl_encoder_new_output_frame_v4l2(const VSLEncoder* encoder,
                                  int               width,
                                  int               height,
                                  int64_t           duration,
                                  int64_t           pts,
                                  int64_t           dts);

#endif // VSL_ENCODER_V4L2_H
