// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "decoder_hantro.h"
#include "frame.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define Align(ptr, align) \
    ((int) (((unsigned long) ptr + (align) - 1) / (align) * (align)))

static int
vsl_decoder_init(VSLDecoder* decoder, VSLDecoderCodec inputCodec)
{
    VpuDecRetCode    ret;
    VpuMemInfo*      sMemInfo      = calloc(sizeof(VpuMemInfo), 1);
    VpuDecOpenParam* sDecOpenParam = calloc(sizeof(VpuDecOpenParam), 1);

    ret = VPU_DecQueryMem(sMemInfo);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_DecQueryMem failed: %d\n", __FUNCTION__, ret);
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }

    // we know what to expect, one phy (idx 1) and one virt (idx 0) block, check
    // it in case something changed
    if (sMemInfo->nSubBlockNum != 2 ||
        sMemInfo->MemSubBlock[0].MemType != VPU_MEM_VIRT ||
        sMemInfo->MemSubBlock[1].MemType != VPU_MEM_PHY) {
        fprintf(stderr,
                "%s: VPU_DecQueryMem returned unexpected memory block "
                "layout.\n",
                __FUNCTION__);
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }

    sMemInfo->MemSubBlock[0].pVirtAddr =
        calloc(1, sMemInfo->MemSubBlock[0].nSize);
    decoder->virtMem = sMemInfo->MemSubBlock[0].pVirtAddr;

    decoder->phyMem.nSize = sMemInfo->MemSubBlock[1].nSize;

    ret = VPU_DecGetMem(&decoder->phyMem);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_DecGetMem failed: %d\n", __FUNCTION__, ret);
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }

    sMemInfo->MemSubBlock[1].pVirtAddr =
        (unsigned char*) decoder->phyMem.nVirtAddr;
    sMemInfo->MemSubBlock[1].pPhyAddr =
        (unsigned char*) decoder->phyMem.nPhyAddr;

    switch (inputCodec) {
    case VSL_DEC_H264:
        sDecOpenParam->CodecFormat    = VPU_V_AVC;
        sDecOpenParam->nReorderEnable = 1;
        break;
    case VSL_DEC_HEVC:
        sDecOpenParam->CodecFormat = VPU_V_HEVC;
        break;
    default:
        fprintf(stderr,
                "%s: Invalid Decoder format: %i\n",
                __FUNCTION__,
                inputCodec);
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }

    ret = VPU_DecOpen(&decoder->handle, sDecOpenParam, sMemInfo);
    if (ret != VPU_DEC_RET_SUCCESS) {
        VPU_DecFreeMem(&decoder->phyMem);
        fprintf(stderr, "%s: VPU_DecOpen failed: %d\n", __FUNCTION__, ret);
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }

    int config_param = VPU_DEC_SKIPNONE;
    ret = VPU_DecConfig(&decoder->handle, VPU_DEC_CONF_SKIPMODE, &config_param);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr,
                "%s: VPU_DecConfig SKIPMODE failed: %d\n",
                __FUNCTION__,
                ret);
    }

    config_param = 0;
    ret = VPU_DecConfig(&decoder->handle, VPU_DEC_CONF_BUFDELAY, &config_param);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr,
                "%s: VPU_DecConfig BUFDELAY failed: %d\n",
                __FUNCTION__,
                ret);
    }

    config_param = VPU_DEC_IN_NORMAL;
    ret =
        VPU_DecConfig(&decoder->handle, VPU_DEC_CONF_INPUTTYPE, &config_param);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr,
                "%s: VPU_DecConfig INPUTTYPE failed: %d\n",
                __FUNCTION__,
                ret);
    }
    free(sMemInfo);
    free(sDecOpenParam);
    return 0;
}

VSL_API
VSLDecoder*
vsl_decoder_create(VSLDecoderCodec inputCodec, int fps)
{
    VSLDecoder* decoder = (VSLDecoder*) calloc(1, sizeof(VSLDecoder));
    if (!decoder) {
#ifndef DEBUG
        fprintf(stderr,
                "%s: decoder struct allocation failed: %s\n",
                __FUNCTION__,
                strerror(errno));
#endif
        return NULL;
    }

    decoder->fps = fps;

    VpuDecRetCode  ret;
    VpuVersionInfo ver;

    ret = VPU_DecLoad();
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_DecLoad failed: %d\n", __FUNCTION__, ret);
        free(decoder);
        return NULL;
    }

    ret = VPU_DecGetVersionInfo(&ver);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr,
                "%s: VPU_DecGetVersionInfo failed: %d\n",
                __FUNCTION__,
                ret);
        free(decoder);
        return NULL;
    }
#ifndef NDEBUG
    fprintf(stdout,
            "vpu dec lib version: %d.%d.%d\n",
            ver.nLibMajor,
            ver.nLibMinor,
            ver.nLibRelease);
    fprintf(stdout,
            "vpu dec fw version: %d.%d.%d_r%d\n",
            ver.nFwMajor,
            ver.nFwMinor,
            ver.nFwRelease,
            ver.nFwCode);
#endif

    // do not print vpu_wrapper version, no need since it's internal anyway
    if (vsl_decoder_init(decoder, inputCodec)) {
        free(decoder);
        return NULL;
    }
    return decoder;
}

static void
vsl_alloc_framebuf(VSLDecoder* decoder)
{
    VpuDecInitInfo initInfo = {};
    VpuDecRetCode  ret      = VPU_DecGetInitialInfo(decoder->handle, &initInfo);
    if (VPU_DEC_RET_SUCCESS != ret) {
        fprintf(stdout,
                "%s: VPU_DecGetInitialInfo failure: %x\n",
                __FUNCTION__,
                ret);
        return;
    }
    int bufNum = initInfo.nMinFrameBufferCount + 2;

    int yStride = Align(initInfo.nPicWidth, 16);
    int ySize   = 0;
    if (initInfo.nInterlace) {
        ySize = Align(initInfo.nPicWidth, 16) *
                Align(initInfo.nPicHeight, (2 * 16));
    } else {
        ySize = Align(initInfo.nPicWidth, 16) * Align(initInfo.nPicHeight, 16);
    }
    // 4:2:0 for all video
    int uStride = yStride / 2;
    int uSize   = ySize / 4;
    int vSize   = uSize;
    int mvSize  = uSize;
#ifndef NDEBUG
    printf("vpu registering %i frame bufs\n", bufNum);
#endif
    VpuMemDesc*     vpuMem   = calloc(sizeof(VpuMemDesc), 1);
    VpuFrameBuffer* frameBuf = calloc(sizeof(VpuFrameBuffer), bufNum);
    for (int i = 0; i < bufNum; i++) {
        int totalSize = (ySize + uSize + vSize + mvSize) * 1;
        vpuMem->nSize = totalSize;
        ret           = VPU_DecGetMem(vpuMem);
        if (VPU_DEC_RET_SUCCESS != ret) {
            printf("%s: vpu malloc frame buf failure: ret=%d\n",
                   __FUNCTION__,
                   ret);
            free(vpuMem);
            free(frameBuf);
            return;
        }
        // fill frameBuf
        unsigned char* ptr     = (unsigned char*) vpuMem->nPhyAddr;
        unsigned char* ptrVirt = (unsigned char*) vpuMem->nVirtAddr;

        /* fill stride info */
        frameBuf[i].nStrideY = yStride;
        frameBuf[i].nStrideC = uStride;

        /* fill phy addr*/
        frameBuf[i].pbufY     = ptr;
        frameBuf[i].pbufCb    = ptr + ySize;
        frameBuf[i].pbufCr    = ptr + ySize + uSize;
        frameBuf[i].pbufMvCol = ptr + ySize + uSize + vSize;

        /* fill virt addr */
        frameBuf[i].pbufVirtY     = ptrVirt;
        frameBuf[i].pbufVirtCb    = ptrVirt + ySize;
        frameBuf[i].pbufVirtCr    = ptrVirt + ySize + uSize;
        frameBuf[i].pbufVirtMvCol = ptrVirt + ySize + uSize + vSize;

#ifdef ILLEGAL_MEMORY_DEBUG
        memset(frameBuf[i].pbufVirtY, 0, ySize);
        memset(frameBuf[i].pbufVirtCb, 0, uSize);
        memset(frameBuf[i].pbufVirtCr, 0, uSize);
#endif

        frameBuf[i].pbufY_tilebot      = 0;
        frameBuf[i].pbufCb_tilebot     = 0;
        frameBuf[i].pbufVirtY_tilebot  = 0;
        frameBuf[i].pbufVirtCb_tilebot = 0;
    }
    ret = VPU_DecRegisterFrameBuffer(decoder->handle, frameBuf, bufNum);
    if (VPU_DEC_RET_SUCCESS != ret) {
        printf("%s: vpu register frame failure: ret=%d\n", __FUNCTION__, ret);
        free(vpuMem);
        free(frameBuf);
        return;
    }
    free(vpuMem);
    free(frameBuf);
#ifndef NDEBUG
    printf("vpu registered frame bufs\n");
#endif
}

struct frame_cleanup_data {
    VSLDecoder*        decoder;
    VpuDecOutFrameInfo frame_info;
};

void
frame_cleanup(VSLFrame* frame)
{
    if (!frame) { return; }
    if (!frame->userptr) { return; }
    struct frame_cleanup_data* _data =
        (struct frame_cleanup_data*) frame->userptr;
    VpuDecRetCode ret =
        VPU_DecOutFrameDisplayed(_data->decoder->handle,
                                 _data->frame_info.pDisplayFrameBuf);
    free(frame->userptr);
    if (VPU_DEC_RET_SUCCESS != ret) {
        printf("%s: vpu dec frame displayed failure: ret=%d\n",
               __FUNCTION__,
               ret);
    }
}

VSLDecoderRetCode
vsl_decode_frame(VSLDecoder*  decoder,
                 const void*  data,
                 unsigned int data_length,
                 size_t*      bytes_used,
                 VSLFrame**   output_frame)
{
    VpuDecRetCode         ret;
    VpuDecOutFrameInfo    frameInfo;
    VpuDecFrameLengthInfo decFrmLengthInfo;
    int                   totalDecConsumedBytes = 0; // stuffer + frame

    VpuBufferNode inData    = {};
    inData.pVirAddr         = (unsigned char*) data;
    inData.nSize            = data_length;
    inData.pPhyAddr         = NULL;
    inData.sCodecData.pData = NULL;
    inData.sCodecData.nSize = 0;

    int ret_code = 0;
    int vsl_ret  = 0;
    VPU_DecDecodeBuf(decoder->handle, &inData, &ret_code);
#ifndef NDEBUG
    printf("%s: VPU_DecDecodeBuf ret code: %x\n", __FUNCTION__, ret_code);
#endif

    if (ret_code & VPU_DEC_RESOLUTION_CHANGED) { vsl_alloc_framebuf(decoder); }

    // check init info
    if (ret_code & VPU_DEC_INIT_OK) {
        // process init info
        VpuDecInitInfo* initInfo = calloc(sizeof(VpuDecInitInfo), 1);
        ret = VPU_DecGetInitialInfo(decoder->handle, initInfo);
        if (VPU_DEC_RET_SUCCESS != ret) {
            fprintf(stdout,
                    "%s: VPU_DecGetInitialInfo failure: %x\n",
                    __FUNCTION__,
                    ret);
            free(initInfo);
            return VSL_DEC_ERR;
        }
#ifndef NDEBUG
        printf("Video is %ix%i %i/%i FPS\n",
               initInfo->nPicWidth,
               initInfo->nPicHeight,
               initInfo->nFrameRateRes,
               initInfo->nFrameRateDiv);
#endif
        decoder->outHeight = initInfo->nPicHeight;
        decoder->outWidth  = initInfo->nPicWidth;

        decoder->cropRegion.x      = initInfo->PicCropRect.nLeft;
        decoder->cropRegion.y      = initInfo->PicCropRect.nTop;
        decoder->cropRegion.width  = initInfo->PicCropRect.nRight;
        decoder->cropRegion.height = initInfo->PicCropRect.nBottom;
        decoder->outputFourcc      = VSL_FOURCC('N', 'V', '1', '2');
        vsl_alloc_framebuf(decoder);
        vsl_ret |= VSL_DEC_INIT_INFO;
        free(initInfo);
    }

    if (ret_code & VPU_DEC_ONE_FRM_CONSUMED ||
        ret_code & VPU_DEC_NO_ENOUGH_INBUF) {
        ret = VPU_DecGetConsumedFrameInfo(decoder->handle, &decFrmLengthInfo);
        if (VPU_DEC_RET_SUCCESS != ret) {
            printf("%s: vpu get consumed frame info failure: ret=%d\n",
                   __FUNCTION__,
                   ret);
            return VSL_DEC_ERR;
        }
        totalDecConsumedBytes +=
            decFrmLengthInfo.nFrameLength + decFrmLengthInfo.nStuffLength;
#ifndef NDEBUG
        printf("[total:0x%X]:one frame is consumed: 0x%X, consumed total size: "
               "%d (stuff size: %d, frame size: %d)\n",
               totalDecConsumedBytes,
               (unsigned int) decFrmLengthInfo.pFrame,
               decFrmLengthInfo.nStuffLength + decFrmLengthInfo.nFrameLength,
               decFrmLengthInfo.nStuffLength,
               decFrmLengthInfo.nFrameLength);
#endif
        *bytes_used = totalDecConsumedBytes;
        vsl_ret |= VSL_DEC_FRAME_DEC;
    }

    if (ret_code & VPU_DEC_OUTPUT_DIS) {
        ret = VPU_DecGetOutputFrame(decoder->handle, &frameInfo);
        if (VPU_DEC_RET_SUCCESS != ret) {
            printf("%s: vpu get output frame failure: ret=%d\n",
                   __FUNCTION__,
                   ret);
            return VSL_DEC_ERR;
        }
#ifndef NDEBUG
        printf("bufID: %i, pbufY: %p, pbufCb: %p, pbufCr: %p, ionFd: %i\n",
               frameInfo.pDisplayFrameBuf->nBufferId,
               frameInfo.pDisplayFrameBuf->pbufY,
               frameInfo.pDisplayFrameBuf->pbufCb,
               frameInfo.pDisplayFrameBuf->pbufCr,
               frameInfo.pDisplayFrameBuf->nIonFd);
#endif

        struct frame_cleanup_data* frame_clean_data =
            calloc(sizeof(struct frame_cleanup_data), 1);
        frame_clean_data->decoder    = decoder;
        frame_clean_data->frame_info = frameInfo;
        VSLFrame* out                = vsl_frame_init(decoder->outWidth,
                                       decoder->outHeight,
                                       0,
                                       decoder->outputFourcc,
                                       frame_clean_data,
                                       frame_cleanup);
        out->handle                  = frameInfo.pDisplayFrameBuf->nIonFd;
        out->info.height             = decoder->outHeight;
        out->info.width              = decoder->outWidth;
        out->info.paddr = (intptr_t) frameInfo.pDisplayFrameBuf->pbufY;

        // 4:2:0 for all video
        int ySize = decoder->outHeight * decoder->outWidth;
        int uSize = ySize / 4;
        int vSize = uSize;

        out->info.size  = ySize + uSize + vSize;
        (*output_frame) = out;

        // covert output frame from GL_VIV_YV12 triplanar format to RBGA with
        // g2d
        // https://www.nxp.com/docs/en/user-guide/i.MX_AA_Graphics_User's_Guide.pdf
        // G2D 2.5.1 Color space conversion from YUV to RGB
    }
    return vsl_ret;
}

VSL_API
int
vsl_decoder_release(VSLDecoder* decoder)
{
    VpuDecRetCode vpuRet;
    int           ret = 0;

    vpuRet = VPU_DecClose(decoder->handle);
    if (vpuRet != VPU_DEC_RET_SUCCESS) {
        printf("%s: vpu decoder close failure : ret=%d \r\n",
               __FUNCTION__,
               vpuRet);
        ret = 1;
    }

    free(decoder->virtMem);

    vpuRet = VPU_DecFreeMem(&decoder->phyMem);
    if (vpuRet != VPU_DEC_RET_SUCCESS) {
        printf("%s: free vpu memory failure : ret=%d \r\n",
               __FUNCTION__,
               vpuRet);
        ret = 1;
    }

    free(decoder);

    return ret;
}

VSL_API
int
vsl_decoder_width(const VSLDecoder* decoder)
{
    return decoder->outWidth;
}

VSL_API
int
vsl_decoder_height(const VSLDecoder* decoder)
{
    return decoder->outHeight;
}

VSL_API
VSLRect
vsl_decoder_crop(const VSLDecoder* decoder)
{
    return decoder->cropRegion;
}