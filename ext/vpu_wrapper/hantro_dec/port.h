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

#ifndef HANTRO_PORT_H
#define HANTRO_PORT_H

#include "OMX_Component.h"
#include "OMX_Types.h"
#include "OMX_Video.h"
#include "OSAL.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_FLAG_IN_USE 0x1
#define BUFFER_FLAG_MY_BUFFER 0x2
#define BUFFER_FLAG_IS_TUNNELED 0x4
#define BUFFER_FLAG_MARK 0x8
#define BUFFER_FLAG_ANDROID_NATIVE_BUFFER 0x10
#define BUFFER_FLAG_EXT_ALLOC 0x20

typedef struct BUFFER {
    OMX_BUFFERHEADERTYPE* header;
    OMX_BUFFERHEADERTYPE  headerdata;
    OMX_U32               flags;
    OMX_U32               allocsize;
    OSAL_BUS_WIDTH        bus_address;
    OMX_U8*               bus_data;
    OMX_S32               ion_fd;
#ifdef USE_ANDROID_NATIVE_BUFFER
    void* native_buffer_hdl;
#endif
} BUFFER;

typedef struct BUFFERLIST {
    BUFFER** list;
    OMX_U32  size; // list size
    OMX_U32  capacity;
} BUFFERLIST;

OMX_ERRORTYPE
HantroOmx_bufferlist_init(BUFFERLIST* list, OMX_U32 size);
OMX_ERRORTYPE
HantroOmx_bufferlist_reserve(BUFFERLIST* list, OMX_U32 newsize);
void
HantroOmx_bufferlist_destroy(BUFFERLIST* list);
OMX_U32
HantroOmx_bufferlist_get_size(BUFFERLIST* list);
OMX_U32
HantroOmx_bufferlist_get_capacity(BUFFERLIST* list);
BUFFER**
HantroOmx_bufferlist_at(BUFFERLIST* list, OMX_U32 i);
void
HantroOmx_bufferlist_remove(BUFFERLIST* list, OMX_U32 i);
void
HantroOmx_bufferlist_clear(BUFFERLIST* list);
OMX_BOOL
HantroOmx_bufferlist_push_back(BUFFERLIST* list, BUFFER* buff);

typedef struct PORT {
    OMX_PARAM_PORTDEFINITIONTYPE def; // OMX port definition
    OMX_TUNNELSETUPTYPE          tunnel;
    OMX_HANDLETYPE               tunnelcomp; // handle to the tunneled component
    OMX_U32        tunnelport;  // port index of the tunneled components port
    BUFFERLIST     buffers;     // buffers for this port
    BUFFERLIST     bufferqueue; // buffers queued up for processing
    OMX_HANDLETYPE buffermutex; // mutex to protect the buffer queue
    OMX_HANDLETYPE bufferevent; // event object for buffer queue
    OMX_HANDLETYPE bufferRdy;   // event object for buffer is ready on the port
    OMX_BOOL useAndroidNativeBuffer; // android stagefright flag for buffer type
} PORT;

// nBufferCountMin is a read-only field that specifies the minimum number of
// buffers that the port requires. The component shall define this non-zero
// default value.

// nBufferCountActual represents the number of buffers that are required on
// this port before it is populated, as indicated by the bPopulated field of
// this structure. The component shall set a default value no less than
// nBufferCountMin for this field.

// nBufferSize is a read-only field that specifies the minimum size in bytes for
// buffers that are allocated for this port. .
OMX_ERRORTYPE
HantroOmx_port_init(PORT*   p,
                    OMX_U32 nBufferCountMin,
                    OMX_U32 nBufferCountActual,
                    OMX_U32 nBuffers,
                    OMX_U32 buffersize);

void
HantroOmx_port_destroy(PORT* p);

OMX_BOOL
HantroOmx_port_is_allocated(PORT* p);

OMX_BOOL
HantroOmx_port_is_ready(PORT* p);

OMX_BOOL
HantroOmx_port_is_enabled(PORT* p);

// Return true if port has allocated buffers, otherwise false.
OMX_BOOL
HantroOmx_port_has_buffers(PORT* p);

OMX_BOOL
HantroOmx_port_is_supplier(PORT* p);
OMX_BOOL
HantroOmx_port_is_tunneled(PORT* p);

OMX_BOOL
HantroOmx_port_has_all_supplied_buffers(PORT* p);

void
HantroOmx_port_setup_tunnel(PORT*                  p,
                            OMX_HANDLETYPE         comp,
                            OMX_U32                port,
                            OMX_BUFFERSUPPLIERTYPE type);

BUFFER*
HantroOmx_port_find_buffer(PORT* p, OMX_BUFFERHEADERTYPE* header);

// Try to allocate next available buffer from the array of buffers associated
// with with the port. The finding is done by looking at the associated buffer
// flags and checking the BUFFER_FLAG_IN_USE flag.
//
// Returns OMX_TRUE if next buffer could be found. Otherwise OMX_FALSE, which
// means that all buffer headers are in use.
OMX_BOOL
HantroOmx_port_allocate_next_buffer(PORT* p, BUFFER** buff);

OMX_BOOL
HantroOmx_port_release_buffer(PORT* p, BUFFER* buff);

OMX_BOOL
HantroOmx_port_release_all_allocated(PORT* p);

// Return how many buffers are allocated for this port.
OMX_U32
HantroOmx_port_buffer_count(PORT* p);

// Get an allocated buffer.
OMX_BOOL
HantroOmx_port_get_allocated_buffer_at(PORT* p, BUFFER** buff, OMX_U32 i);

// queue functions
// Push next buffer into the port's buffer queue.
OMX_ERRORTYPE
HantroOmx_port_push_buffer(PORT* p, BUFFER* buff);

// Get next buffer from the port's buffer queue.
OMX_BOOL
HantroOmx_port_get_buffer(PORT* p, BUFFER** buff);

// Get a buffer at a certain location from port's buffer queue
OMX_BOOL
HantroOmx_port_get_buffer_at(PORT* P, BUFFER** buff, OMX_U32 i);

// Pop off the first buffer from the port's buffer queue
OMX_BOOL
HantroOmx_port_pop_buffer(PORT* p);

// Pop a buffer at a certain location from port's buffer queue
OMX_BOOL
HantroOmx_port_pop_buffer_at(PORT* p, OMX_U32 i);

// Lock the buffer queue
OMX_ERRORTYPE
HantroOmx_port_lock_buffers(PORT* p);

// Unlock the buffer queue
OMX_ERRORTYPE
HantroOmx_port_unlock_buffers(PORT* p);

// Return how many buffers are queued for this port.
OMX_U32
HantroOmx_port_buffer_queue_count(PORT* p);

void
HantroOmx_port_buffer_queue_clear(PORT* p);

#ifdef __cplusplus
}
#endif
#endif // HANTRO_PORT_H
