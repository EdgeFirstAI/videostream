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
};

#endif // HANTRO_DECODER_H