// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSL_CODEC_BACKEND_H
#define VSL_CODEC_BACKEND_H

#include "videostream.h"

#include <stdbool.h>

/**
 * Environment variable to override backend selection.
 *
 * Values:
 *   "hantro" - Force Hantro backend (libcodec.so) even if V4L2 available
 *   "v4l2"   - Force V4L2 backend (fail if unavailable)
 *   "auto"   - Auto-detect best backend (default)
 */
#define VSL_CODEC_BACKEND_ENV "VSL_CODEC_BACKEND"

/**
 * V4L2 device paths for encoder and decoder.
 * These are the vsi_v4l2 driver device nodes on i.MX 8M Plus.
 * video0 = encoder (vsi_v4l2enc), video1 = decoder (vsi_v4l2dec)
 */
#define VSL_V4L2_ENCODER_DEV "/dev/video0"
#define VSL_V4L2_DECODER_DEV "/dev/video1"

/**
 * Hantro device paths for encoder and decoder.
 * These are the user-space DWL interface devices on i.MX 8M Plus.
 */
#define VSL_HANTRO_ENCODER_DEV "/dev/mxc_hantro_vc8000e"
#define VSL_HANTRO_DECODER_DEV "/dev/mxc_hantro"

// VSLCodecBackend enum is defined in videostream.h

/**
 * Detect the best available codec backend.
 *
 * Checks device availability and environment variable override to
 * determine which backend to use.
 *
 * @param is_encoder true for encoder, false for decoder
 * @return Detected backend type, or VSL_CODEC_BACKEND_AUTO if none available
 */
VSLCodecBackend
vsl_detect_codec_backend(bool is_encoder);

/**
 * Check if V4L2 codec device is available and has M2M capability.
 *
 * @param is_encoder true to check encoder device, false for decoder
 * @return true if V4L2 M2M device is available
 */
bool
vsl_v4l2_codec_available(bool is_encoder);

/**
 * Check if Hantro device is available.
 *
 * @param is_encoder true to check encoder device, false for decoder
 * @return true if Hantro device is accessible
 */
bool
vsl_hantro_codec_available(bool is_encoder);

/**
 * Get string name for backend type (for logging).
 *
 * @param backend Backend type
 * @return String name ("auto", "hantro", "v4l2")
 */
const char*
vsl_codec_backend_name(VSLCodecBackend backend);

#endif // VSL_CODEC_BACKEND_H
