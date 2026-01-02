// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * Unified decoder API with backend selection.
 *
 * This file provides the public decoder API that dispatches to either
 * the V4L2 or Hantro backend based on availability and configuration.
 */

#include "codec_backend.h"
#include "videostream.h"

#ifdef ENABLE_V4L2_CODEC
#include "decoder_v4l2.h"
#endif

#ifdef ENABLE_HANTRO_CODEC
#include "decoder_hantro.h"
#endif

#include <errno.h>
#include <stdio.h>

/**
 * Get the backend type from a decoder instance.
 *
 * The backend field is at offset 0 in both decoder structures,
 * allowing safe access without knowing the concrete type.
 */
static inline VSLCodecBackend
get_decoder_backend(const VSLDecoder* decoder)
{
    // Backend is first field in both structs
    return *((const VSLCodecBackend*) decoder);
}

VSL_API
VSLDecoder*
vsl_decoder_create(VSLDecoderCodec codec, int fps)
{
    // Convert enum to fourcc for the extended API
    uint32_t fourcc;
    switch (codec) {
    case VSL_DEC_H264:
        fourcc = VSL_FOURCC('H', '2', '6', '4');
        break;
    case VSL_DEC_HEVC:
        fourcc = VSL_FOURCC('H', 'E', 'V', 'C');
        break;
    default:
        fprintf(stderr, "vsl_decoder_create: unsupported codec: %d\n", codec);
        errno = EINVAL;
        return NULL;
    }

    return vsl_decoder_create_ex(fourcc, fps, VSL_CODEC_BACKEND_AUTO);
}

VSL_API
VSLDecoder*
vsl_decoder_create_ex(uint32_t codec, int fps, VSLCodecBackend backend)
{
    VSLCodecBackend effective = backend;

    // Resolve AUTO to concrete backend
    if (effective == VSL_CODEC_BACKEND_AUTO) {
        effective = vsl_detect_codec_backend(false /* is_encoder */);
        if (effective == VSL_CODEC_BACKEND_AUTO) {
            fprintf(stderr,
                    "vsl_decoder_create_ex: no codec backend available\n");
            errno = ENODEV;
            return NULL;
        }
    }

    switch (effective) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decoder_create_v4l2(codec, fps);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decoder_create_hantro(codec, fps);
#endif

    default:
        fprintf(stderr,
                "vsl_decoder_create_ex: backend %s not available "
                "(compiled out or unsupported)\n",
                vsl_codec_backend_name(effective));
        errno = ENOTSUP;
        return NULL;
    }
}

VSL_API
VSLDecoderRetCode
vsl_decode_frame(VSLDecoder*  decoder,
                 const void*  data,
                 unsigned int data_length,
                 size_t*      bytes_used,
                 VSLFrame**   output_frame)
{
    if (!decoder) {
        errno = EINVAL;
        return VSL_DEC_ERR;
    }

    VSLCodecBackend backend = get_decoder_backend(decoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decode_frame_v4l2(decoder,
                                     data,
                                     data_length,
                                     bytes_used,
                                     output_frame);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decode_frame_hantro(decoder,
                                       data,
                                       data_length,
                                       bytes_used,
                                       output_frame);
#endif

    default:
        fprintf(stderr, "vsl_decode_frame: unknown backend %d\n", backend);
        errno = EINVAL;
        return VSL_DEC_ERR;
    }
}

VSL_API
int
vsl_decoder_release(VSLDecoder* decoder)
{
    if (!decoder) { return 0; }

    VSLCodecBackend backend = get_decoder_backend(decoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decoder_release_v4l2(decoder);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decoder_release_hantro(decoder);
#endif

    default:
        fprintf(stderr, "vsl_decoder_release: unknown backend %d\n", backend);
        errno = EINVAL;
        return -1;
    }
}

VSL_API
int
vsl_decoder_width(const VSLDecoder* decoder)
{
    if (!decoder) { return 0; }

    VSLCodecBackend backend = get_decoder_backend(decoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decoder_width_v4l2(decoder);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decoder_width_hantro(decoder);
#endif

    default:
        return 0;
    }
}

VSL_API
int
vsl_decoder_height(const VSLDecoder* decoder)
{
    if (!decoder) { return 0; }

    VSLCodecBackend backend = get_decoder_backend(decoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decoder_height_v4l2(decoder);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decoder_height_hantro(decoder);
#endif

    default:
        return 0;
    }
}

VSL_API
VSLRect
vsl_decoder_crop(const VSLDecoder* decoder)
{
    VSLRect empty = {0, 0, 0, 0};

    if (!decoder) { return empty; }

    VSLCodecBackend backend = get_decoder_backend(decoder);

    switch (backend) {
#ifdef ENABLE_V4L2_CODEC
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decoder_crop_v4l2(decoder);
#endif

#ifdef ENABLE_HANTRO_CODEC
    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decoder_crop_hantro(decoder);
#endif

    default:
        return empty;
    }
}
