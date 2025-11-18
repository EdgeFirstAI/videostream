#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>

#include "codec_dlopen.h"

#define LIB_NAME "libcodec.so.1"
#define ENV_NAME "LIBCODEC_LOCATION"

#define LIB_CALL_CHECK(ptr)                                                 \
    do                                                                      \
    {                                                                       \
        if (!ptr) {                                                         \
            fprintf(stderr, "Error calling %s: %s\n", __func__, dlerror()); \
            assert(false); }                                                \
    } while (0)

static void *handle = NULL;

int
Codec_OpenLib()
{
    if (handle == NULL) {
        const char* lib_path = NULL;

        lib_path = getenv(ENV_NAME);
        if (lib_path == NULL) {
            // Environmental variable is not set, search for the library in
            // typical locations
            handle = dlopen(LIB_NAME, RTLD_LAZY);
        } else {
            // Environmental variable is set, try to load the library from the
            // specified path
            handle = dlopen(lib_path, RTLD_LAZY);
        }

        if (handle == NULL) {
            // Library could not be loaded
            fprintf(stderr, "Error while opening library %s\n", LIB_NAME);

            if (lib_path == NULL) {
                // Environmental variable is not set and library not found in
                // typical locations
                errno = ENOENT;
                return -1;
            } else {
                // Environmental variable is set but library not found at
                // specified path
                errno = EINVAL;
                return -1;
            }
        }

        // Success
        return 0;
    }

    // Already opened
    return 1;
}

int
Codec_CloseLib()
{
    if (handle != NULL)
    {
        dlclose(handle);
        // success
        return 0;
    }

    // already closed or not opened
    return 1;
}
CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_avs(const void*                   DWLInstance,
                                  OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (
        *HantroHwDecOmx_decoder_create_avs_func_t)(const void* DWLInstance,
                                                   OMX_VIDEO_PARAM_G1CONFIGTYPE*
                                                       g1Conf);
    HantroHwDecOmx_decoder_create_avs_func_t
        HantroHwDecOmx_decoder_create_avs_func =
            (HantroHwDecOmx_decoder_create_avs_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_avs");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_avs_func);
    return HantroHwDecOmx_decoder_create_avs_func(DWLInstance, g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_h264(const void*                   DWLInstance,
                                   OMX_BOOL                      mvc_stream,
                                   OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (*HantroHwDecOmx_decoder_create_h264_func_t)(
        const void*                   DWLInstance,
        OMX_BOOL                      mvc_stream,
        OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf);
    HantroHwDecOmx_decoder_create_h264_func_t
        HantroHwDecOmx_decoder_create_h264_func =
            (HantroHwDecOmx_decoder_create_h264_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_h264");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_h264_func);
    return HantroHwDecOmx_decoder_create_h264_func(DWLInstance,
                                                   mvc_stream,
                                                   g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_hevc(const void*                   DWLInstance,
                                   OMX_VIDEO_PARAM_G2CONFIGTYPE* g2Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (*HantroHwDecOmx_decoder_create_hevc_func_t)(
        const void* DWLInstance, OMX_VIDEO_PARAM_G2CONFIGTYPE* g2Conf);
    HantroHwDecOmx_decoder_create_hevc_func_t
        HantroHwDecOmx_decoder_create_hevc_func =
            (HantroHwDecOmx_decoder_create_hevc_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_hevc");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_hevc_func);
    return HantroHwDecOmx_decoder_create_hevc_func(DWLInstance, g2Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_jpeg(OMX_BOOL motion_jpeg)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (*HantroHwDecOmx_decoder_create_jpeg_func_t)(
        OMX_BOOL motion_jpeg);
    HantroHwDecOmx_decoder_create_jpeg_func_t
        HantroHwDecOmx_decoder_create_jpeg_func =
            (HantroHwDecOmx_decoder_create_jpeg_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_jpeg");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_jpeg_func);
    return HantroHwDecOmx_decoder_create_jpeg_func(motion_jpeg);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_mpeg2(const void*                   DWLInstance,
                                    OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (*HantroHwDecOmx_decoder_create_mpeg2_func_t)(
        const void* DWLInstance, OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf);
    HantroHwDecOmx_decoder_create_mpeg2_func_t
        HantroHwDecOmx_decoder_create_mpeg2_func =
            (HantroHwDecOmx_decoder_create_mpeg2_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_mpeg2");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_mpeg2_func);
    return HantroHwDecOmx_decoder_create_mpeg2_func(DWLInstance, g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_mpeg4(const void*  DWLInstance,
                                    OMX_BOOL     enable_deblocking,
                                    MPEG4_FORMAT format,
                                    OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (*HantroHwDecOmx_decoder_create_mpeg4_func_t)(
        const void*                   DWLInstance,
        OMX_BOOL                      enable_deblocking,
        MPEG4_FORMAT                  format,
        OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf);
    HantroHwDecOmx_decoder_create_mpeg4_func_t
        HantroHwDecOmx_decoder_create_mpeg4_func =
            (HantroHwDecOmx_decoder_create_mpeg4_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_mpeg4");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_mpeg4_func);
    return HantroHwDecOmx_decoder_create_mpeg4_func(DWLInstance,
                                                    enable_deblocking,
                                                    format,
                                                    g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_rv(const void* DWLInstance,
                                 OMX_BOOL    bIsRV8,
                                 OMX_U32     frame_code_length,
                                 OMX_U32*    frame_sizes,
                                 OMX_U32     maxWidth,
                                 OMX_U32     maxHeight,
                                 OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (
        *HantroHwDecOmx_decoder_create_rv_func_t)(const void* DWLInstance,
                                                  OMX_BOOL    bIsRV8,
                                                  OMX_U32     frame_code_length,
                                                  OMX_U32*    frame_sizes,
                                                  OMX_U32     maxWidth,
                                                  OMX_U32     maxHeight,
                                                  OMX_VIDEO_PARAM_G1CONFIGTYPE*
                                                      g1Conf);
    HantroHwDecOmx_decoder_create_rv_func_t
        HantroHwDecOmx_decoder_create_rv_func =
            (HantroHwDecOmx_decoder_create_rv_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_rv");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_rv_func);
    return HantroHwDecOmx_decoder_create_rv_func(DWLInstance,
                                                 bIsRV8,
                                                 frame_code_length,
                                                 frame_sizes,
                                                 maxWidth,
                                                 maxHeight,
                                                 g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_vc1(const void*                   DWLInstance,
                                  OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (
        *HantroHwDecOmx_decoder_create_vc1_func_t)(const void* DWLInstance,
                                                   OMX_VIDEO_PARAM_G1CONFIGTYPE*
                                                       g1Conf);
    HantroHwDecOmx_decoder_create_vc1_func_t
        HantroHwDecOmx_decoder_create_vc1_func =
            (HantroHwDecOmx_decoder_create_vc1_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_vc1");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_vc1_func);
    return HantroHwDecOmx_decoder_create_vc1_func(DWLInstance, g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_vp6(const void*                   DWLInstance,
                                  OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (
        *HantroHwDecOmx_decoder_create_vp6_func_t)(const void* DWLInstance,
                                                   OMX_VIDEO_PARAM_G1CONFIGTYPE*
                                                       g1Conf);
    HantroHwDecOmx_decoder_create_vp6_func_t
        HantroHwDecOmx_decoder_create_vp6_func =
            (HantroHwDecOmx_decoder_create_vp6_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_vp6");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_vp6_func);
    return HantroHwDecOmx_decoder_create_vp6_func(DWLInstance, g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_vp8(const void*                   DWLInstance,
                                  OMX_VIDEO_PARAM_G1CONFIGTYPE* g1Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (
        *HantroHwDecOmx_decoder_create_vp8_func_t)(const void* DWLInstance,
                                                   OMX_VIDEO_PARAM_G1CONFIGTYPE*
                                                       g1Conf);
    HantroHwDecOmx_decoder_create_vp8_func_t
        HantroHwDecOmx_decoder_create_vp8_func =
            (HantroHwDecOmx_decoder_create_vp8_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_vp8");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_vp8_func);
    return HantroHwDecOmx_decoder_create_vp8_func(DWLInstance, g1Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_vp9(const void*                   DWLInstance,
                                  OMX_VIDEO_PARAM_G2CONFIGTYPE* g2Conf)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (
        *HantroHwDecOmx_decoder_create_vp9_func_t)(const void* DWLInstance,
                                                   OMX_VIDEO_PARAM_G2CONFIGTYPE*
                                                       g2Conf);
    HantroHwDecOmx_decoder_create_vp9_func_t
        HantroHwDecOmx_decoder_create_vp9_func =
            (HantroHwDecOmx_decoder_create_vp9_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_vp9");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_vp9_func);
    return HantroHwDecOmx_decoder_create_vp9_func(DWLInstance, g2Conf);
}

CODEC_PROTOTYPE*
HantroHwDecOmx_decoder_create_webp(const void* DWLInstance)
{
    LIB_CALL_CHECK(handle);
    typedef CODEC_PROTOTYPE* (*HantroHwDecOmx_decoder_create_webp_func_t)(
        const void* DWLInstance);
    HantroHwDecOmx_decoder_create_webp_func_t
        HantroHwDecOmx_decoder_create_webp_func =
            (HantroHwDecOmx_decoder_create_webp_func_t)
                dlsym(handle, "HantroHwDecOmx_decoder_create_webp");
    LIB_CALL_CHECK(HantroHwDecOmx_decoder_create_webp_func);
    return HantroHwDecOmx_decoder_create_webp_func(DWLInstance);
}

OSAL_ERRORTYPE
OSAL_AllocatorAllocMem(OSAL_ALLOCATOR* alloc,
                       OSAL_U32*       size,
                       OSAL_U8**       bus_data,
                       OSAL_BUS_WIDTH* bus_address)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (
        *OSAL_AllocatorAllocMem_func_t)(OSAL_ALLOCATOR* alloc,
                                        OSAL_U32*       size,
                                        OSAL_U8**       bus_data,
                                        OSAL_BUS_WIDTH* bus_address);
    OSAL_AllocatorAllocMem_func_t OSAL_AllocatorAllocMem_func =
        (OSAL_AllocatorAllocMem_func_t) dlsym(handle, "OSAL_AllocatorAllocMem");
    LIB_CALL_CHECK(OSAL_AllocatorAllocMem_func);
    return OSAL_AllocatorAllocMem_func(alloc, size, bus_data, bus_address);
}

void
OSAL_AllocatorDestroy(OSAL_ALLOCATOR* alloc)
{
    LIB_CALL_CHECK(handle);
    typedef void (*OSAL_AllocatorDestroy_func_t)(OSAL_ALLOCATOR* alloc);
    OSAL_AllocatorDestroy_func_t OSAL_AllocatorDestroy_func =
        (OSAL_AllocatorDestroy_func_t) dlsym(handle, "OSAL_AllocatorDestroy");
    LIB_CALL_CHECK(OSAL_AllocatorDestroy_func);
    return OSAL_AllocatorDestroy_func(alloc);
}

void
OSAL_AllocatorFreeMem(OSAL_ALLOCATOR* alloc,
                      OSAL_U32        size,
                      OSAL_U8*        bus_data,
                      OSAL_BUS_WIDTH  bus_address)
{
    LIB_CALL_CHECK(handle);
    typedef void (*OSAL_AllocatorFreeMem_func_t)(OSAL_ALLOCATOR* alloc,
                                                 OSAL_U32        size,
                                                 OSAL_U8*        bus_data,
                                                 OSAL_BUS_WIDTH  bus_address);
    OSAL_AllocatorFreeMem_func_t OSAL_AllocatorFreeMem_func =
        (OSAL_AllocatorFreeMem_func_t) dlsym(handle, "OSAL_AllocatorFreeMem");
    LIB_CALL_CHECK(OSAL_AllocatorFreeMem_func);
    return OSAL_AllocatorFreeMem_func(alloc, size, bus_data, bus_address);
}

OSAL_ERRORTYPE
OSAL_AllocatorInit(OSAL_ALLOCATOR* alloc)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_AllocatorInit_func_t)(OSAL_ALLOCATOR* alloc);
    OSAL_AllocatorInit_func_t OSAL_AllocatorInit_func =
        (OSAL_AllocatorInit_func_t) dlsym(handle, "OSAL_AllocatorInit");
    LIB_CALL_CHECK(OSAL_AllocatorInit_func);
    return OSAL_AllocatorInit_func(alloc);
}

OSAL_BOOL
OSAL_AllocatorIsReady(const OSAL_ALLOCATOR* alloc)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_BOOL (*OSAL_AllocatorIsReady_func_t)(
        const OSAL_ALLOCATOR* alloc);
    OSAL_AllocatorIsReady_func_t OSAL_AllocatorIsReady_func =
        (OSAL_AllocatorIsReady_func_t) dlsym(handle, "OSAL_AllocatorIsReady");
    LIB_CALL_CHECK(OSAL_AllocatorIsReady_func);
    return OSAL_AllocatorIsReady_func(alloc);
}

OSAL_ERRORTYPE
OSAL_EventCreate(OSAL_PTR* phEvent)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_EventCreate_func_t)(OSAL_PTR* phEvent);
    OSAL_EventCreate_func_t OSAL_EventCreate_func =
        (OSAL_EventCreate_func_t) dlsym(handle, "OSAL_EventCreate");
    LIB_CALL_CHECK(OSAL_EventCreate_func);
    return OSAL_EventCreate_func(phEvent);
}

OSAL_ERRORTYPE
OSAL_EventDestroy(OSAL_PTR hEvent)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_EventDestroy_func_t)(OSAL_PTR hEvent);
    OSAL_EventDestroy_func_t OSAL_EventDestroy_func =
        (OSAL_EventDestroy_func_t) dlsym(handle, "OSAL_EventDestroy");
    LIB_CALL_CHECK(OSAL_EventDestroy_func);
    return OSAL_EventDestroy_func(hEvent);
}

OSAL_ERRORTYPE
OSAL_EventReset(OSAL_PTR hEvent)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_EventReset_func_t)(OSAL_PTR hEvent);
    OSAL_EventReset_func_t OSAL_EventReset_func =
        (OSAL_EventReset_func_t) dlsym(handle, "OSAL_EventReset");
    LIB_CALL_CHECK(OSAL_EventReset_func);
    return OSAL_EventReset_func(hEvent);
}

OSAL_ERRORTYPE
OSAL_EventSet(OSAL_PTR hEvent)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_EventSet_func_t)(OSAL_PTR hEvent);
    OSAL_EventSet_func_t OSAL_EventSet_func =
        (OSAL_EventSet_func_t) dlsym(handle, "OSAL_EventSet");
    LIB_CALL_CHECK(OSAL_EventSet_func);
    return OSAL_EventSet_func(hEvent);
}

OSAL_ERRORTYPE
OSAL_EventWait(OSAL_PTR hEvent, OSAL_U32 mSec, OSAL_BOOL* pbTimedOut)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_EventWait_func_t)(OSAL_PTR   hEvent,
                                                    OSAL_U32   mSec,
                                                    OSAL_BOOL* pbTimedOut);
    OSAL_EventWait_func_t OSAL_EventWait_func =
        (OSAL_EventWait_func_t) dlsym(handle, "OSAL_EventWait");
    LIB_CALL_CHECK(OSAL_EventWait_func);
    return OSAL_EventWait_func(hEvent, mSec, pbTimedOut);
}

OSAL_ERRORTYPE
OSAL_EventWaitMultiple(OSAL_PTR*  hEvents,
                       OSAL_BOOL* bSignaled,
                       OSAL_U32   nCount,
                       OSAL_U32   mSec,
                       OSAL_BOOL* pbTimedOut)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (
        *OSAL_EventWaitMultiple_func_t)(OSAL_PTR*  hEvents,
                                        OSAL_BOOL* bSignaled,
                                        OSAL_U32   nCount,
                                        OSAL_U32   mSec,
                                        OSAL_BOOL* pbTimedOut);
    OSAL_EventWaitMultiple_func_t OSAL_EventWaitMultiple_func =
        (OSAL_EventWaitMultiple_func_t) dlsym(handle, "OSAL_EventWaitMultiple");
    LIB_CALL_CHECK(OSAL_EventWaitMultiple_func);
    return OSAL_EventWaitMultiple_func(hEvents,
                                       bSignaled,
                                       nCount,
                                       mSec,
                                       pbTimedOut);
}

void
OSAL_Free(OSAL_PTR pData)
{
    LIB_CALL_CHECK(handle);
    typedef void (*OSAL_Free_func_t)(OSAL_PTR pData);
    OSAL_Free_func_t OSAL_Free_func =
        (OSAL_Free_func_t) dlsym(handle, "OSAL_Free");
    LIB_CALL_CHECK(OSAL_Free_func);
    return OSAL_Free_func(pData);
}

OSAL_U32
OSAL_GetTime(void)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_U32 (*OSAL_GetTime_func_t)(void);
    OSAL_GetTime_func_t OSAL_GetTime_func =
        (OSAL_GetTime_func_t) dlsym(handle, "OSAL_GetTime");
    LIB_CALL_CHECK(OSAL_GetTime_func);
    return OSAL_GetTime_func();
}

OSAL_PTR
OSAL_Malloc(OSAL_U32 size)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_PTR (*OSAL_Malloc_func_t)(OSAL_U32 size);
    OSAL_Malloc_func_t OSAL_Malloc_func =
        (OSAL_Malloc_func_t) dlsym(handle, "OSAL_Malloc");
    LIB_CALL_CHECK(OSAL_Malloc_func);
    return OSAL_Malloc_func(size);
}

OSAL_PTR
OSAL_Memcpy(OSAL_PTR pDest, OSAL_PTR pSrc, OSAL_U32 count)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_PTR (
        *OSAL_Memcpy_func_t)(OSAL_PTR pDest, OSAL_PTR pSrc, OSAL_U32 count);
    OSAL_Memcpy_func_t OSAL_Memcpy_func =
        (OSAL_Memcpy_func_t) dlsym(handle, "OSAL_Memcpy");
    LIB_CALL_CHECK(OSAL_Memcpy_func);
    return OSAL_Memcpy_func(pDest, pSrc, count);
}

OSAL_PTR
OSAL_Memset(OSAL_PTR pDest, OSAL_U32 ch, OSAL_U32 count)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_PTR (
        *OSAL_Memset_func_t)(OSAL_PTR pDest, OSAL_U32 ch, OSAL_U32 count);
    OSAL_Memset_func_t OSAL_Memset_func =
        (OSAL_Memset_func_t) dlsym(handle, "OSAL_Memset");
    LIB_CALL_CHECK(OSAL_Memset_func);
    return OSAL_Memset_func(pDest, ch, count);
}

OSAL_ERRORTYPE
OSAL_MutexCreate(OSAL_PTR* phMutex)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_MutexCreate_func_t)(OSAL_PTR* phMutex);
    OSAL_MutexCreate_func_t OSAL_MutexCreate_func =
        (OSAL_MutexCreate_func_t) dlsym(handle, "OSAL_MutexCreate");
    LIB_CALL_CHECK(OSAL_MutexCreate_func);
    return OSAL_MutexCreate_func(phMutex);
}

OSAL_ERRORTYPE
OSAL_MutexDestroy(OSAL_PTR hMutex)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_MutexDestroy_func_t)(OSAL_PTR hMutex);
    OSAL_MutexDestroy_func_t OSAL_MutexDestroy_func =
        (OSAL_MutexDestroy_func_t) dlsym(handle, "OSAL_MutexDestroy");
    LIB_CALL_CHECK(OSAL_MutexDestroy_func);
    return OSAL_MutexDestroy_func(hMutex);
}

OSAL_ERRORTYPE
OSAL_MutexLock(OSAL_PTR hMutex)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_MutexLock_func_t)(OSAL_PTR hMutex);
    OSAL_MutexLock_func_t OSAL_MutexLock_func =
        (OSAL_MutexLock_func_t) dlsym(handle, "OSAL_MutexLock");
    LIB_CALL_CHECK(OSAL_MutexLock_func);
    return OSAL_MutexLock_func(hMutex);
}

OSAL_ERRORTYPE
OSAL_MutexUnlock(OSAL_PTR hMutex)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_MutexUnlock_func_t)(OSAL_PTR hMutex);
    OSAL_MutexUnlock_func_t OSAL_MutexUnlock_func =
        (OSAL_MutexUnlock_func_t) dlsym(handle, "OSAL_MutexUnlock");
    LIB_CALL_CHECK(OSAL_MutexUnlock_func);
    return OSAL_MutexUnlock_func(hMutex);
}

OSAL_ERRORTYPE
OSAL_ThreadCreate(OSAL_U32 (*pFunc)(OSAL_PTR pParam),
                  OSAL_PTR  pParam,
                  OSAL_U32  nPriority,
                  OSAL_PTR* phThread)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_ThreadCreate_func_t)(OSAL_U32 (*pFunc)(OSAL_PTR pParam),
                                   OSAL_PTR  pParam,
                                   OSAL_U32  nPriority,
                                   OSAL_PTR* phThread);
    OSAL_ThreadCreate_func_t OSAL_ThreadCreate_func =
        (OSAL_ThreadCreate_func_t) dlsym(handle, "OSAL_ThreadCreate");
    LIB_CALL_CHECK(OSAL_ThreadCreate_func);
    return OSAL_ThreadCreate_func(pFunc, pParam, nPriority, phThread);
}

OSAL_ERRORTYPE
OSAL_ThreadDestroy(OSAL_PTR hThread)
{
    LIB_CALL_CHECK(handle);
    typedef OSAL_ERRORTYPE (*OSAL_ThreadDestroy_func_t)(OSAL_PTR hThread);
    OSAL_ThreadDestroy_func_t OSAL_ThreadDestroy_func =
        (OSAL_ThreadDestroy_func_t) dlsym(handle, "OSAL_ThreadDestroy");
    LIB_CALL_CHECK(OSAL_ThreadDestroy_func);
    return OSAL_ThreadDestroy_func(hThread);
}

void
OSAL_ThreadSleep(OSAL_U32 ms)
{
    LIB_CALL_CHECK(handle);
    typedef void (*OSAL_ThreadSleep_func_t)(OSAL_U32 ms);
    OSAL_ThreadSleep_func_t OSAL_ThreadSleep_func =
        (OSAL_ThreadSleep_func_t) dlsym(handle, "OSAL_ThreadSleep");
    LIB_CALL_CHECK(OSAL_ThreadSleep_func);
    return OSAL_ThreadSleep_func(ms);
}
