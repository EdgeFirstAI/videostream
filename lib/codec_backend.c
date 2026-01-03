// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "codec_backend.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/videodev2.h>
#endif

const char*
vsl_codec_backend_name(VSLCodecBackend backend)
{
    switch (backend) {
    case VSL_CODEC_BACKEND_AUTO:
        return "auto";
    case VSL_CODEC_BACKEND_HANTRO:
        return "hantro";
    case VSL_CODEC_BACKEND_V4L2:
        return "v4l2";
    default:
        return "unknown";
    }
}

bool
vsl_v4l2_codec_available(bool is_encoder)
{
#ifdef __linux__
    const char* dev = is_encoder ? VSL_V4L2_ENCODER_DEV : VSL_V4L2_DECODER_DEV;

    // Open device and verify it has M2M capability
    // Note: We don't use access() before open() to avoid TOCTOU race condition
    int fd = open(dev, O_RDWR | O_NONBLOCK);
    if (fd < 0) { return false; }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    bool available = false;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        // Use device_caps if V4L2_CAP_DEVICE_CAPS is set, otherwise
        // capabilities
        __u32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
                         ? cap.device_caps
                         : cap.capabilities;

        // Check for M2M capability (vsi_v4l2 uses single-planar M2M)
        if (caps & (V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE)) {
            available = true;
#ifndef NDEBUG
            fprintf(stderr,
                    "[codec_backend] V4L2 %s device available: %s (%s)\n",
                    is_encoder ? "encoder" : "decoder",
                    dev,
                    cap.card);
#endif
        }
    }

    close(fd);
    return available;
#else
    (void) is_encoder;
    return false;
#endif
}

bool
vsl_hantro_codec_available(bool is_encoder)
{
    const char* dev =
        is_encoder ? VSL_HANTRO_ENCODER_DEV : VSL_HANTRO_DECODER_DEV;

    bool available = (access(dev, R_OK | W_OK) == 0);

#ifndef NDEBUG
    if (available) {
        fprintf(stderr,
                "[codec_backend] Hantro %s device available: %s\n",
                is_encoder ? "encoder" : "decoder",
                dev);
    }
#endif

    return available;
}

VSLCodecBackend
vsl_detect_codec_backend(bool is_encoder)
{
    const char* type_str = is_encoder ? "encoder" : "decoder";

    // 1. Check environment variable override first
    const char* env = getenv(VSL_CODEC_BACKEND_ENV);
    if (env && *env) {
        if (strcasecmp(env, "hantro") == 0) {
#ifndef NDEBUG
            fprintf(stderr,
                    "[codec_backend] %s: forced to HANTRO via %s\n",
                    type_str,
                    VSL_CODEC_BACKEND_ENV);
#endif
            return VSL_CODEC_BACKEND_HANTRO;
        } else if (strcasecmp(env, "v4l2") == 0) {
#ifndef NDEBUG
            fprintf(stderr,
                    "[codec_backend] %s: forced to V4L2 via %s\n",
                    type_str,
                    VSL_CODEC_BACKEND_ENV);
#endif
            return VSL_CODEC_BACKEND_V4L2;
        }
        // "auto" or unknown value falls through to auto-detection
    }

    // 2. Prefer V4L2 if available (faster, more stable)
    if (vsl_v4l2_codec_available(is_encoder)) {
#ifndef NDEBUG
        fprintf(stderr,
                "[codec_backend] %s: auto-selected V4L2 (preferred)\n",
                type_str);
#endif
        return VSL_CODEC_BACKEND_V4L2;
    }

    // 3. Fall back to Hantro if available
    if (vsl_hantro_codec_available(is_encoder)) {
#ifndef NDEBUG
        fprintf(stderr,
                "[codec_backend] %s: auto-selected HANTRO (V4L2 unavailable)\n",
                type_str);
#endif
        return VSL_CODEC_BACKEND_HANTRO;
    }

    // 4. No backend available
#ifndef NDEBUG
    fprintf(stderr, "[codec_backend] %s: no backend available\n", type_str);
#endif
    return VSL_CODEC_BACKEND_AUTO; // Will cause create to fail
}
