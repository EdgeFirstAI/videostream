// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "encoder_hantro.h"
#include "frame.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define Align(ptr, align) \
    (((unsigned long) ptr + (align) - 1) / (align) * (align))

static int
vpu_codec_from_fourcc(uint32_t fourcc)
{
    switch (fourcc) {
    case VSL_FOURCC('H', '2', '6', '4'):
        return VPU_V_AVC;
    case VSL_FOURCC('H', 'E', 'V', 'C'):
        return VPU_V_HEVC;
    default:
        return -1;
    }
}

static int
vpu_color_from_fourcc(uint32_t fourcc, int* chromaInterleave)
{
    switch (fourcc) {
    case VSL_FOURCC('R', 'G', 'B', 'A'):
        *chromaInterleave = false;
        return VPU_COLOR_ARGB8888;
    case VSL_FOURCC('B', 'G', 'R', 'A'):
        *chromaInterleave = false;
        return VPU_COLOR_BGRA8888;
    case VSL_FOURCC('Y', 'U', 'Y', 'V'):
    case VSL_FOURCC('Y', 'U', 'Y', '2'):
        *chromaInterleave = false;
        return VPU_COLOR_422YUYV;
    case VSL_FOURCC('U', 'Y', 'V', 'Y'):
        *chromaInterleave = false;
        return VPU_COLOR_422UYVY;
    case VSL_FOURCC('N', 'V', '1', '2'):
        *chromaInterleave = true;
        return VPU_COLOR_420;
    case VSL_FOURCC('I', '4', '2', '0'):
        *chromaInterleave = false;
        return VPU_COLOR_420;
    default:
        return -1;
    }
}

VSL_API
VSLEncoder*
vsl_encoder_create(VSLEncoderProfile profile, uint32_t outputFourcc, int fps)
{
    VSLEncoder* encoder = (VSLEncoder*) calloc(1, sizeof(VSLEncoder));
    if (!encoder) {
#ifndef DEBUG
        fprintf(stderr,
                "%s: encoder struct allocation failed: %s\n",
                __FUNCTION__,
                strerror(errno));
#endif
        return NULL;
    }

    encoder->fps          = fps;
    encoder->outputFourcc = outputFourcc;
    encoder->profile      = profile;

    VpuEncRetCode  ret;
    VpuVersionInfo ver;

    ret = VPU_EncLoad();
    if (ret != VPU_ENC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_EncLoad failed: %d\n", __FUNCTION__, ret);
        free(encoder);
        return NULL;
    }

    ret = VPU_EncGetVersionInfo(&ver);
    if (ret != VPU_ENC_RET_SUCCESS) {
        fprintf(stderr,
                "%s: VPU_EncGetVersionInfo failed: %d\n",
                __FUNCTION__,
                ret);
        free(encoder);
        return NULL;
    }
#ifndef NDEBUG
    fprintf(stdout,
            "vpu lib version: %d.%d.%d\n",
            ver.nLibMajor,
            ver.nLibMinor,
            ver.nLibRelease);
    fprintf(stdout,
            "vpu fw version: %d.%d.%d_r%d\n",
            ver.nFwMajor,
            ver.nFwMinor,
            ver.nFwRelease,
            ver.nFwCode);
#endif

    // do not print vpu_wrapper version, no need since it's internal anyway

    return encoder;
}

static int
vsl_encoder_init(VSLEncoder*    encoder,
                 uint32_t       inputFourcc,
                 int            inWidth,
                 int            inHeight,
                 const VSLRect* cropRegion)
{
    VpuEncRetCode       ret;
    VpuMemInfo          sMemInfo;
    VpuEncOpenParamSimp sEncOpenParamSimp;

    // Use memset to avoid aarch64 stack issues with designated initializers
    memset(&sMemInfo, 0, sizeof(sMemInfo));
    memset(&sEncOpenParamSimp, 0, sizeof(sEncOpenParamSimp));

    encoder->inputFourcc = inputFourcc;
    if (cropRegion) {
        encoder->cropRegion = calloc(1, sizeof(VSLRect));
        memcpy(encoder->cropRegion, cropRegion, sizeof(VSLRect));
    }

    ret = VPU_EncQueryMem(&sMemInfo);
    if (ret != VPU_ENC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_EncQueryMem failed: %d\n", __FUNCTION__, ret);
        goto err_crop;
    }

    // we know what to expect, one phy (idx 1) and one virt (idx 0) block, check
    // it in case something changed
    if (sMemInfo.nSubBlockNum != 2 ||
        sMemInfo.MemSubBlock[0].MemType != VPU_MEM_VIRT ||
        sMemInfo.MemSubBlock[1].MemType != VPU_MEM_PHY) {
        fprintf(stderr,
                "%s: VPU_EncQueryMem returned unexpected memory block "
                "layout.\n",
                __FUNCTION__);
        goto err_crop;
    }

    sMemInfo.MemSubBlock[0].pVirtAddr =
        calloc(1, sMemInfo.MemSubBlock[0].nSize);
    encoder->virtMem = sMemInfo.MemSubBlock[0].pVirtAddr;

    encoder->phyMem.nSize = sMemInfo.MemSubBlock[1].nSize;

    ret = VPU_EncGetMem(&encoder->phyMem);
    if (ret != VPU_ENC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_EncGetMem failed: %d\n", __FUNCTION__, ret);
        goto err_virtmem;
    }

    sMemInfo.MemSubBlock[1].pVirtAddr =
        (unsigned char*) encoder->phyMem.nVirtAddr;
    sMemInfo.MemSubBlock[1].pPhyAddr =
        (unsigned char*) encoder->phyMem.nPhyAddr;

    int vpuCodec = vpu_codec_from_fourcc(encoder->outputFourcc);
    if (vpuCodec == -1) {
        fprintf(stderr,
                "%s: unsupported output codec: %d\n",
                __FUNCTION__,
                encoder->outputFourcc);
        goto err_phymem;
    }

    sEncOpenParamSimp.eFormat = vpuCodec;

    int chromaInterleave = 0;
    int vpuColor = vpu_color_from_fourcc(inputFourcc, &chromaInterleave);
    if (vpuColor == -1) {
        fprintf(stderr,
                "%s: unsupported input color format: %d\n",
                __FUNCTION__,
                inputFourcc);
        goto err_phymem;
    }

    sEncOpenParamSimp.nChromaInterleave = chromaInterleave;
    sEncOpenParamSimp.eColorFormat      = vpuColor;

    if (cropRegion) {
        sEncOpenParamSimp.nOrigWidth  = inWidth;
        sEncOpenParamSimp.nOrigHeight = inHeight;
        sEncOpenParamSimp.nPicWidth   = encoder->cropRegion->width;
        sEncOpenParamSimp.nPicHeight  = encoder->cropRegion->height;
        sEncOpenParamSimp.nXOffset    = encoder->cropRegion->x;
        sEncOpenParamSimp.nYOffset    = encoder->cropRegion->y;
        encoder->outWidth             = encoder->cropRegion->width;
        encoder->outHeight            = encoder->cropRegion->height;
    } else {
        sEncOpenParamSimp.nPicWidth  = inWidth;
        sEncOpenParamSimp.nPicHeight = inHeight;
        encoder->outWidth            = inWidth;
        encoder->outHeight           = inHeight;
    }

    sEncOpenParamSimp.nFrameRate = encoder->fps;

    sEncOpenParamSimp.nGOPSize = encoder->fps;
    sEncOpenParamSimp.nBitRate = 0;
    sEncOpenParamSimp.nIntraQP = 0;

    switch (encoder->outputFourcc) {
    case VSL_FOURCC('H', '2', '6', '4'):
    case VSL_FOURCC('H', 'E', 'V', 'C'):
        switch (encoder->profile) {
        case VSL_ENCODE_PROFILE_5000_KBPS:
            sEncOpenParamSimp.nBitRate = 5000;
            break;
        case VSL_ENCODE_PROFILE_25000_KBPS:
            sEncOpenParamSimp.nBitRate = 25000;
            break;
        case VSL_ENCODE_PROFILE_50000_KBPS:
            sEncOpenParamSimp.nBitRate = 50000;
            break;
        case VSL_ENCODE_PROFILE_100000_KBPS:
            sEncOpenParamSimp.nBitRate = 100000;
            break;
        case VSL_ENCODE_PROFILE_AUTO:
        default:
            break;
        }
        break;

    default:
        fprintf(stderr, "Missing encode profile implementation\n");
        assert(false);
    }

    ret = VPU_EncOpenSimp(&encoder->handle, &sMemInfo, &sEncOpenParamSimp);
    if (ret != VPU_ENC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_EncOpenSimp failed: %d\n", __FUNCTION__, ret);
        goto err_phymem;
    }

    VpuEncInitInfo sEncInitInfo;

    ret = VPU_EncGetInitialInfo(&encoder->handle, &sEncInitInfo);
    if (VPU_ENC_RET_SUCCESS != ret) {
        fprintf(stderr,
                "%s: VPU_EncGetInitialInfo failed: %d\n",
                __FUNCTION__,
                ret);
        goto err_handle;
    }

    return 0;

err_handle:
    VPU_EncClose(encoder->handle);
    encoder->handle = NULL;
err_phymem:
    VPU_EncFreeMem(&encoder->phyMem);
    encoder->phyMem.nPhyAddr = 0;
err_virtmem:
    free(encoder->virtMem);
    encoder->virtMem = NULL;
err_crop:
    free(encoder->cropRegion);
    encoder->cropRegion = NULL;
    return -1;
}

VSL_API
void
vsl_encoder_release(VSLEncoder* enc)
{
    if (!enc) { return; }

    if (enc->phyMem.nPhyAddr) { VPU_EncFreeMem(&enc->phyMem); }

    if (enc->handle) { VPU_EncClose(enc->handle); }

    if (enc->virtMem) { free(enc->virtMem); }

    if (enc->cropRegion) { free(enc->cropRegion); }

    free(enc);
}

VSL_API
void
vsl_encoder_frame_cleanup(VSLFrame* frame)
{
#ifndef NDEBUG
    fprintf(stdout, "%s: %p\n", __FUNCTION__, frame);
#endif

    if (!frame) { return; }
    if (!frame->userptr) { return; }
    VpuMemDesc* memDesc = frame->userptr;
    // update internal map pointer to indicate if memory was already unmapped to
    // avoid double unmap
    memDesc->nVirtAddr = (unsigned long) frame->map;
    VPU_EncFreeMem(memDesc);
    free(memDesc);
}

VSL_API
int
vsl_encode_frame(VSLEncoder*    encoder,
                 VSLFrame*      source,
                 VSLFrame*      destination,
                 const VSLRect* cropRegion,
                 int*           keyframe)
{
    if (!encoder || !source || !destination) {
        errno = EINVAL;
        return -1;
    }

    // Delayed initialization
    // Due to nature of vc8000e encoder configuration schema
    if (!encoder->handle) // encoder not initialized
    {
        if (-1 == vsl_encoder_init(encoder,
                                   source->info.fourcc,
                                   source->info.width,
                                   source->info.height,
                                   cropRegion)) {
            return -1;
        }
    } else if (cropRegion && (cropRegion->width != encoder->cropRegion->width ||
               cropRegion->height != encoder->cropRegion->height ||
               cropRegion->x != encoder->cropRegion->x ||
               cropRegion->y != encoder->cropRegion->y)) {
        fprintf(stderr,
                "Changing crop region is not supported for Hantro VC8000e "
                "encoder!\n");
        errno = EINVAL;
        return -1;
    }

    // check source fourcc matches with encoder configuration
    if (source->info.fourcc != encoder->inputFourcc) {
        fprintf(stderr,
                "Changing input frame color format is not supported for Hantro "
                "VC8000e encoder!\n");
        errno = EINVAL;
        return -1;
    }

    // check destination fourcc matches with encoder configuration
    if (destination->info.fourcc != encoder->outputFourcc) {
        fprintf(stderr,
                "Changing output frame codec is not supported for Hantro "
                "VC8000e encoder!\n");
        errno = EINVAL;
        return -1;
    }

    // success of vsl_frame_mmap and vsl_frame_paddr ensures that source is
    // dbabuf backed memory
    if (!vsl_frame_mmap(source, NULL)) {

        fprintf(stderr, "%s: frame mmap failed\n", __FUNCTION__);
        return -1;
    }

    if (-1 == vsl_frame_paddr(source)) {
        fprintf(stderr, "%s: frame paddr failed\n", __FUNCTION__);
        return -1;
    }

    // Use memset to avoid aarch64 stack issues with designated initializers
    VpuEncEncParam sEncEncParam;
    memset(&sEncEncParam, 0, sizeof(sEncEncParam));
    sEncEncParam.nPicWidth   = encoder->outWidth;
    sEncEncParam.nPicHeight  = encoder->outHeight;
    sEncEncParam.nFrameRate  = encoder->fps;
    sEncEncParam.nQuantParam = 35;
    sEncEncParam.nInPhyInput = source->info.paddr + source->info.offset;
    sEncEncParam.nInVirtInput =
        (unsigned long) source->map + source->info.offset;
    sEncEncParam.nInInputSize = (int) source->info.size;

#ifndef NDEBUG
    printf("src map: %p\n", source->map);
    printf("src paddr: %lx\n", source->info.paddr);
    printf("src size: %ld\n", source->info.size);
    printf("src offset: %ld\n", source->info.offset);
#endif

    sEncEncParam.nInPhyOutput    = destination->info.paddr;
    sEncEncParam.nInVirtOutput   = (long unsigned int) destination->map;
    sEncEncParam.nInOutputBufLen = (unsigned int) destination->mapsize;

    VpuEncRetCode ret = VPU_EncEncodeFrame(encoder->handle, &sEncEncParam);
    if (ret != VPU_ENC_RET_SUCCESS) {
        fprintf(stderr,
                "%s: VPU_EncEncodeFrame failed: %d\n",
                __FUNCTION__,
                ret);
        return -1;
    }
    if (keyframe) {
        *keyframe =
            (sEncEncParam.eOutRetCode & VPU_ENC_OUTPUT_KEYFRAME) ? 1 : 0;
    }

    if ((sEncEncParam.eOutRetCode & VPU_ENC_OUTPUT_DIS) ||
        (sEncEncParam.eOutRetCode & VPU_ENC_OUTPUT_SEQHEADER)) {
        destination->info.size = sEncEncParam.nOutOutputSize;
        return 0;
    }

    return 0;
}

// Forward declaration for DMA heap-based allocation
VSLFrame*
vsl_encoder_new_output_frame_dmabuf(const VSLEncoder* encoder,
                                    int               width,
                                    int               height,
                                    int64_t           duration,
                                    int64_t           pts,
                                    int64_t           dts);

VSL_API
VSLFrame*
vsl_encoder_new_output_frame(const VSLEncoder* encoder,
                             int               width,
                             int               height,
                             int64_t           duration,
                             int64_t           pts,
                             int64_t           dts)
{
    // Use DMA heap allocation for cross-process frame sharing
    // This provides a valid dmabuf FD that can be sent via SCM_RIGHTS
    VSLFrame* frame = vsl_encoder_new_output_frame_dmabuf(encoder,
                                                          width,
                                                          height,
                                                          duration,
                                                          pts,
                                                          dts);

    if (!frame) {
        fprintf(stderr,
                "%s: DMA heap allocation failed, falling back to "
                "VPU_EncGetMem\n",
                __FUNCTION__);

        // Fallback to legacy VPU_EncGetMem (won't work for cross-process)
        VpuMemDesc* memDesc = calloc(1, sizeof(VpuMemDesc));
        if (!memDesc) {
            fprintf(stderr,
                    "%s: memDesc allocation failed: %d\n",
                    __FUNCTION__,
                    errno);
            return NULL;
        }

        memDesc->nSize = 1024 * 1024;

        VpuEncRetCode ret = VPU_EncGetMem(memDesc);
        if (VPU_ENC_RET_SUCCESS != ret) {
            fprintf(stderr,
                    "%s: VPU_EncGetMem failed: %d\n",
                    __FUNCTION__,
                    ret);
            free(memDesc);
            return NULL;
        }

        frame = vsl_frame_init(width,
                               height,
                               -1, // prevent form calculating stride,
                                   // it's not relevant for encoded frame
                               encoder->outputFourcc,
                               memDesc, // memDesc as userptr, used to
                                        // free the EWL memory
                               vsl_encoder_frame_cleanup);
        if (!frame) {
            fprintf(stderr, "%s: vsl_frame_init failed\n", __FUNCTION__);
            VPU_EncFreeMem(memDesc);
            free(memDesc);
            return NULL;
        }

        frame->map           = (void*) memDesc->nVirtAddr;
        frame->mapsize       = memDesc->nSize;
        frame->info.paddr    = memDesc->nPhyAddr;
        frame->info.duration = duration;
        frame->info.dts      = dts;
        frame->info.pts      = pts;

        fprintf(stderr,
                "%s: WARNING: frame allocated with VPU_EncGetMem (handle=-1), "
                "cannot be shared across processes\n",
                __FUNCTION__);
    }

    return frame;
}
