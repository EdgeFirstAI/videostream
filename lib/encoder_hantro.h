// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef HANTRO_ENCODER_H
#define HANTRO_ENCODER_H

#include "codec_backend.h"
#include "videostream.h"
#include "vpu_wrapper.h"

/**
 * Hantro encoder internal state.
 *
 * Uses the VC8000e VPU wrapper (libcodec.so) for hardware encoding.
 */
struct vsl_encoder_hantro {
    // Backend type - MUST be first field for dispatch logic
    VSLCodecBackend backend;

    int               out_width;
    int               out_height;
    int               fps;
    uint32_t          input_fourcc;
    uint32_t          output_fourcc;
    VpuEncHandle      handle;
    VSLEncoderProfile profile;
    VSLRect*          crop_region;

    // Internal memory
    VpuMemDesc     phy_mem;
    unsigned char* virt_mem;
};

/**
 * Create a Hantro-based encoder instance.
 *
 * @param profile Bitrate profile for encoding quality
 * @param output_fourcc Codec type (H264 or HEVC fourcc)
 * @param fps Frame rate for encoding
 * @return Encoder instance or NULL on failure
 */
VSLEncoder*
vsl_encoder_create_hantro(VSLEncoderProfile profile,
                          uint32_t          output_fourcc,
                          int               fps);

/**
 * Release Hantro encoder and all associated resources.
 *
 * @param encoder Encoder instance to release
 */
void
vsl_encoder_release_hantro(VSLEncoder* encoder);

/**
 * Encode a frame using Hantro VPU.
 *
 * @param encoder Encoder instance
 * @param source Source frame (raw data)
 * @param destination Pre-allocated destination frame for encoded data
 * @param crop_region Optional crop region (can be NULL)
 * @param keyframe Output: 1 if encoded frame is a keyframe, 0 otherwise
 * @return 0 on success, -1 on error
 */
int
vsl_encode_frame_hantro(VSLEncoder*    encoder,
                        VSLFrame*      source,
                        VSLFrame*      destination,
                        const VSLRect* crop_region,
                        int*           keyframe);

/**
 * Create an output frame suitable for Hantro encoder.
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
vsl_encoder_new_output_frame_hantro(const VSLEncoder* encoder,
                                    int               width,
                                    int               height,
                                    int64_t           duration,
                                    int64_t           pts,
                                    int64_t           dts);

#endif // HANTRO_ENCODER_H