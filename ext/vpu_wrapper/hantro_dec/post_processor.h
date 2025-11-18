/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef HANTRO_POST_PROCESSOR_H
#define HANTRO_POST_PROCESSOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "codec.h"

    typedef enum PP_STATE
    {
        PP_DISABLED,
        PP_PIPELINE,
        PP_STANDALONE
    } PP_STATE;

#ifdef ENABLE_PP
#include "ppapi.h"
#include "ppinternal.h"

    typedef enum PP_TRANSFORMS
    {
        PPTR_NONE = 0,
        PPTR_SCALE = 1,
        PPTR_CROP = 2,
        PPTR_FORMAT = 4,
        PPTR_ROTATE = 8,
        PPTR_MIRROR = 16,
        PPTR_MASK1 = 32,
        PPTR_MASK2 = 64,
        PPTR_DEINTERLACE = 128,
        PPTR_ARG_ERROR = 256
    } PP_TRANSFORMS;

// create pp instance & get default config
    PPResult HantroHwDecOmx_pp_init(PPInst * pp_instance, PPConfig * pp_config);

// destroy pp instance
    void HantroHwDecOmx_pp_destroy(PPInst * pp_instance);

// setup necessary transformations
    PP_TRANSFORMS HantroHwDecOmx_pp_set_output(PPConfig * pp_config,
                                               PP_ARGS * args,
                                               OMX_BOOL bDeIntEna);

// setup input image params
    void HantroHwDecOmx_pp_set_info(PPConfig * pp_config, STREAM_INFO * info,
                                    PP_TRANSFORMS state);

// setup input image (standalone mode only)
    void HantroHwDecOmx_pp_set_input_buffer(PPConfig * pp_config, OSAL_BUS_WIDTH bus_addr);

    void HantroHwDecOmx_pp_set_input_buffer_planes(PPConfig * pp_config,
                                                   OSAL_BUS_WIDTH bus_addr_y,
                                                   OSAL_BUS_WIDTH bus_addr_cbcr);

// setup output image
    void HantroHwDecOmx_pp_set_output_buffer(PPConfig * pp_config,
                                             FRAME * frame);

// set configuration to pp
    PPResult HantroHwDecOmx_pp_set(PPInst pp_instance, PPConfig * pp_config);

// get result (standalone mode only)
    PPResult HantroHwDecOmx_pp_execute(PPInst pp_instance);

// enable pipeline mode
    PPResult HantroHwDecOmx_pp_pipeline_enable(PPInst pp_instance,
                                               const void *codec_instance,
                                               u32 type);

// disable pipeline mode
    void HantroHwDecOmx_pp_pipeline_disable(PPInst pp_instance,
                                            const void *codec_instance);

    OMX_S32 HantroHwDecOmx_pp_get_framesize(const PPConfig * pp_config);
#endif

#ifdef __cplusplus
}
#endif
#endif                       // HANTRO_POST_PROCESSOR_H
