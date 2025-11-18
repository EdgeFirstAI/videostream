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

#ifndef HANTRO_MSGQUE_H
#define HANTRO_MSGQUE_H

#include "OMX_Core.h"
#include "OMX_Types.h"
#include "OSAL.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msg_node msg_node;

struct msg_node {
    msg_node* next;
    msg_node* prev;
    OMX_PTR   data;
};

typedef struct msgque {
    msg_node*      head;
    msg_node*      tail;
    OMX_U32        size;
    OMX_HANDLETYPE mutex;
    OMX_HANDLETYPE event;
} msgque;

// Initialize a new message queue instance
OMX_ERRORTYPE
HantroOmx_msgque_init(OMX_IN msgque* q);

// Destroy the message queue instance, free allocated resources
void
HantroOmx_msgque_destroy(OMX_IN msgque* q);

// Push a new message at the end of the queue.
// Function provides commit/rollback semantics.
OMX_ERRORTYPE
HantroOmx_msgque_push_back(OMX_IN msgque* q, OMX_IN OMX_PTR ptr);

// Get a message from the front, returns always immediately but
// ptr will point to NULL if the queue is empty.
// Function provides commit/rollback semantics.
OMX_ERRORTYPE
HantroOmx_msgque_get_front(OMX_IN msgque* q, OMX_OUT OMX_PTR* ptr);

// Get current queue size
OMX_ERRORTYPE
HantroOmx_msgque_get_size(OMX_IN msgque* q, OMX_OUT OMX_U32* size);

#ifdef __cplusplus
}
#endif
#endif // HANTRO_MSGQUE_H
