#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "hantro_vc8000e_enc.h"

#define LIB_NAME "libhantro_vc8000e.so.1"
#define ENV_NAME "LIBHANTRO_VC8000E_LOCATION"

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
        void *libm_handle = dlopen("libm.so.6", RTLD_NOW | RTLD_GLOBAL);
        if (!libm_handle) {
            libm_handle = dlopen("libm.so", RTLD_NOW | RTLD_GLOBAL);
        }
        done = 1;
    }
}

int
HantroVCEnc_OpenLib()
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

int HantroVCEnc_CloseLib()
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

u32 EWLReadAsicID(u32 core_id)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*EWLReadAsicID_func_t)(u32 core_id);
    EWLReadAsicID_func_t EWLReadAsicID_func = (EWLReadAsicID_func_t)dlsym(handle, "EWLReadAsicID");
    LIB_CALL_CHECK(EWLReadAsicID_func);
    return EWLReadAsicID_func(core_id);
}

u32 EWLGetCoreNum(void)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*EWLGetCoreNum_func_t)(void);
    EWLGetCoreNum_func_t EWLGetCoreNum_func = (EWLGetCoreNum_func_t)dlsym(handle, "EWLGetCoreNum");
    LIB_CALL_CHECK(EWLGetCoreNum_func);
    return EWLGetCoreNum_func();
}

i32 EWLGetDec400Coreid(const void *inst)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*EWLGetDec400Coreid_func_t)(const void *inst);
    EWLGetDec400Coreid_func_t EWLGetDec400Coreid_func = (EWLGetDec400Coreid_func_t)dlsym(handle, "EWLGetDec400Coreid");
    LIB_CALL_CHECK(EWLGetDec400Coreid_func);
    return EWLGetDec400Coreid_func(inst);
}

int MapAsicRegisters(void *ewl)
{
    LIB_CALL_CHECK(handle);
    typedef int (*MapAsicRegisters_func_t)(void *ewl);
    MapAsicRegisters_func_t MapAsicRegisters_func = (MapAsicRegisters_func_t)dlsym(handle, "MapAsicRegisters");
    LIB_CALL_CHECK(MapAsicRegisters_func);
    return MapAsicRegisters_func(ewl);
}

const void *EWLInit(EWLInitParam_t *param)
{
    LIB_CALL_CHECK(handle);
    typedef const void * (*EWLInit_func_t)(EWLInitParam_t *param);
    EWLInit_func_t EWLInit_func = (EWLInit_func_t)dlsym(handle, "EWLInit");
    LIB_CALL_CHECK(EWLInit_func);
    return EWLInit_func(param);
}

EWLHwConfig_t EWLReadAsicConfig(u32 core_id)
{
    LIB_CALL_CHECK(handle);
    typedef EWLHwConfig_t (*EWLReadAsicConfig_func_t)(u32 core_id);
    EWLReadAsicConfig_func_t EWLReadAsicConfig_func = (EWLReadAsicConfig_func_t)dlsym(handle, "EWLReadAsicConfig");
    LIB_CALL_CHECK(EWLReadAsicConfig_func);
    return EWLReadAsicConfig_func(core_id);
}

i32 EWLRelease(const void *inst)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*EWLRelease_func_t)(const void *inst);
    EWLRelease_func_t EWLRelease_func = (EWLRelease_func_t)dlsym(handle, "EWLRelease");
    LIB_CALL_CHECK(EWLRelease_func);
    return EWLRelease_func(inst);
}

i32 EWLReserveHw(const void *inst, u32 *core_info)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*EWLReserveHw_func_t)(const void *inst, u32 *core_info);
    EWLReserveHw_func_t EWLReserveHw_func = (EWLReserveHw_func_t)dlsym(handle, "EWLReserveHw");
    LIB_CALL_CHECK(EWLReserveHw_func);
    return EWLReserveHw_func(inst, core_info);
}

void EWLReleaseHw(const void *inst)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLReleaseHw_func_t)(const void *inst);
    EWLReleaseHw_func_t EWLReleaseHw_func = (EWLReleaseHw_func_t)dlsym(handle, "EWLReleaseHw");
    LIB_CALL_CHECK(EWLReleaseHw_func);
    return EWLReleaseHw_func(inst);
}

u32 EWLGetPerformance(const void *inst)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*EWLGetPerformance_func_t)(const void *inst);
    EWLGetPerformance_func_t EWLGetPerformance_func = (EWLGetPerformance_func_t)dlsym(handle, "EWLGetPerformance");
    LIB_CALL_CHECK(EWLGetPerformance_func);
    return EWLGetPerformance_func(inst);
}

i32 EWLMallocRefFrm(const void *instance, u32 size, u32 alignment, EWLLinearMem_t *info)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*EWLMallocRefFrm_func_t)(const void *instance, u32 size, u32 alignment, EWLLinearMem_t *info);
    EWLMallocRefFrm_func_t EWLMallocRefFrm_func = (EWLMallocRefFrm_func_t)dlsym(handle, "EWLMallocRefFrm");
    LIB_CALL_CHECK(EWLMallocRefFrm_func);
    return EWLMallocRefFrm_func(instance, size, alignment, info);
}

void EWLFreeRefFrm(const void *inst, EWLLinearMem_t *info)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLFreeRefFrm_func_t)(const void *inst, EWLLinearMem_t *info);
    EWLFreeRefFrm_func_t EWLFreeRefFrm_func = (EWLFreeRefFrm_func_t)dlsym(handle, "EWLFreeRefFrm");
    LIB_CALL_CHECK(EWLFreeRefFrm_func);
    return EWLFreeRefFrm_func(inst, info);
}

i32 EWLMallocLinear(const void *instance, u32 size, u32 alignment, EWLLinearMem_t *info)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*EWLMallocLinear_func_t)(const void *instance, u32 size, u32 alignment, EWLLinearMem_t *info);
    EWLMallocLinear_func_t EWLMallocLinear_func = (EWLMallocLinear_func_t)dlsym(handle, "EWLMallocLinear");
    LIB_CALL_CHECK(EWLMallocLinear_func);
    return EWLMallocLinear_func(instance, size, alignment, info);
}

void EWLFreeLinear(const void *inst, EWLLinearMem_t *info)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLFreeLinear_func_t)(const void *inst, EWLLinearMem_t *info);
    EWLFreeLinear_func_t EWLFreeLinear_func = (EWLFreeLinear_func_t)dlsym(handle, "EWLFreeLinear");
    LIB_CALL_CHECK(EWLFreeLinear_func);
    return EWLFreeLinear_func(inst, info);
}

void EWLDCacheRangeFlush(const void *instance, EWLLinearMem_t *info)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLDCacheRangeFlush_func_t)(const void *instance, EWLLinearMem_t *info);
    EWLDCacheRangeFlush_func_t EWLDCacheRangeFlush_func = (EWLDCacheRangeFlush_func_t)dlsym(handle, "EWLDCacheRangeFlush");
    LIB_CALL_CHECK(EWLDCacheRangeFlush_func);
    return EWLDCacheRangeFlush_func(instance, info);
}

void EWLDCacheRangeRefresh(const void *instance, EWLLinearMem_t *info)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLDCacheRangeRefresh_func_t)(const void *instance, EWLLinearMem_t *info);
    EWLDCacheRangeRefresh_func_t EWLDCacheRangeRefresh_func = (EWLDCacheRangeRefresh_func_t)dlsym(handle, "EWLDCacheRangeRefresh");
    LIB_CALL_CHECK(EWLDCacheRangeRefresh_func);
    return EWLDCacheRangeRefresh_func(instance, info);
}

void EWLWriteReg(const void *inst, u32 offset, u32 val)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLWriteReg_func_t)(const void *inst, u32 offset, u32 val);
    EWLWriteReg_func_t EWLWriteReg_func = (EWLWriteReg_func_t)dlsym(handle, "EWLWriteReg");
    LIB_CALL_CHECK(EWLWriteReg_func);
    return EWLWriteReg_func(inst, offset, val);
}

void EWLWriteBackReg(const void *inst, u32 offset, u32 val)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLWriteBackReg_func_t)(const void *inst, u32 offset, u32 val);
    EWLWriteBackReg_func_t EWLWriteBackReg_func = (EWLWriteBackReg_func_t)dlsym(handle, "EWLWriteBackReg");
    LIB_CALL_CHECK(EWLWriteBackReg_func);
    return EWLWriteBackReg_func(inst, offset, val);
}

void EWLWriteCoreReg(const void *inst, u32 offset, u32 val, u32 core_id)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLWriteCoreReg_func_t)(const void *inst, u32 offset, u32 val, u32 core_id);
    EWLWriteCoreReg_func_t EWLWriteCoreReg_func = (EWLWriteCoreReg_func_t)dlsym(handle, "EWLWriteCoreReg");
    LIB_CALL_CHECK(EWLWriteCoreReg_func);
    return EWLWriteCoreReg_func(inst, offset, val, core_id);
}

u32 EWLReadReg(const void *inst, u32 offset)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*EWLReadReg_func_t)(const void *inst, u32 offset);
    EWLReadReg_func_t EWLReadReg_func = (EWLReadReg_func_t)dlsym(handle, "EWLReadReg");
    LIB_CALL_CHECK(EWLReadReg_func);
    return EWLReadReg_func(inst, offset);
}

void EWLWriteRegAll(const void *inst, const u32 *table, u32 size)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLWriteRegAll_func_t)(const void *inst, const u32 *table, u32 size);
    EWLWriteRegAll_func_t EWLWriteRegAll_func = (EWLWriteRegAll_func_t)dlsym(handle, "EWLWriteRegAll");
    LIB_CALL_CHECK(EWLWriteRegAll_func);
    return EWLWriteRegAll_func(inst, table, size);
}

void EWLReadRegAll(const void *inst, u32 *table, u32 size)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLReadRegAll_func_t)(const void *inst, u32 *table, u32 size);
    EWLReadRegAll_func_t EWLReadRegAll_func = (EWLReadRegAll_func_t)dlsym(handle, "EWLReadRegAll");
    LIB_CALL_CHECK(EWLReadRegAll_func);
    return EWLReadRegAll_func(inst, table, size);
}

int EWLIoctlWriteRegs(int fd, u32 core_id, u32 offset, u32 size, u32 *val)
{
    LIB_CALL_CHECK(handle);
    typedef int (*EWLIoctlWriteRegs_func_t)(int fd, u32 core_id, u32 offset, u32 size, u32 *val);
    EWLIoctlWriteRegs_func_t EWLIoctlWriteRegs_func = (EWLIoctlWriteRegs_func_t)dlsym(handle, "EWLIoctlWriteRegs");
    LIB_CALL_CHECK(EWLIoctlWriteRegs_func);
    return EWLIoctlWriteRegs_func(fd, core_id, offset, size, val);
}

int EWLIoctlReadRegs(int fd, u32 core_id, u32 offset, u32 size, u32 *val)
{
    LIB_CALL_CHECK(handle);
    typedef int (*EWLIoctlReadRegs_func_t)(int fd, u32 core_id, u32 offset, u32 size, u32 *val);
    EWLIoctlReadRegs_func_t EWLIoctlReadRegs_func = (EWLIoctlReadRegs_func_t)dlsym(handle, "EWLIoctlReadRegs");
    LIB_CALL_CHECK(EWLIoctlReadRegs_func);
    return EWLIoctlReadRegs_func(fd, core_id, offset, size, val);
}

int EWLEnableHW(const void *inst, u32 offset, u32 val)
{
    LIB_CALL_CHECK(handle);
    typedef int (*EWLEnableHW_func_t)(const void *inst, u32 offset, u32 val);
    EWLEnableHW_func_t EWLEnableHW_func = (EWLEnableHW_func_t)dlsym(handle, "EWLEnableHW");
    LIB_CALL_CHECK(EWLEnableHW_func);
    return EWLEnableHW_func(inst, offset, val);
}

void EWLDisableHW(const void *inst, u32 offset, u32 val)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLDisableHW_func_t)(const void *inst, u32 offset, u32 val);
    EWLDisableHW_func_t EWLDisableHW_func = (EWLDisableHW_func_t)dlsym(handle, "EWLDisableHW");
    LIB_CALL_CHECK(EWLDisableHW_func);
    return EWLDisableHW_func(inst, offset, val);
}

i32 EWLWaitHwRdy(const void *inst, u32 *slicesReady, u32 totalsliceNumber, u32 *status_register)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*EWLWaitHwRdy_func_t)(const void *inst, u32 *slicesReady, u32 totalsliceNumber, u32 *status_register);
    EWLWaitHwRdy_func_t EWLWaitHwRdy_func = (EWLWaitHwRdy_func_t)dlsym(handle, "EWLWaitHwRdy");
    LIB_CALL_CHECK(EWLWaitHwRdy_func);
    return EWLWaitHwRdy_func(inst, slicesReady, totalsliceNumber, status_register);
}

void EWLfree(void *p)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLfree_func_t)(void *p);
    EWLfree_func_t EWLfree_func = (EWLfree_func_t)dlsym(handle, "EWLfree");
    LIB_CALL_CHECK(EWLfree_func);
    return EWLfree_func(p);
}

int EWLmemcmp(const void *s1, const void *s2, u32 n)
{
    LIB_CALL_CHECK(handle);
    typedef int (*EWLmemcmp_func_t)(const void *s1, const void *s2, u32 n);
    EWLmemcmp_func_t EWLmemcmp_func = (EWLmemcmp_func_t)dlsym(handle, "EWLmemcmp");
    LIB_CALL_CHECK(EWLmemcmp_func);
    return EWLmemcmp_func(s1, s2, n);
}

void EWLTraceProfile(const void *inst)
{
    LIB_CALL_CHECK(handle);
    typedef void (*EWLTraceProfile_func_t)(const void *inst);
    EWLTraceProfile_func_t EWLTraceProfile_func = (EWLTraceProfile_func_t)dlsym(handle, "EWLTraceProfile");
    LIB_CALL_CHECK(EWLTraceProfile_func);
    return EWLTraceProfile_func(inst);
}
VCEncApiVersion VCEncGetApiVersion(void)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncApiVersion (*VCEncGetApiVersion_func_t)(void);
    VCEncGetApiVersion_func_t VCEncGetApiVersion_func = (VCEncGetApiVersion_func_t)dlsym(handle, "VCEncGetApiVersion");
    LIB_CALL_CHECK(VCEncGetApiVersion_func);
    return VCEncGetApiVersion_func();
}

VCEncBuild VCEncGetBuild(u32 core_id)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncBuild (*VCEncGetBuild_func_t)(u32 core_id);
    VCEncGetBuild_func_t VCEncGetBuild_func = (VCEncGetBuild_func_t)dlsym(handle, "VCEncGetBuild");
    LIB_CALL_CHECK(VCEncGetBuild_func);
    return VCEncGetBuild_func(core_id);
}

u32 VCEncGetRoiMapVersion(u32 core_id)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*VCEncGetRoiMapVersion_func_t)(u32 core_id);
    VCEncGetRoiMapVersion_func_t VCEncGetRoiMapVersion_func = (VCEncGetRoiMapVersion_func_t)dlsym(handle, "VCEncGetRoiMapVersion");
    LIB_CALL_CHECK(VCEncGetRoiMapVersion_func);
    return VCEncGetRoiMapVersion_func(core_id);
}

u32 VCEncGetBitsPerPixel(VCEncPictureType type)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*VCEncGetBitsPerPixel_func_t)(VCEncPictureType type);
    VCEncGetBitsPerPixel_func_t VCEncGetBitsPerPixel_func = (VCEncGetBitsPerPixel_func_t)dlsym(handle, "VCEncGetBitsPerPixel");
    LIB_CALL_CHECK(VCEncGetBitsPerPixel_func);
    return VCEncGetBitsPerPixel_func(type);
}

u32 VCEncGetAlignedStride(int width, i32 input_format, u32 *luma_stride, u32 *chroma_stride, u32 input_alignment)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*VCEncGetAlignedStride_func_t)(int width, i32 input_format, u32 *luma_stride, u32 *chroma_stride, u32 input_alignment);
    VCEncGetAlignedStride_func_t VCEncGetAlignedStride_func = (VCEncGetAlignedStride_func_t)dlsym(handle, "VCEncGetAlignedStride");
    LIB_CALL_CHECK(VCEncGetAlignedStride_func);
    return VCEncGetAlignedStride_func(width, input_format, luma_stride, chroma_stride, input_alignment);
}

VCEncRet VCEncInit(const VCEncConfig *config, VCEncInst *instAddr)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncInit_func_t)(const VCEncConfig *config, VCEncInst *instAddr);
    VCEncInit_func_t VCEncInit_func = (VCEncInit_func_t)dlsym(handle, "VCEncInit");
    LIB_CALL_CHECK(VCEncInit_func);
    return VCEncInit_func(config, instAddr);
}

VCEncRet VCEncRelease(VCEncInst inst)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncRelease_func_t)(VCEncInst inst);
    VCEncRelease_func_t VCEncRelease_func = (VCEncRelease_func_t)dlsym(handle, "VCEncRelease");
    LIB_CALL_CHECK(VCEncRelease_func);
    return VCEncRelease_func(inst);
}

u32 VCEncGetPerformance(VCEncInst inst)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*VCEncGetPerformance_func_t)(VCEncInst inst);
    VCEncGetPerformance_func_t VCEncGetPerformance_func = (VCEncGetPerformance_func_t)dlsym(handle, "VCEncGetPerformance");
    LIB_CALL_CHECK(VCEncGetPerformance_func);
    return VCEncGetPerformance_func(inst);
}

VCEncRet VCEncSetCodingCtrl(VCEncInst instAddr, const VCEncCodingCtrl *pCodeParams)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncSetCodingCtrl_func_t)(VCEncInst instAddr, const VCEncCodingCtrl *pCodeParams);
    VCEncSetCodingCtrl_func_t VCEncSetCodingCtrl_func = (VCEncSetCodingCtrl_func_t)dlsym(handle, "VCEncSetCodingCtrl");
    LIB_CALL_CHECK(VCEncSetCodingCtrl_func);
    return VCEncSetCodingCtrl_func(instAddr, pCodeParams);
}

VCEncRet VCEncGetCodingCtrl(VCEncInst inst, VCEncCodingCtrl *pCodeParams)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncGetCodingCtrl_func_t)(VCEncInst inst, VCEncCodingCtrl * pCodeParams);
    VCEncGetCodingCtrl_func_t VCEncGetCodingCtrl_func = (VCEncGetCodingCtrl_func_t)dlsym(handle, "VCEncGetCodingCtrl");
    LIB_CALL_CHECK(VCEncGetCodingCtrl_func);
    return VCEncGetCodingCtrl_func(inst, pCodeParams);
}

VCEncRet VCEncSetRateCtrl(VCEncInst inst, const VCEncRateCtrl *pRateCtrl)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncSetRateCtrl_func_t)(VCEncInst inst, const VCEncRateCtrl *pRateCtrl);
    VCEncSetRateCtrl_func_t VCEncSetRateCtrl_func = (VCEncSetRateCtrl_func_t)dlsym(handle, "VCEncSetRateCtrl");
    LIB_CALL_CHECK(VCEncSetRateCtrl_func);
    return VCEncSetRateCtrl_func(inst, pRateCtrl);
}

VCEncRet VCEncGetRateCtrl(VCEncInst inst, VCEncRateCtrl *pRateCtrl)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncGetRateCtrl_func_t)(VCEncInst inst, VCEncRateCtrl * pRateCtrl);
    VCEncGetRateCtrl_func_t VCEncGetRateCtrl_func = (VCEncGetRateCtrl_func_t)dlsym(handle, "VCEncGetRateCtrl");
    LIB_CALL_CHECK(VCEncGetRateCtrl_func);
    return VCEncGetRateCtrl_func(inst, pRateCtrl);
}

VCEncRet VCEncSetPreProcessing(VCEncInst inst, const VCEncPreProcessingCfg *pPreProcCfg)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncSetPreProcessing_func_t)(VCEncInst inst, const VCEncPreProcessingCfg *pPreProcCfg);
    VCEncSetPreProcessing_func_t VCEncSetPreProcessing_func = (VCEncSetPreProcessing_func_t)dlsym(handle, "VCEncSetPreProcessing");
    LIB_CALL_CHECK(VCEncSetPreProcessing_func);
    return VCEncSetPreProcessing_func(inst, pPreProcCfg);
}

VCEncRet VCEncGetPreProcessing(VCEncInst inst, VCEncPreProcessingCfg *pPreProcCfg)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncGetPreProcessing_func_t)(VCEncInst inst, VCEncPreProcessingCfg * pPreProcCfg);
    VCEncGetPreProcessing_func_t VCEncGetPreProcessing_func = (VCEncGetPreProcessing_func_t)dlsym(handle, "VCEncGetPreProcessing");
    LIB_CALL_CHECK(VCEncGetPreProcessing_func);
    return VCEncGetPreProcessing_func(inst, pPreProcCfg);
}

VCEncRet VCEncSetSeiUserData(VCEncInst inst, const u8 *pUserData, u32 userDataSize)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncSetSeiUserData_func_t)(VCEncInst inst, const u8 *pUserData, u32 userDataSize);
    VCEncSetSeiUserData_func_t VCEncSetSeiUserData_func = (VCEncSetSeiUserData_func_t)dlsym(handle, "VCEncSetSeiUserData");
    LIB_CALL_CHECK(VCEncSetSeiUserData_func);
    return VCEncSetSeiUserData_func(inst, pUserData, userDataSize);
}

VCEncRet VCEncStrmStart(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncStrmStart_func_t)(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut);
    VCEncStrmStart_func_t VCEncStrmStart_func = (VCEncStrmStart_func_t)dlsym(handle, "VCEncStrmStart");
    LIB_CALL_CHECK(VCEncStrmStart_func);
    return VCEncStrmStart_func(inst, pEncIn, pEncOut);
}

VCEncRet VCEncStrmEncode(VCEncInst inst, const VCEncIn *pEncIn,
                         VCEncOut *pEncOut,
                         VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                         void *pAppData)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncStrmEncode_func_t)(VCEncInst inst, const VCEncIn *pEncIn,
                                               VCEncOut *pEncOut,
                                               VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                                               void *pAppData);
    VCEncStrmEncode_func_t VCEncStrmEncode_func = (VCEncStrmEncode_func_t)dlsym(handle, "VCEncStrmEncode");
    LIB_CALL_CHECK(VCEncStrmEncode_func);
    return VCEncStrmEncode_func(inst, pEncIn, pEncOut, sliceReadyCbFunc, pAppData);
}

VCEncRet VCEncStrmEncodeExt(VCEncInst inst, const VCEncIn *pEncIn,
                            const VCEncExtParaIn *pEncExtParaIn,
                            VCEncOut *pEncOut,
                            VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                            void *pAppData, i32 useExtFlag)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncStrmEncodeExt_func_t)(VCEncInst inst, const VCEncIn *pEncIn,
                                                  const VCEncExtParaIn *pEncExtParaIn,
                                                  VCEncOut *pEncOut,
                                                  VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                                                  void *pAppData, i32 useExtFlag);
    VCEncStrmEncodeExt_func_t VCEncStrmEncodeExt_func = (VCEncStrmEncodeExt_func_t)dlsym(handle, "VCEncStrmEncodeExt");
    LIB_CALL_CHECK(VCEncStrmEncodeExt_func);
    return VCEncStrmEncodeExt_func(inst, pEncIn, pEncExtParaIn, pEncOut, sliceReadyCbFunc, pAppData, useExtFlag);
}

VCEncRet VCEncStrmEnd(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncStrmEnd_func_t)(VCEncInst inst, const VCEncIn *pEncIn, VCEncOut *pEncOut);
    VCEncStrmEnd_func_t VCEncStrmEnd_func = (VCEncStrmEnd_func_t)dlsym(handle, "VCEncStrmEnd");
    LIB_CALL_CHECK(VCEncStrmEnd_func);
    return VCEncStrmEnd_func(inst, pEncIn, pEncOut);
}

VCEncRet VCEncFlush(VCEncInst inst, const VCEncIn *pEncIn,
                    VCEncOut *pEncOut,
                    VCEncSliceReadyCallBackFunc sliceReadyCbFunc)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncFlush_func_t)(VCEncInst inst, const VCEncIn *pEncIn,
                                          VCEncOut *pEncOut,
                                          VCEncSliceReadyCallBackFunc sliceReadyCbFunc);
    VCEncFlush_func_t VCEncFlush_func = (VCEncFlush_func_t)dlsym(handle, "VCEncFlush");
    LIB_CALL_CHECK(VCEncFlush_func);
    return VCEncFlush_func(inst, pEncIn, pEncOut, sliceReadyCbFunc);
}

VCEncRet VCEncSetTestId(VCEncInst inst, u32 testId)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncSetTestId_func_t)(VCEncInst inst, u32 testId);
    VCEncSetTestId_func_t VCEncSetTestId_func = (VCEncSetTestId_func_t)dlsym(handle, "VCEncSetTestId");
    LIB_CALL_CHECK(VCEncSetTestId_func);
    return VCEncSetTestId_func(inst, testId);
}

VCEncRet VCEncCreateNewPPS(VCEncInst inst, const VCEncPPSCfg *pPPSCfg, i32 *newPPSId)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncCreateNewPPS_func_t)(VCEncInst inst, const VCEncPPSCfg *pPPSCfg, i32 *newPPSId);
    VCEncCreateNewPPS_func_t VCEncCreateNewPPS_func = (VCEncCreateNewPPS_func_t)dlsym(handle, "VCEncCreateNewPPS");
    LIB_CALL_CHECK(VCEncCreateNewPPS_func);
    return VCEncCreateNewPPS_func(inst, pPPSCfg, newPPSId);
}

VCEncRet VCEncModifyOldPPS(VCEncInst inst, const VCEncPPSCfg *pPPSCfg, i32 ppsId)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncModifyOldPPS_func_t)(VCEncInst inst, const VCEncPPSCfg *pPPSCfg, i32 ppsId);
    VCEncModifyOldPPS_func_t VCEncModifyOldPPS_func = (VCEncModifyOldPPS_func_t)dlsym(handle, "VCEncModifyOldPPS");
    LIB_CALL_CHECK(VCEncModifyOldPPS_func);
    return VCEncModifyOldPPS_func(inst, pPPSCfg, ppsId);
}

VCEncRet VCEncGetPPSData(VCEncInst inst, VCEncPPSCfg *pPPSCfg, i32 ppsId)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncGetPPSData_func_t)(VCEncInst inst, VCEncPPSCfg * pPPSCfg, i32 ppsId);
    VCEncGetPPSData_func_t VCEncGetPPSData_func = (VCEncGetPPSData_func_t)dlsym(handle, "VCEncGetPPSData");
    LIB_CALL_CHECK(VCEncGetPPSData_func);
    return VCEncGetPPSData_func(inst, pPPSCfg, ppsId);
}

VCEncRet VCEncActiveAnotherPPS(VCEncInst inst, i32 ppsId)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncActiveAnotherPPS_func_t)(VCEncInst inst, i32 ppsId);
    VCEncActiveAnotherPPS_func_t VCEncActiveAnotherPPS_func = (VCEncActiveAnotherPPS_func_t)dlsym(handle, "VCEncActiveAnotherPPS");
    LIB_CALL_CHECK(VCEncActiveAnotherPPS_func);
    return VCEncActiveAnotherPPS_func(inst, ppsId);
}

VCEncRet VCEncGetActivePPSId(VCEncInst inst, i32 *ppsId)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncGetActivePPSId_func_t)(VCEncInst inst, i32 * ppsId);
    VCEncGetActivePPSId_func_t VCEncGetActivePPSId_func = (VCEncGetActivePPSId_func_t)dlsym(handle, "VCEncGetActivePPSId");
    LIB_CALL_CHECK(VCEncGetActivePPSId_func);
    return VCEncGetActivePPSId_func(inst, ppsId);
}

VCEncRet VCEncSetInputMBLines(VCEncInst inst, u32 lines)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncSetInputMBLines_func_t)(VCEncInst inst, u32 lines);
    VCEncSetInputMBLines_func_t VCEncSetInputMBLines_func = (VCEncSetInputMBLines_func_t)dlsym(handle, "VCEncSetInputMBLines");
    LIB_CALL_CHECK(VCEncSetInputMBLines_func);
    return VCEncSetInputMBLines_func(inst, lines);
}

u32 VCEncGetEncodedMbLines(VCEncInst inst)
{
    LIB_CALL_CHECK(handle);
    typedef u32 (*VCEncGetEncodedMbLines_func_t)(VCEncInst inst);
    VCEncGetEncodedMbLines_func_t VCEncGetEncodedMbLines_func = (VCEncGetEncodedMbLines_func_t)dlsym(handle, "VCEncGetEncodedMbLines");
    LIB_CALL_CHECK(VCEncGetEncodedMbLines_func);
    return VCEncGetEncodedMbLines_func(inst);
}

VCEncRet VCEncGetCuInfo(VCEncInst inst, VCEncCuOutData *pEncCuOutData,
                        VCEncCuInfo *pEncCuInfo, u32 ctuNum, u32 cuNum)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncRet (*VCEncGetCuInfo_func_t)(VCEncInst inst, VCEncCuOutData * pEncCuOutData,
                                              VCEncCuInfo * pEncCuInfo, u32 ctuNum, u32 cuNum);
    VCEncGetCuInfo_func_t VCEncGetCuInfo_func = (VCEncGetCuInfo_func_t)dlsym(handle, "VCEncGetCuInfo");
    LIB_CALL_CHECK(VCEncGetCuInfo_func);
    return VCEncGetCuInfo_func(inst, pEncCuOutData, pEncCuInfo, ctuNum, cuNum);
}

VCEncPictureCodingType VCEncFindNextPic (VCEncInst inst, VCEncIn *encIn, i32 nextGopSize, const u8 *gopCfgOffset, bool forceIDR)
{
    LIB_CALL_CHECK(handle);
    typedef VCEncPictureCodingType (*VCEncFindNextPic_func_t)(VCEncInst inst, VCEncIn *encIn, i32 nextGopSize, const u8 *gopCfgOffset, bool forceIDR);
    VCEncFindNextPic_func_t VCEncFindNextPic_func = (VCEncFindNextPic_func_t)dlsym(handle, "VCEncFindNextPic");
    LIB_CALL_CHECK(VCEncFindNextPic_func);
    return VCEncFindNextPic_func(inst, encIn, nextGopSize, gopCfgOffset, forceIDR);
}

void VCEncTrace(const char *msg)
{
    LIB_CALL_CHECK(handle);
    typedef void (*VCEncTrace_func_t)(const char *msg);
    VCEncTrace_func_t VCEncTrace_func = (VCEncTrace_func_t)dlsym(handle, "VCEncTrace");
    LIB_CALL_CHECK(VCEncTrace_func);
    return VCEncTrace_func(msg);
}

void VCEncTraceProfile(VCEncInst inst)
{
    LIB_CALL_CHECK(handle);
    typedef void (*VCEncTraceProfile_func_t)(VCEncInst inst);
    VCEncTraceProfile_func_t VCEncTraceProfile_func = (VCEncTraceProfile_func_t)dlsym(handle, "VCEncTraceProfile");
    LIB_CALL_CHECK(VCEncTraceProfile_func);
    return VCEncTraceProfile_func(inst);
}

EWLHwConfig_t VCEncGetAsicConfig(VCEncVideoCodecFormat codecFormat)
{
    LIB_CALL_CHECK(handle);
    typedef EWLHwConfig_t (*VCEncGetAsicConfig_func_t)(VCEncVideoCodecFormat codecFormat);
    VCEncGetAsicConfig_func_t VCEncGetAsicConfig_func = (VCEncGetAsicConfig_func_t)dlsym(handle, "VCEncGetAsicConfig");
    LIB_CALL_CHECK(VCEncGetAsicConfig_func);
    return VCEncGetAsicConfig_func(codecFormat);
}

i32 VCEncGetPass1UpdatedGopSize(VCEncInst inst)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*VCEncGetPass1UpdatedGopSize_func_t)(VCEncInst inst);
    VCEncGetPass1UpdatedGopSize_func_t VCEncGetPass1UpdatedGopSize_func = (VCEncGetPass1UpdatedGopSize_func_t)dlsym(handle, "VCEncGetPass1UpdatedGopSize");
    LIB_CALL_CHECK(VCEncGetPass1UpdatedGopSize_func);
    return VCEncGetPass1UpdatedGopSize_func(inst);
}

i32 VCEncSetVuiColorDescription(VCEncInst inst, u32 vuiVideoSignalTypePresentFlag, u32 vuiVideoFormat,
                                u32 vuiColorDescripPresentFlag, u32 vuiColorPrimaries, u32 vuiTransferCharacteristics, u32 vuiMatrixCoefficients)
{
    LIB_CALL_CHECK(handle);
    typedef i32 (*VCEncSetVuiColorDescription_func_t)(VCEncInst inst, u32 vuiVideoSignalTypePresentFlag, u32 vuiVideoFormat,
                                                      u32 vuiColorDescripPresentFlag, u32 vuiColorPrimaries, u32 vuiTransferCharacteristics, u32 vuiMatrixCoefficients);
    VCEncSetVuiColorDescription_func_t VCEncSetVuiColorDescription_func = (VCEncSetVuiColorDescription_func_t)dlsym(handle, "VCEncSetVuiColorDescription");
    LIB_CALL_CHECK(VCEncSetVuiColorDescription_func);
    return VCEncSetVuiColorDescription_func(inst, vuiVideoSignalTypePresentFlag, vuiVideoFormat, vuiColorDescripPresentFlag, vuiColorPrimaries, vuiTransferCharacteristics, vuiMatrixCoefficients);
}