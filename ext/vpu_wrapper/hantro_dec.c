#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hantro_dec.h"

#define LIB_NAME "libhantro.so.1"
#define ENV_NAME "LIBHANTRO_LOCATION"

#define LIB_CALL_CHECK(ptr)                                                 \
    do                                                                      \
    {                                                                       \
        if (!ptr) {                                                         \
            fprintf(stderr, "Error calling %s: %s\n", __func__, dlerror()); \
            assert(false); }                                                \
    } while (0)

static void *handle = NULL;

// Workaround: The Hantro library uses libm symbols (e.g., pow) but doesn't
// link to libm. We load libm with RTLD_GLOBAL to make its symbols available
// for subsequent dlopen calls.
static void ensure_libm_global(void)
{
    static int done = 0;
    if (!done) {
        dlopen("libm.so.6", RTLD_NOW | RTLD_GLOBAL);
        done = 1;
    }
}

int
HantroDec_OpenLib()
{
    if (handle == NULL) {
        const char* lib_path = NULL;

        // Ensure libm symbols are globally available for Hantro library
        ensure_libm_global();

        lib_path = getenv(ENV_NAME);
        if (lib_path == NULL) {
            // Environmental variable is not set, search for the library in
            // typical locations
            // Use RTLD_NOW to resolve all symbols immediately
            handle = dlopen(LIB_NAME, RTLD_NOW);
        } else {
            // Environmental variable is set, try to load the library from the
            // specified path
            handle = dlopen(lib_path, RTLD_NOW);
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
HantroDec_CloseLib()
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
void
DWLDisableHw(const void* instance, i32 core_id, u32 offset, u32 value)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLDisableHw_func_t)(const void* instance,
                                        i32         core_id,
                                        u32         offset,
                                        u32         value);
    DWLDisableHw_func_t DWLDisableHw_func =
        (DWLDisableHw_func_t) dlsym(handle, "DWLDisableHw");
    LIB_CALL_CHECK(DWLDisableHw_func);
    return DWLDisableHw_func(instance, core_id, offset, value);
}

void
DWLEnableHw(const void* instance, i32 core_id, u32 offset, u32 value)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLEnableHw_func_t)(const void* instance,
                                       i32         core_id,
                                       u32         offset,
                                       u32         value);
    DWLEnableHw_func_t DWLEnableHw_func =
        (DWLEnableHw_func_t) dlsym(handle, "DWLEnableHw");
    LIB_CALL_CHECK(DWLEnableHw_func);
    return DWLEnableHw_func(instance, core_id, offset, value);
}

i32
DWLFlushCache(const void* instance, struct DWLLinearMem* info)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*DWLFlushCache_func_t)(const void*          instance,
                                        struct DWLLinearMem* info);
    DWLFlushCache_func_t DWLFlushCache_func =
        (DWLFlushCache_func_t) dlsym(handle, "DWLFlushCache");
    LIB_CALL_CHECK(DWLFlushCache_func);
    return DWLFlushCache_func(instance, info);
}

void
DWLFreeLinear(const void* instance, struct DWLLinearMem* info)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLFreeLinear_func_t)(const void*          instance,
                                         struct DWLLinearMem* info);
    DWLFreeLinear_func_t DWLFreeLinear_func =
        (DWLFreeLinear_func_t) dlsym(handle, "DWLFreeLinear");
    LIB_CALL_CHECK(DWLFreeLinear_func);
    return DWLFreeLinear_func(instance, info);
}

void
DWLFreeRefFrm(const void* instance, struct DWLLinearMem* info)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLFreeRefFrm_func_t)(const void*          instance,
                                         struct DWLLinearMem* info);
    DWLFreeRefFrm_func_t DWLFreeRefFrm_func =
        (DWLFreeRefFrm_func_t) dlsym(handle, "DWLFreeRefFrm");
    LIB_CALL_CHECK(DWLFreeRefFrm_func);
    return DWLFreeRefFrm_func(instance, info);
}

const void*
DWLInit(struct DWLInitParam* param)
{
    LIB_CALL_CHECK(handle);
    typedef const void* (*DWLInit_func_t)(struct DWLInitParam* param);
    DWLInit_func_t DWLInit_func = (DWLInit_func_t) dlsym(handle, "DWLInit");
    LIB_CALL_CHECK(DWLInit_func);
    return DWLInit_func(param);
}

i32
DWLMallocLinear(const void* instance, u32 size, struct DWLLinearMem* info)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*DWLMallocLinear_func_t)(const void*          instance,
                                          u32                  size,
                                          struct DWLLinearMem* info);
    DWLMallocLinear_func_t DWLMallocLinear_func =
        (DWLMallocLinear_func_t) dlsym(handle, "DWLMallocLinear");
    LIB_CALL_CHECK(DWLMallocLinear_func);
    return DWLMallocLinear_func(instance, size, info);
}

i32
DWLMallocRefFrm(const void* instance, u32 size, struct DWLLinearMem* info)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*DWLMallocRefFrm_func_t)(const void*          instance,
                                          u32                  size,
                                          struct DWLLinearMem* info);
    DWLMallocRefFrm_func_t DWLMallocRefFrm_func =
        (DWLMallocRefFrm_func_t) dlsym(handle, "DWLMallocRefFrm");
    LIB_CALL_CHECK(DWLMallocRefFrm_func);
    return DWLMallocRefFrm_func(instance, size, info);
}

void*
DWLPrivateAreaMemcpy(void* d, const void* s, u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef void* (*DWLPrivateAreaMemcpy_func_t)(void* d, const void* s, u32 n);
    DWLPrivateAreaMemcpy_func_t DWLPrivateAreaMemcpy_func =
        (DWLPrivateAreaMemcpy_func_t) dlsym(handle, "DWLPrivateAreaMemcpy");
    LIB_CALL_CHECK(DWLPrivateAreaMemcpy_func);
    return DWLPrivateAreaMemcpy_func(d, s, n);
}

void*
DWLPrivateAreaMemset(void* p, i32 c, u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef void* (*DWLPrivateAreaMemset_func_t)(void* p, i32 c, u32 n);
    DWLPrivateAreaMemset_func_t DWLPrivateAreaMemset_func =
        (DWLPrivateAreaMemset_func_t) dlsym(handle, "DWLPrivateAreaMemset");
    LIB_CALL_CHECK(DWLPrivateAreaMemset_func);
    return DWLPrivateAreaMemset_func(p, c, n);
}

u8
DWLPrivateAreaReadByte(const u8* p)
{
    LIB_CALL_CHECK(handle);
    typedef u8 (*DWLPrivateAreaReadByte_func_t)(const u8* p);
    DWLPrivateAreaReadByte_func_t DWLPrivateAreaReadByte_func =
        (DWLPrivateAreaReadByte_func_t) dlsym(handle, "DWLPrivateAreaReadByte");
    LIB_CALL_CHECK(DWLPrivateAreaReadByte_func);
    return DWLPrivateAreaReadByte_func(p);
}

void
DWLPrivateAreaWriteByte(u8* p, u8 data)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLPrivateAreaWriteByte_func_t)(u8* p, u8 data);
    DWLPrivateAreaWriteByte_func_t DWLPrivateAreaWriteByte_func =
        (DWLPrivateAreaWriteByte_func_t) dlsym(handle,
                                               "DWLPrivateAreaWriteByte");
    LIB_CALL_CHECK(DWLPrivateAreaWriteByte_func);
    return DWLPrivateAreaWriteByte_func(p, data);
}

void
DWLReadAsicConfig(DWLHwConfig* hw_cfg, u32 client_type)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLReadAsicConfig_func_t)(DWLHwConfig* hw_cfg,
                                             u32          client_type);
    DWLReadAsicConfig_func_t DWLReadAsicConfig_func =
        (DWLReadAsicConfig_func_t) dlsym(handle, "DWLReadAsicConfig");
    LIB_CALL_CHECK(DWLReadAsicConfig_func);
    return DWLReadAsicConfig_func(hw_cfg, client_type);
}

u32
DWLReadAsicCoreCount(void)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*DWLReadAsicCoreCount_func_t)(void);
    DWLReadAsicCoreCount_func_t DWLReadAsicCoreCount_func =
        (DWLReadAsicCoreCount_func_t) dlsym(handle, "DWLReadAsicCoreCount");
    LIB_CALL_CHECK(DWLReadAsicCoreCount_func);
    return DWLReadAsicCoreCount_func();
}

void
DWLReadAsicFuseStatus(struct DWLHwFuseStatus* hw_fuse_sts)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLReadAsicFuseStatus_func_t)(
        struct DWLHwFuseStatus* hw_fuse_sts);
    DWLReadAsicFuseStatus_func_t DWLReadAsicFuseStatus_func =
        (DWLReadAsicFuseStatus_func_t) dlsym(handle, "DWLReadAsicFuseStatus");
    LIB_CALL_CHECK(DWLReadAsicFuseStatus_func);
    return DWLReadAsicFuseStatus_func(hw_fuse_sts);
}

u32
DWLReadAsicID(u32 client_type)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*DWLReadAsicID_func_t)(u32 client_type);
    DWLReadAsicID_func_t DWLReadAsicID_func =
        (DWLReadAsicID_func_t) dlsym(handle, "DWLReadAsicID");
    LIB_CALL_CHECK(DWLReadAsicID_func);
    return DWLReadAsicID_func(client_type);
}

void
DWLReadMCAsicConfig(DWLHwConfig hw_cfg[MAX_ASIC_CORES])
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLReadMCAsicConfig_func_t)(
        DWLHwConfig hw_cfg[MAX_ASIC_CORES]);
    DWLReadMCAsicConfig_func_t DWLReadMCAsicConfig_func =
        (DWLReadMCAsicConfig_func_t) dlsym(handle, "DWLReadMCAsicConfig");
    LIB_CALL_CHECK(DWLReadMCAsicConfig_func);
    return DWLReadMCAsicConfig_func(hw_cfg);
}

u32
DWLReadReg(const void* instance, i32 core_id, u32 offset)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (
        *DWLReadReg_func_t)(const void* instance, i32 core_id, u32 offset);
    DWLReadReg_func_t DWLReadReg_func =
        (DWLReadReg_func_t) dlsym(handle, "DWLReadReg");
    LIB_CALL_CHECK(DWLReadReg_func);
    return DWLReadReg_func(instance, core_id, offset);
}

i32
DWLRelease(const void* instance)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*DWLRelease_func_t)(const void* instance);
    DWLRelease_func_t DWLRelease_func =
        (DWLRelease_func_t) dlsym(handle, "DWLRelease");
    LIB_CALL_CHECK(DWLRelease_func);
    return DWLRelease_func(instance);
}

void
DWLReleaseHw(const void* instance, i32 core_id)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLReleaseHw_func_t)(const void* instance, i32 core_id);
    DWLReleaseHw_func_t DWLReleaseHw_func =
        (DWLReleaseHw_func_t) dlsym(handle, "DWLReleaseHw");
    LIB_CALL_CHECK(DWLReleaseHw_func);
    return DWLReleaseHw_func(instance, core_id);
}

i32
DWLReserveHw(const void* instance, i32* core_id)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*DWLReserveHw_func_t)(const void* instance, i32* core_id);
    DWLReserveHw_func_t DWLReserveHw_func =
        (DWLReserveHw_func_t) dlsym(handle, "DWLReserveHw");
    LIB_CALL_CHECK(DWLReserveHw_func);
    return DWLReserveHw_func(instance, core_id);
}

i32
DWLReserveHwPipe(const void* instance, i32* core_id)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*DWLReserveHwPipe_func_t)(const void* instance, i32* core_id);
    DWLReserveHwPipe_func_t DWLReserveHwPipe_func =
        (DWLReserveHwPipe_func_t) dlsym(handle, "DWLReserveHwPipe");
    LIB_CALL_CHECK(DWLReserveHwPipe_func);
    return DWLReserveHwPipe_func(instance, core_id);
}

void
DWLSetIRQCallback(const void*       instance,
                  i32               core_id,
                  DWLIRQCallbackFn* callback_fn,
                  void*             arg)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLSetIRQCallback_func_t)(const void*       instance,
                                             i32               core_id,
                                             DWLIRQCallbackFn* callback_fn,
                                             void*             arg);
    DWLSetIRQCallback_func_t DWLSetIRQCallback_func =
        (DWLSetIRQCallback_func_t) dlsym(handle, "DWLSetIRQCallback");
    LIB_CALL_CHECK(DWLSetIRQCallback_func);
    return DWLSetIRQCallback_func(instance, core_id, callback_fn, arg);
}

void
DWLSetSecureMode(const void* instance, u32 use_secure_mode)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLSetSecureMode_func_t)(const void* instance,
                                            u32         use_secure_mode);
    DWLSetSecureMode_func_t DWLSetSecureMode_func =
        (DWLSetSecureMode_func_t) dlsym(handle, "DWLSetSecureMode");
    LIB_CALL_CHECK(DWLSetSecureMode_func);
    return DWLSetSecureMode_func(instance, use_secure_mode);
}

i32
DWLWaitHwReady(const void* instance, i32 core_id, u32 timeout)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (
        *DWLWaitHwReady_func_t)(const void* instance, i32 core_id, u32 timeout);
    DWLWaitHwReady_func_t DWLWaitHwReady_func =
        (DWLWaitHwReady_func_t) dlsym(handle, "DWLWaitHwReady");
    LIB_CALL_CHECK(DWLWaitHwReady_func);
    return DWLWaitHwReady_func(instance, core_id, timeout);
}

void
DWLWriteReg(const void* instance, i32 core_id, u32 offset, u32 value)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLWriteReg_func_t)(const void* instance,
                                       i32         core_id,
                                       u32         offset,
                                       u32         value);
    DWLWriteReg_func_t DWLWriteReg_func =
        (DWLWriteReg_func_t) dlsym(handle, "DWLWriteReg");
    LIB_CALL_CHECK(DWLWriteReg_func);
    return DWLWriteReg_func(instance, core_id, offset, value);
}

void*
DWLcalloc(u32 n, u32 s)
{
    LIB_CALL_CHECK(handle);
    typedef void* (*DWLcalloc_func_t)(u32 n, u32 s);
    DWLcalloc_func_t DWLcalloc_func =
        (DWLcalloc_func_t) dlsym(handle, "DWLcalloc");
    LIB_CALL_CHECK(DWLcalloc_func);
    return DWLcalloc_func(n, s);
}

void
DWLfree(void* p)
{
    LIB_CALL_CHECK(handle);
    typedef void (*DWLfree_func_t)(void* p);
    DWLfree_func_t DWLfree_func = (DWLfree_func_t) dlsym(handle, "DWLfree");
    LIB_CALL_CHECK(DWLfree_func);
    return DWLfree_func(p);
}

void*
DWLmalloc(u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef void* (*DWLmalloc_func_t)(u32 n);
    DWLmalloc_func_t DWLmalloc_func =
        (DWLmalloc_func_t) dlsym(handle, "DWLmalloc");
    LIB_CALL_CHECK(DWLmalloc_func);
    return DWLmalloc_func(n);
}

void*
DWLmemcpy(void* d, const void* s, u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef void* (*DWLmemcpy_func_t)(void* d, const void* s, u32 n);
    DWLmemcpy_func_t DWLmemcpy_func =
        (DWLmemcpy_func_t) dlsym(handle, "DWLmemcpy");
    LIB_CALL_CHECK(DWLmemcpy_func);
    return DWLmemcpy_func(d, s, n);
}

void*
DWLmemset(void* d, i32 c, u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef void* (*DWLmemset_func_t)(void* d, i32 c, u32 n);
    DWLmemset_func_t DWLmemset_func =
        (DWLmemset_func_t) dlsym(handle, "DWLmemset");
    LIB_CALL_CHECK(DWLmemset_func);
    return DWLmemset_func(d, c, n);
}

enum DecRet
HevcDecAbort(HevcDecInst dec_inst)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecAbort_func_t)(HevcDecInst dec_inst);
    HevcDecAbort_func_t HevcDecAbort_func =
        (HevcDecAbort_func_t) dlsym(handle, "HevcDecAbort");
    LIB_CALL_CHECK(HevcDecAbort_func);
    return HevcDecAbort_func(dec_inst);
}

enum DecRet
HevcDecAbortAfter(HevcDecInst dec_inst)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecAbortAfter_func_t)(HevcDecInst dec_inst);
    HevcDecAbortAfter_func_t HevcDecAbortAfter_func =
        (HevcDecAbortAfter_func_t) dlsym(handle, "HevcDecAbortAfter");
    LIB_CALL_CHECK(HevcDecAbortAfter_func);
    return HevcDecAbortAfter_func(dec_inst);
}

enum DecRet
HevcDecAddBuffer(HevcDecInst dec_inst, struct DWLLinearMem* info)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecAddBuffer_func_t)(HevcDecInst dec_inst,
                                                   struct DWLLinearMem* info);
    HevcDecAddBuffer_func_t HevcDecAddBuffer_func =
        (HevcDecAddBuffer_func_t) dlsym(handle, "HevcDecAddBuffer");
    LIB_CALL_CHECK(HevcDecAddBuffer_func);
    return HevcDecAddBuffer_func(dec_inst, info);
}

enum DecRet
HevcDecDecode(HevcDecInst                dec_inst,
              const struct HevcDecInput* input,
              struct HevcDecOutput*      output)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (
        *HevcDecDecode_func_t)(HevcDecInst                dec_inst,
                               const struct HevcDecInput* input,
                               struct HevcDecOutput*      output);
    HevcDecDecode_func_t HevcDecDecode_func =
        (HevcDecDecode_func_t) dlsym(handle, "HevcDecDecode");
    LIB_CALL_CHECK(HevcDecDecode_func);
    return HevcDecDecode_func(dec_inst, input, output);
}

u32
HevcDecDiscardDpbNums(HevcDecInst dec_inst)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*HevcDecDiscardDpbNums_func_t)(HevcDecInst dec_inst);
    HevcDecDiscardDpbNums_func_t HevcDecDiscardDpbNums_func =
        (HevcDecDiscardDpbNums_func_t) dlsym(handle, "HevcDecDiscardDpbNums");
    LIB_CALL_CHECK(HevcDecDiscardDpbNums_func);
    return HevcDecDiscardDpbNums_func(dec_inst);
}

enum DecRet
HevcDecEndOfStream(HevcDecInst dec_inst)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecEndOfStream_func_t)(HevcDecInst dec_inst);
    HevcDecEndOfStream_func_t HevcDecEndOfStream_func =
        (HevcDecEndOfStream_func_t) dlsym(handle, "HevcDecEndOfStream");
    LIB_CALL_CHECK(HevcDecEndOfStream_func);
    return HevcDecEndOfStream_func(dec_inst);
}

enum DecRet
HevcDecGetBufferInfo(HevcDecInst dec_inst, struct HevcDecBufferInfo* mem_info)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (
        *HevcDecGetBufferInfo_func_t)(HevcDecInst               dec_inst,
                                      struct HevcDecBufferInfo* mem_info);
    HevcDecGetBufferInfo_func_t HevcDecGetBufferInfo_func =
        (HevcDecGetBufferInfo_func_t) dlsym(handle, "HevcDecGetBufferInfo");
    LIB_CALL_CHECK(HevcDecGetBufferInfo_func);
    return HevcDecGetBufferInfo_func(dec_inst, mem_info);
}

HevcDecBuild
HevcDecGetBuild(void)
{
    LIB_CALL_CHECK(handle);
    typedef HevcDecBuild (*HevcDecGetBuild_func_t)(void);
    HevcDecGetBuild_func_t HevcDecGetBuild_func =
        (HevcDecGetBuild_func_t) dlsym(handle, "HevcDecGetBuild");
    LIB_CALL_CHECK(HevcDecGetBuild_func);
    return HevcDecGetBuild_func();
}

enum DecRet
HevcDecGetInfo(HevcDecInst dec_inst, struct HevcDecInfo* dec_info)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecGetInfo_func_t)(HevcDecInst         dec_inst,
                                                 struct HevcDecInfo* dec_info);
    HevcDecGetInfo_func_t HevcDecGetInfo_func =
        (HevcDecGetInfo_func_t) dlsym(handle, "HevcDecGetInfo");
    LIB_CALL_CHECK(HevcDecGetInfo_func);
    return HevcDecGetInfo_func(dec_inst, dec_info);
}

enum DecRet
HevcDecGetSpsBitDepth(HevcDecInst dec_inst, u32* bit_depth)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecGetSpsBitDepth_func_t)(HevcDecInst dec_inst,
                                                        u32*        bit_depth);
    HevcDecGetSpsBitDepth_func_t HevcDecGetSpsBitDepth_func =
        (HevcDecGetSpsBitDepth_func_t) dlsym(handle, "HevcDecGetSpsBitDepth");
    LIB_CALL_CHECK(HevcDecGetSpsBitDepth_func);
    return HevcDecGetSpsBitDepth_func(dec_inst, bit_depth);
}

enum DecRet
HevcDecInit(HevcDecInst*          dec_inst,
            const void*           dwl,
            struct HevcDecConfig* dec_cfg)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecInit_func_t)(HevcDecInst*          dec_inst,
                                              const void*           dwl,
                                              struct HevcDecConfig* dec_cfg);
    HevcDecInit_func_t HevcDecInit_func =
        (HevcDecInit_func_t) dlsym(handle, "HevcDecInit");
    LIB_CALL_CHECK(HevcDecInit_func);
    return HevcDecInit_func(dec_inst, dwl, dec_cfg);
}

enum DecRet
HevcDecNextPicture(HevcDecInst dec_inst, struct HevcDecPicture* picture)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (
        *HevcDecNextPicture_func_t)(HevcDecInst            dec_inst,
                                    struct HevcDecPicture* picture);
    HevcDecNextPicture_func_t HevcDecNextPicture_func =
        (HevcDecNextPicture_func_t) dlsym(handle, "HevcDecNextPicture");
    LIB_CALL_CHECK(HevcDecNextPicture_func);
    return HevcDecNextPicture_func(dec_inst, picture);
}

enum DecRet
HevcDecPeek(HevcDecInst dec_inst, struct HevcDecPicture* output)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecPeek_func_t)(HevcDecInst            dec_inst,
                                              struct HevcDecPicture* output);
    HevcDecPeek_func_t HevcDecPeek_func =
        (HevcDecPeek_func_t) dlsym(handle, "HevcDecPeek");
    LIB_CALL_CHECK(HevcDecPeek_func);
    return HevcDecPeek_func(dec_inst, output);
}

enum DecRet
HevcDecPictureConsumed(HevcDecInst                  dec_inst,
                       const struct HevcDecPicture* picture)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (
        *HevcDecPictureConsumed_func_t)(HevcDecInst                  dec_inst,
                                        const struct HevcDecPicture* picture);
    HevcDecPictureConsumed_func_t HevcDecPictureConsumed_func =
        (HevcDecPictureConsumed_func_t) dlsym(handle, "HevcDecPictureConsumed");
    LIB_CALL_CHECK(HevcDecPictureConsumed_func);
    return HevcDecPictureConsumed_func(dec_inst, picture);
}

void
HevcDecRelease(HevcDecInst dec_inst)
{
    LIB_CALL_CHECK(handle);
    typedef void (*HevcDecRelease_func_t)(HevcDecInst dec_inst);
    HevcDecRelease_func_t HevcDecRelease_func =
        (HevcDecRelease_func_t) dlsym(handle, "HevcDecRelease");
    LIB_CALL_CHECK(HevcDecRelease_func);
    return HevcDecRelease_func(dec_inst);
}

enum DecRet
HevcDecRemoveBuffer(HevcDecInst dec_inst)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecRemoveBuffer_func_t)(HevcDecInst dec_inst);
    HevcDecRemoveBuffer_func_t HevcDecRemoveBuffer_func =
        (HevcDecRemoveBuffer_func_t) dlsym(handle, "HevcDecRemoveBuffer");
    LIB_CALL_CHECK(HevcDecRemoveBuffer_func);
    return HevcDecRemoveBuffer_func(dec_inst);
}

enum DecRet
HevcDecSetInfo(HevcDecInst dec_inst, struct HevcDecConfig* dec_cfg)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecSetInfo_func_t)(HevcDecInst           dec_inst,
                                                 struct HevcDecConfig* dec_cfg);
    HevcDecSetInfo_func_t HevcDecSetInfo_func =
        (HevcDecSetInfo_func_t) dlsym(handle, "HevcDecSetInfo");
    LIB_CALL_CHECK(HevcDecSetInfo_func);
    return HevcDecSetInfo_func(dec_inst, dec_cfg);
}

enum DecRet
HevcDecSetNoReorder(HevcDecInst dec_inst, u32 no_reorder)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (*HevcDecSetNoReorder_func_t)(HevcDecInst dec_inst,
                                                      u32         no_reorder);
    HevcDecSetNoReorder_func_t HevcDecSetNoReorder_func =
        (HevcDecSetNoReorder_func_t) dlsym(handle, "HevcDecSetNoReorder");
    LIB_CALL_CHECK(HevcDecSetNoReorder_func);
    return HevcDecSetNoReorder_func(dec_inst, no_reorder);
}

enum DecRet
HevcDecUseExtraFrmBuffers(HevcDecInst dec_inst, u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef enum DecRet (
        *HevcDecUseExtraFrmBuffers_func_t)(HevcDecInst dec_inst, u32 n);
    HevcDecUseExtraFrmBuffers_func_t HevcDecUseExtraFrmBuffers_func =
        (HevcDecUseExtraFrmBuffers_func_t) dlsym(handle,
                                                 "HevcDecUseExtraFrmBuffers");
    LIB_CALL_CHECK(HevcDecUseExtraFrmBuffers_func);
    return HevcDecUseExtraFrmBuffers_func(dec_inst, n);
}
