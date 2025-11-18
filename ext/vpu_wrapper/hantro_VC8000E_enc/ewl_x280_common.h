/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Abstract : H2 Encoder Wrapper Layer for OS services
--
------------------------------------------------------------------------------*/
#ifndef __EWL_X280_COMMON_H__
#define __EWL_X280_COMMON_H__

#include <stdio.h>
#include <signal.h>
#include <semaphore.h>
#include <queue.h>

extern FILE *fEwl;

/* Macro for debug printing */
#undef PTRACE
#ifdef TRACE_EWL
#   include <stdio.h>
#   define PTRACE(...) if (fEwl) {fprintf(fEwl,"%s:%d:",__FILE__,__LINE__);fprintf(fEwl,__VA_ARGS__);}
#else
#   define PTRACE(...)  /* no trace */
#endif

#ifdef USE_ION
#include <linux/version.h>
#undef MEMALLOC_MODULE_PATH
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
#define MEMALLOC_MODULE_PATH        "/dev/ion"
#else
#define MEMALLOC_MODULE_PATH        "/dev/dma_heap/linux,cma-uncached"
#endif
#endif

#ifdef ENABLE_DMABUF_HEAP
#undef MEMALLOC_MODULE_PATH
#define MEMALLOC_MODULE_PATH "/dev/dma_heap/reserved-uncached"
#endif

/* the encoder device driver nod */
#ifndef MEMALLOC_MODULE_PATH
#define MEMALLOC_MODULE_PATH        "/tmp/dev/memalloc"
#endif

#ifndef ENC_MODULE_PATH
#define ENC_MODULE_PATH             "/tmp/dev/hx280"
#endif

#ifndef SDRAM_LM_BASE
#define SDRAM_LM_BASE               0x00000000
#endif

typedef struct
{
 i32 core_id; //physical core id
 u32 regSize;             /* IO mem size */
 u32 regBase;
 volatile u32 *pRegBase;  /* IO mem base */
}regMapping;

typedef struct {
  struct node *next;
  int core_id;
} EWLWorker;
#define FIRST_CORE(inst) (((EWLWorker *)(inst->workers.tail))->core_id)
#define LAST_CORE(inst)  (((EWLWorker *)(inst->workers.head))->core_id)

/* EWL internal information for Linux */
typedef struct
{
    u32 clientType;
#ifdef USE_VSI_ENV
    int fd_mem;              /* /dev/mem */
#endif
    int fd_enc;              /* /dev/hx280 */
    int fd_memalloc;         /* /dev/memalloc */
    regMapping reg; //register for reserved cores
    regMapping *reg_all_cores;
    u32 coreAmout;
    u32 performance;
    struct queue freelist;
    struct queue workers;

    /* loopback line buffer in on-chip SRAM*/
    u32 lineBufSramBase;  /* bus addr */
    volatile u32 *pLineBufSram; /* virtual addr */
    u32 lineBufSramSize;
    u32 mmuEnable;
} hx280ewl_t;

#endif /* __EWLX280_COMMON_H__ */
