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

#ifndef HANTRO_UTIL_H
#define HANTRO_UTIL_H

#include "OMX_Component.h"
#include "OMX_Core.h"
#include "OMX_Types.h"

// Qm.n conversion macros
#define Q16_FLOAT(a) ((float) (a) / 65536.0)
#define FLOAT_Q16(a) ((OMX_U32) ((float) (a) * 65536.0))
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define UNUSED_PARAMETER(p) (void) (p)

#ifdef __cplusplus
extern "C" {
#endif

const char*
HantroOmx_str_omx_state(OMX_STATETYPE s);
const char*
HantroOmx_str_omx_err(OMX_ERRORTYPE e);
const char*
HantroOmx_str_omx_cmd(OMX_COMMANDTYPE c);
const char*
HantroOmx_str_omx_event(OMX_EVENTTYPE e);
const char*
HantroOmx_str_omx_index(OMX_INDEXTYPE i);
const char*
HantroOmx_str_omx_color(OMX_COLOR_FORMATTYPE f);
const char*
HantroOmx_str_omx_supplier(OMX_BUFFERSUPPLIERTYPE bst);

OMX_U32
HantroOmx_make_int_ver(OMX_U8 major, OMX_U8 minor);

#ifdef __cplusplus
}
#endif
#endif // HANTRO_UTIL_H
