// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "decoder_hantro.h"
#include "common.h"
#include "frame.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int
vsl_decoder_init_hantro(struct vsl_decoder_hantro* decoder,
                        VSLDecoderCodec            inputCodec)
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
    if (!sMemInfo->MemSubBlock[0].pVirtAddr) {
        fprintf(stderr,
                "%s: failed to allocate virtual memory block: %s\n",
                __FUNCTION__,
                strerror(errno));
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }
    decoder->virt_mem = sMemInfo->MemSubBlock[0].pVirtAddr;

    decoder->phy_mem.nSize = sMemInfo->MemSubBlock[1].nSize;

    ret = VPU_DecGetMem(&decoder->phy_mem);
    if (ret != VPU_DEC_RET_SUCCESS) {
        fprintf(stderr, "%s: VPU_DecGetMem failed: %d\n", __FUNCTION__, ret);
        free(decoder->virt_mem);
        decoder->virt_mem = NULL;
        free(sMemInfo);
        free(sDecOpenParam);
        return -1;
    }

    sMemInfo->MemSubBlock[1].pVirtAddr =
        (unsigned char*) decoder->phy_mem.nVirtAddr;
    sMemInfo->MemSubBlock[1].pPhyAddr =
        (unsigned char*) decoder->phy_mem.nPhyAddr;

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
        VPU_DecFreeMem(&decoder->phy_mem);
        fprintf(stderr, "%s: VPU_DecOpen failed: %d\n", __FUNCTION__, ret);
        free(decoder->virt_mem);
        decoder->virt_mem = NULL;
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

    // Use KICK mode for non-blocking operation. NORMAL mode has a ~200ms
    // timeout which is too slow for 30fps real-time decoding. KICK mode
    // returns immediately with whatever output is available.
    config_param = VPU_DEC_IN_KICK;
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

VSLDecoder*
vsl_decoder_create_hantro(uint32_t codec, int fps)
{
    // Convert fourcc to codec enum
    VSLDecoderCodec inputCodec;
    if (codec == VSL_FOURCC('H', '2', '6', '4')) {
        inputCodec = VSL_DEC_H264;
    } else if (codec == VSL_FOURCC('H', 'E', 'V', 'C')) {
        inputCodec = VSL_DEC_HEVC;
    } else {
        fprintf(stderr, "%s: unsupported codec: 0x%08x\n", __FUNCTION__, codec);
        errno = EINVAL;
        return NULL;
    }

    struct vsl_decoder_hantro* decoder =
        calloc(1, sizeof(struct vsl_decoder_hantro));
    if (!decoder) {
#ifndef NDEBUG
        fprintf(stderr,
                "%s: decoder struct allocation failed: %s\n",
                __FUNCTION__,
                strerror(errno));
#endif
        return NULL;
    }

    decoder->backend = VSL_CODEC_BACKEND_HANTRO;
    decoder->fps     = fps;

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
    if (vsl_decoder_init_hantro(decoder, inputCodec)) {
        free(decoder);
        return NULL;
    }
    return (VSLDecoder*) decoder;
}

static void
vsl_alloc_framebuf_hantro(struct vsl_decoder_hantro* decoder)
{
    VpuDecInitInfo initInfo;
    memset(&initInfo, 0, sizeof(initInfo));
    VpuDecRetCode ret = VPU_DecGetInitialInfo(decoder->handle, &initInfo);
    if (VPU_DEC_RET_SUCCESS != ret) {
        fprintf(stdout,
                "%s: VPU_DecGetInitialInfo failure: %x\n",
                __FUNCTION__,
                ret);
        return;
    }
    int bufNum = initInfo.nMinFrameBufferCount + 2;

    int yStride = (int) VSL_ALIGN(initInfo.nPicWidth, 16);
    int ySize   = 0;
    if (initInfo.nInterlace) {
        ySize = (int) (VSL_ALIGN(initInfo.nPicWidth, 16) *
                       VSL_ALIGN(initInfo.nPicHeight, (2 * 16)));
    } else {
        ySize = (int) (VSL_ALIGN(initInfo.nPicWidth, 16) *
                       VSL_ALIGN(initInfo.nPicHeight, 16));
    }
    // 4:2:0 for all video
    int uStride = yStride / 2;
    int uSize   = ySize / 4;
    int vSize   = uSize;
    int mvSize  = uSize;
#ifndef NDEBUG
    printf("vpu registering %i frame bufs\n", bufNum);
#endif

    VpuFrameBuffer* frameBuf = calloc(sizeof(VpuFrameBuffer), bufNum);

    // Allocate arrays to store dmabuf FDs and maps
    decoder->frame_buf_fds  = calloc(bufNum, sizeof(int));
    decoder->frame_buf_maps = calloc(bufNum, sizeof(void*));

    // Initialize FDs to -1 (0 would incorrectly indicate stdin)
    for (int i = 0; i < bufNum; i++) {
        decoder->frame_buf_fds[i] = -1;
    }

    // Try DMA heap allocation first for cross-process sharing
    int dmabuf_result =
        vsl_decoder_alloc_frame_buffers_dmabuf(bufNum,
                                               yStride,
                                               ySize,
                                               uSize,
                                               vSize,
                                               mvSize,
                                               frameBuf,
                                               decoder->frame_buf_fds,
                                               decoder->frame_buf_maps);

    if (dmabuf_result == 0) {
        // Success - store buffer info for cleanup
        decoder->frame_buf_count  = bufNum;
        decoder->frame_buf_y_size  = ySize;
        decoder->frame_buf_u_size  = uSize;
        decoder->frame_buf_v_size  = vSize;
        decoder->frame_buf_mv_size = mvSize;

#ifndef NDEBUG
        printf("%s: allocated %d frame buffers via DMA heap\n",
               __FUNCTION__,
               bufNum);
#endif
    } else {
        // Fallback to legacy VPU_DecGetMem (won't work for cross-process)
        fprintf(stderr,
                "%s: DMA heap allocation failed, falling back to "
                "VPU_DecGetMem\n",
                __FUNCTION__);

        free(decoder->frame_buf_fds);
        free(decoder->frame_buf_maps);
        decoder->frame_buf_fds   = NULL;
        decoder->frame_buf_maps  = NULL;
        decoder->frame_buf_count = 0;

        VpuMemDesc* vpuMem = calloc(sizeof(VpuMemDesc), 1);
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
        free(vpuMem);

        fprintf(stderr,
                "%s: WARNING: frame buffers allocated with VPU_DecGetMem, "
                "cannot be shared across processes\n",
                __FUNCTION__);
    }

    ret = VPU_DecRegisterFrameBuffer(decoder->handle, frameBuf, bufNum);
    if (VPU_DEC_RET_SUCCESS != ret) {
        printf("%s: vpu register frame failure: ret=%d\n", __FUNCTION__, ret);
        if (decoder->frame_buf_fds) {
            vsl_decoder_free_frame_buffers_dmabuf(decoder->frame_buf_count,
                                                  decoder->frame_buf_y_size,
                                                  decoder->frame_buf_u_size,
                                                  decoder->frame_buf_v_size,
                                                  decoder->frame_buf_mv_size,
                                                  decoder->frame_buf_fds,
                                                  decoder->frame_buf_maps);
            free(decoder->frame_buf_fds);
            free(decoder->frame_buf_maps);
            decoder->frame_buf_fds  = NULL;
            decoder->frame_buf_maps = NULL;
        }
        free(frameBuf);
        return;
    }
    free(frameBuf);
#ifndef NDEBUG
    printf("vpu registered frame bufs\n");
#endif
}

struct hantro_frame_cleanup_data {
    struct vsl_decoder_hantro* decoder;
    VpuDecOutFrameInfo         frame_info;
};

static void
hantro_frame_cleanup(VSLFrame* frame)
{
    if (!frame) { return; }
    if (!frame->userptr) { return; }
    struct hantro_frame_cleanup_data* _data =
        (struct hantro_frame_cleanup_data*) frame->userptr;
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
vsl_decode_frame_hantro(VSLDecoder*  decoder_,
                        const void*  data,
                        unsigned int data_length,
                        size_t*      bytes_used,
                        VSLFrame**   output_frame)
{
    struct vsl_decoder_hantro* decoder = (struct vsl_decoder_hantro*) decoder_;
    VpuDecRetCode              ret;
    VpuDecOutFrameInfo         frameInfo;
    VpuDecFrameLengthInfo      decFrmLengthInfo;
    VpuBufferNode              inData;
    int                        totalDecConsumedBytes = 0; // stuffer + frame

    // Timing instrumentation
    int64_t t_start     = vsl_timestamp_us();
    int64_t t_decode1   = 0;
    int64_t t_decode2   = 0;
    int64_t t_getinfo   = 0;
    int64_t t_getoutput = 0;
    decoder->frame_num++;

    // Initialize all stack-allocated VPU structures to avoid aarch64 stack
    // issues and undefined behavior from uninitialized memory
    memset(&frameInfo, 0, sizeof(frameInfo));
    memset(&decFrmLengthInfo, 0, sizeof(decFrmLengthInfo));
    memset(&inData, 0, sizeof(inData));
    inData.pVirAddr         = (unsigned char*) (uintptr_t) data;
    inData.nSize            = data_length;
    inData.pPhyAddr         = NULL;
    inData.sCodecData.pData = NULL;
    inData.sCodecData.nSize = 0;

    int ret_code = 0;
    int vsl_ret  = 0;

    // First VPU_DecDecodeBuf call
    int64_t t0 = vsl_timestamp_us();
    VPU_DecDecodeBuf(decoder->handle, &inData, &ret_code);
    t_decode1 = vsl_timestamp_us() - t0;

    // If consumed but no output, poll again with empty buffer to check if
    // output is ready (common with B-frames which need future reference frames)
    int consumed = (ret_code & VPU_DEC_ONE_FRM_CONSUMED) ? 1 : 0;
    int output   = (ret_code & VPU_DEC_OUTPUT_DIS) ? 1 : 0;
    if (consumed && !output) {
        // Use stack allocation for secondary decode poll (B-frame output check)
        // This avoids malloc/free overhead in the hot path
        VpuBufferNode emptyData;
        memset(&emptyData, 0, sizeof(emptyData));
        int kick_ret = 0;
        t0           = vsl_timestamp_us();
        VPU_DecDecodeBuf(decoder->handle, &emptyData, &kick_ret);
        t_decode2 = vsl_timestamp_us() - t0;

        // Merge result with original
        if (kick_ret & VPU_DEC_OUTPUT_DIS) { ret_code |= VPU_DEC_OUTPUT_DIS; }
    }

    // Always print timing for first 10 frames, then every 30th frame
    if (decoder->frame_num <= 10 || decoder->frame_num % 30 == 0 ||
        t_decode1 > 10000) {
        fprintf(stderr,
                "[DECODE-TIMING] frame=%d decode1=%lldus decode2=%lldus "
                "ret=0x%x consumed=%d output=%d\n",
                decoder->frame_num,
                (long long) t_decode1,
                (long long) t_decode2,
                ret_code,
                consumed,
                output);
    }

#ifndef NDEBUG
    printf("%s: VPU_DecDecodeBuf ret code: %x\n", __FUNCTION__, ret_code);
#endif

    if (ret_code & VPU_DEC_RESOLUTION_CHANGED) {
        vsl_alloc_framebuf_hantro(decoder);
    }

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
        decoder->out_height = initInfo->nPicHeight;
        decoder->out_width  = initInfo->nPicWidth;

        decoder->crop_region.x      = initInfo->PicCropRect.nLeft;
        decoder->crop_region.y      = initInfo->PicCropRect.nTop;
        decoder->crop_region.width  = initInfo->PicCropRect.nRight;
        decoder->crop_region.height = initInfo->PicCropRect.nBottom;
        decoder->output_fourcc      = VSL_FOURCC('N', 'V', '1', '2');
        vsl_alloc_framebuf_hantro(decoder);
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

        struct hantro_frame_cleanup_data* frame_clean_data =
            calloc(sizeof(struct hantro_frame_cleanup_data), 1);
        frame_clean_data->decoder    = decoder;
        frame_clean_data->frame_info = frameInfo;
        VSLFrame* out                = vsl_frame_init(decoder->out_width,
                                       decoder->out_height,
                                       0,
                                       decoder->output_fourcc,
                                       frame_clean_data,
                                       hantro_frame_cleanup);
        out->handle                  = frameInfo.pDisplayFrameBuf->nIonFd;
        out->info.height             = decoder->out_height;
        out->info.width              = decoder->out_width;
        out->info.paddr = (intptr_t) frameInfo.pDisplayFrameBuf->pbufY;

        // 4:2:0 for all video
        int ySize = decoder->out_height * decoder->out_width;
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

int
vsl_decoder_release_hantro(VSLDecoder* decoder_)
{
    struct vsl_decoder_hantro* decoder = (struct vsl_decoder_hantro*) decoder_;
    VpuDecRetCode              vpuRet;
    int                        ret = 0;

    vpuRet = VPU_DecClose(decoder->handle);
    if (vpuRet != VPU_DEC_RET_SUCCESS) {
        printf("%s: vpu decoder close failure : ret=%d \r\n",
               __FUNCTION__,
               vpuRet);
        ret = 1;
    }

    // Free DMA heap frame buffers if allocated
    if (decoder->frame_buf_fds) {
        vsl_decoder_free_frame_buffers_dmabuf(decoder->frame_buf_count,
                                              decoder->frame_buf_y_size,
                                              decoder->frame_buf_u_size,
                                              decoder->frame_buf_v_size,
                                              decoder->frame_buf_mv_size,
                                              decoder->frame_buf_fds,
                                              decoder->frame_buf_maps);
        free(decoder->frame_buf_fds);
        free(decoder->frame_buf_maps);
    }

    free(decoder->virt_mem);

    vpuRet = VPU_DecFreeMem(&decoder->phy_mem);
    if (vpuRet != VPU_DEC_RET_SUCCESS) {
        printf("%s: free vpu memory failure : ret=%d \r\n",
               __FUNCTION__,
               vpuRet);
        ret = 1;
    }

    free(decoder);

    return ret;
}

// Accessor functions for Hantro decoder
int
vsl_decoder_width_hantro(const VSLDecoder* decoder_)
{
    const struct vsl_decoder_hantro* decoder =
        (const struct vsl_decoder_hantro*) decoder_;
    return decoder->out_width;
}

int
vsl_decoder_height_hantro(const VSLDecoder* decoder_)
{
    const struct vsl_decoder_hantro* decoder =
        (const struct vsl_decoder_hantro*) decoder_;
    return decoder->out_height;
}

VSLRect
vsl_decoder_crop_hantro(const VSLDecoder* decoder_)
{
    const struct vsl_decoder_hantro* decoder =
        (const struct vsl_decoder_hantro*) decoder_;
    return decoder->crop_region;
}