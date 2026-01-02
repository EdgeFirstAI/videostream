// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * Unified encoder API with backend selection.
 *
 * This file provides the public encoder API that dispatches to either
 * the V4L2 or Hantro backend based on availability and configuration.
 */

#include "codec_backend.h"
#include "videostream.h"

#ifdef ENABLE_V4L2_CODEC
#include "encoder_v4l2.h"
#endif

#ifdef ENABLE_HANTRO_CODEC
#include "encoder_hantro.h"
#endif

#include <errno.h>
#include <stdio.h>

/**
 * Get the backend type from an encoder instance.
 *
 * The backend field is at offset 0 in both encoder structures,
 * allowing safe access without knowing the concrete type.
 */
static inline VSLCodecBackend
get_encoder_backend(const VSLEncoder* encoder)
{
    // Backend is first field in both structs
    return *((const VSLCodecBackend*) encoder);
}

VSL_API
VSLEncoder*
vsl_encoder_create(VSLEncoderProfile profile, uint32_t outputFourcc, int fps)
{
    return vsl_encoder_create_ex(profile, outputFourcc, fps, VSL_CODEC_BACKEND_AUTO);
}

VSL_API
VSLEncoder*
vsl_encoder_create_ex(VSLEncoderProfile profile,
                      uint32_t          outputFourcc,
                      int               fps,
                      VSLCodecBackend   backend)
{
    VSLCodecBackend effective = backend;

    // Resolve AUTO to concrete backend
    if (effective == VSL_CODEC_BACKEND_AUTO) {
        effective = vsl_detect_codec_backend(true /* is_encoder */);
        if (effective == VSL_CODEC_BACKEND_AUTO) {
            fprintf(stderr,
                    "vsl_encoder_create_ex: no codec backend available\n");
            errno = ENODEV;
            return NULL;
        }
    }

    switch (effective) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_encoder_create_v4l2(profile, outputFourcc, fps);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_encoder_create_hantro(profile, outputFourcc, fps);
#endif

    default:
        fprintf(stderr,
                "vsl_encoder_create_ex: backend %s not available "
                "(compiled out or unsupported)\n",
                vsl_codec_backend_name(effective));
        errno = ENOTSUP;
        return NULL;
    }
}

VSL_API
int
vsl_encode_frame(VSLEncoder*    encoder,
                 VSLFrame*      source,
                 VSLFrame*      destination,
                 const VSLRect* cropRegion,
                 int*           keyframe)
{
    if (!encoder) {
        errno = EINVAL;
        return -1;
    }

    VSLCodecBackend backend = get_encoder_backend(encoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_encode_frame_v4l2(encoder,
                                     source,
                                     destination,
                                     cropRegion,
                                     keyframe);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_encode_frame_hantro(encoder,
                                       source,
                                       destination,
                                       cropRegion,
                                       keyframe);
#endif

    default:
        fprintf(stderr, "vsl_encode_frame: unknown backend %d\n", backend);
        errno = EINVAL;
        return -1;
    }
}

VSL_API
void
vsl_encoder_release(VSLEncoder* encoder)
{
    if (!encoder) { return; }

    VSLCodecBackend backend = get_encoder_backend(encoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        vsl_encoder_release_v4l2(encoder);
        return;
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        vsl_encoder_release_hantro(encoder);
        return;
#endif

    default:
        fprintf(stderr, "vsl_encoder_release: unknown backend %d\n", backend);
    }
}

VSL_API
VSLFrame*
vsl_encoder_new_output_frame(const VSLEncoder* encoder,
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

    VSLCodecBackend backend = get_encoder_backend(encoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_encoder_new_output_frame_v4l2(encoder,
                                                 width,
                                                 height,
                                                 duration,
                                                 pts,
                                                 dts);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_encoder_new_output_frame_hantro(encoder,
                                                   width,
                                                   height,
                                                   duration,
                                                   pts,
                                                   dts);
#endif

    default:
        fprintf(stderr,
                "vsl_encoder_new_output_frame: unknown backend %d\n",
                backend);
        errno = EINVAL;
        return NULL;
    }
}
