// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef HANTRO_ENCODER_H
#define HANTRO_ENCODER_H

#include "videostream.h"
#include "vpu_wrapper.h"

struct vsl_encoder {
    int               outWidth;
    int               outHeight;
    int               fps;
    uint32_t          inputFourcc;
    uint32_t          outputFourcc;
    VpuEncHandle      handle;
    VSLEncoderProfile profile;
    VSLRect*          cropRegion;

    // Internal memory
    VpuMemDesc     phyMem;
    unsigned char* virtMem;
};

#endif // HANTRO_ENCODER_H