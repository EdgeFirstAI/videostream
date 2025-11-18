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

#ifndef HANTRO_DEBUG_MACROS_H
#define HANTRO_DEBUG_MACROS_H

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#ifndef __USE_GNU
#    define __USE_GNU
#endif

#ifndef ANDROID
#include <execinfo.h>
#include <link.h>
#endif
#include <dlfcn.h>
#include <elf.h>

/*****************
 ** DEBUG FLAGS **
 *****************/

//enable debugging 
#undef DEBUG

//log into file
//#define LOG_IN_FILE

/*****************/

#ifdef DEBUG
#ifdef ANDROID
#define TRACE_FILE "/data/local/trace.log"
#else
#define TRACE_FILE "trace.log"
#endif

#ifdef LOG_IN_FILE
#define TRACE(...)                                              \
    {                                                           \
        FILE *trace_f;                                          \
        trace_f = fopen(TRACE_FILE, "a");                       \
        (void*)fprintf(trace_f, "%-20s:", __FILE__);            \
        (void*)fprintf(trace_f, "%04d: ", __LINE__);            \
        (void*)fprintf(trace_f, "%-40s: ", __FUNCTION__);       \
        (void*)fprintf(trace_f, __VA_ARGS__);                   \
        fclose(trace_f);                                        \
    }
#else
#define TRACE(...)                                              \
        printf("%-20s:",__FILE__);                              \
        printf("%04d: ", __LINE__);                             \
        printf("%-40s: ", __FUNCTION__);                        \
        printf(__VA_ARGS__);
#endif

#ifdef LOG_IN_FILE
#define CALLSTACK                                               \
    {                                                           \
        Dl_info dli0;                                           \
        Dl_info dli1;                                           \
        Dl_info dli2;                                           \
        FILE *trace_f;                                          \
        dladdr(__builtin_return_address(0), &dli0);             \
        dladdr(__builtin_return_address(1), &dli1);             \
        dladdr(__builtin_return_address(2), &dli2);             \
        trace_f = fopen(TRACE_FILE, "a");                       \
        (void*)fprintf(trace_f, "%-20s:", __FILE__);            \
        (void*)fprintf(trace_f, "%04d: ", __LINE__);            \
        (void*)fprintf(trace_f, "%-40s: ", __FUNCTION__);       \
        (void*)fprintf(trace_f, "CALLSTACK:  %-40s => %-40s => %-40s\n", dli2.dli_sname, dli1.dli_sname, dli0.dli_sname);                   \
        fclose(trace_f);                                        \
    }
#else
#define CALLSTACK                                               \
{                                                               \
        Dl_info dli0;                                           \
        Dl_info dli1;                                           \
        Dl_info dli2;                                           \
        dladdr(__builtin_return_address(0), &dli0);             \
        dladdr(__builtin_return_address(1), &dli1);             \
        dladdr(__builtin_return_address(2), &dli2);             \
        printf("%-20s:",__FILE__);                              \
        printf("%04d: ", __LINE__);                             \
        printf("%-40s: ", __FUNCTION__);                        \
        printf("CALLSTACK:  %-40s => %-40s => %-40s\n", dli2.dli_sname, dli1.dli_sname, dli0.dli_sname);                                    \
}
#endif


#ifdef LOG_IN_FILE
#define MEMDUMP(addr, size)  \
    {                                                                                                                                           \
        FILE *trace_f;                                                                                                                          \
        char trace_str[256];                                                                                                                    \
        trace_f = fopen(TRACE_FILE, "a");                                                                                                       \
        (void*)fputs("\n******************************* Start of memdump *******************************\n", trace_f);                          \
        int d_i;                                                                                                                                \
        unsigned char* d_p = (unsigned char*)addr;                                                                                              \
        for(d_i = 0; d_i < size; d_i+=16, d_p+=16)                                                                                              \
        {                                                                                                                                       \
            sprintf(trace_str,                                                                                                                  \
                    "%p: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ; %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", \
                    d_p,                                                                                                                        \
                    *(d_p), *(d_p+1), *(d_p+2), *(d_p+3), *(d_p+4), *(d_p+5), *(d_p+6), *(d_p+7),                                               \
                    *(d_p+8), *(d_p+9), *(d_p+10), *(d_p+11), *(d_p+12), *(d_p+13), *(d_p+14), *(d_p+15),                                       \
                    (*(d_p) > 31 && *(d_p) < 127) ? *(d_p) : '.', (*(d_p+1) > 31 && *(d_p+1) < 127) ? *(d_p+1) : '.',                           \
                    (*(d_p+2) > 31 && *(d_p+2) < 127) ? *(d_p+2) : '.', (*(d_p+3) > 31 && *(d_p+3) < 127) ? *(d_p+3) : '.',                     \
                    (*(d_p+4) > 31 && *(d_p+4) < 127) ? *(d_p+4) : '.', (*(d_p+5) > 31 && *(d_p+5) < 127) ? *(d_p+5) : '.',                     \
                    (*(d_p+6) > 31 && *(d_p+6) < 127) ? *(d_p+6) : '.', (*(d_p+7) > 31 && *(d_p+7) < 127) ? *(d_p+7) : '.',                     \
                    (*(d_p+8) > 31 && *(d_p+8) < 127) ? *(d_p+8) : '.', (*(d_p+9) > 31 && *(d_p+9) < 127) ? *(d_p+9) : '.',                     \
                    (*(d_p+10) > 31 && *(d_p+10) < 127) ? *(d_p+10) : '.', (*(d_p+11) > 31 && *(d_p+11) < 127) ? *(d_p+11) : '.',               \
                    (*(d_p+12) > 31 && *(d_p+12) < 127) ? *(d_p+12) : '.', (*(d_p+13) > 31 && *(d_p+13) < 127) ? *(d_p+13) : '.',               \
                    (*(d_p+14) > 31 && *(d_p+14) < 127) ? *(d_p+14) : '.', (*(d_p+15) > 31 && *(d_p+15) < 127) ? *(d_p+15) : '.');              \
            fputs(trace_str, trace_f);                                                                                                          \
        }                                                                                                                                       \
        fputs("******************************** End of memdump ********************************\n\n", trace_f);                                 \
        fclose(trace_f);                                                                                                                        \
    }
#else
#define MEMDUMP(addr, size)                                                                                                                    \
    {                                                                                                                                          \
    printf("\n******************************* Start of memdump *******************************\n");                                            \
        int d_i;                                                                                                                               \
        unsigned char* d_p = (unsigned char*)addr;                                                                                             \
        for(d_i = 0; d_i < size; d_i+=16, d_p+=16)                                                                                             \
        {                                                                                                                                      \
            printf("%p: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ; %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", \
                   d_p,                                                                                                                        \
                   *(d_p), *(d_p+1), *(d_p+2), *(d_p+3), *(d_p+4), *(d_p+5), *(d_p+6), *(d_p+7),                                               \
                   *(d_p+8), *(d_p+9), *(d_p+10), *(d_p+11), *(d_p+12), *(d_p+13), *(d_p+14), *(d_p+15),                                       \
                   (*(d_p) > 31 && *(d_p) < 127) ? *(d_p) : '.', (*(d_p+1) > 31 && *(d_p+1) < 127) ? *(d_p+1) : '.',                           \
                   (*(d_p+2) > 31 && *(d_p+2) < 127) ? *(d_p+2) : '.', (*(d_p+3) > 31 && *(d_p+3) < 127) ? *(d_p+3) : '.',                     \
                   (*(d_p+4) > 31 && *(d_p+4) < 127) ? *(d_p+4) : '.', (*(d_p+5) > 31 && *(d_p+5) < 127) ? *(d_p+5) : '.',                     \
                   (*(d_p+6) > 31 && *(d_p+6) < 127) ? *(d_p+6) : '.', (*(d_p+7) > 31 && *(d_p+7) < 127) ? *(d_p+7) : '.',                     \
                   (*(d_p+8) > 31 && *(d_p+8) < 127) ? *(d_p+8) : '.', (*(d_p+9) > 31 && *(d_p+9) < 127) ? *(d_p+9) : '.',                     \
                   (*(d_p+10) > 31 && *(d_p+10) < 127) ? *(d_p+10) : '.', (*(d_p+11) > 31 && *(d_p+11) < 127) ? *(d_p+11) : '.',               \
                   (*(d_p+12) > 31 && *(d_p+12) < 127) ? *(d_p+12) : '.', (*(d_p+13) > 31 && *(d_p+13) < 127) ? *(d_p+13) : '.',               \
                   (*(d_p+14) > 31 && *(d_p+14) < 127) ? *(d_p+14) : '.', (*(d_p+15) > 31 && *(d_p+15) < 127) ? *(d_p+15) : '.' );             \
        }                                                                                                                                      \
    printf("******************************** End of memdump ********************************\n\n");                                            \
    }
#endif

#define RETADDR(level) __builtin_return_address(level);

#ifdef LOG_IN_FILE
#define BACKTRACE                                                                                                   \
{                                                                                                                   \
    FILE *trace_f;                                                                                                  \
    Dl_info dli;                                                                                                    \
    trace_f = fopen(TRACE_FILE, "a");                                                                               \
    dladdr(__builtin_return_address(0), &dli);                                                                      \
    fprintf(trace_f, "\n************************** Start of backtrace [%d] **************************\n", getpid());\
    fprintf(trace_f, "%s "                                                                                          \
                    "called by %p [ %s(%p) %s(%p) ]\n",                                                             \
                    __func__,                                                                                       \
                    __builtin_return_address(0),                                                                    \
                    strrchr(dli.dli_fname, '/') ?                                                                   \
                    strrchr(dli.dli_fname, '/')+1 : dli.dli_fname,                                                  \
                    dli.dli_fbase, dli.dli_sname, dli.dli_saddr);                                                   \
    dladdr(__builtin_return_address(1), &dli);                                                                      \
    fprintf(trace_f, "%*s "                                                                                         \
                    "called by %p [ %s(%p) %s(%p) ]\n",                                                             \
                    strlen(__func__), "...",                                                                        \
                    __builtin_return_address(1),                                                                    \
                    strrchr(dli.dli_fname, '/') ?                                                                   \
                    strrchr(dli.dli_fname, '/')+1 : dli.dli_fname,                                                  \
                    dli.dli_fbase, dli.dli_sname, dli.dli_saddr);                                                   \
    fprintf(trace_f, "*************************** End of backtrace [%d] ***************************\n\n", getpid());\
    fclose(trace_f);                                                                                                \
}
#else
#define BACKTRACE                                                                                                   \
{                                                                                                                   \
    Dl_info dli;                                                                                                    \
    dladdr(__builtin_return_address(0), &dli);                                                                      \
    fprintf(stdout, "\n************************** Start of backtrace [%d] **************************\n", getpid()); \
    fprintf(stdout, "%s "                                                                                           \
                    "called by %p [ %s(%p) %s(%p) ]\n",                                                             \
                    __func__,                                                                                       \
                    __builtin_return_address(0),                                                                    \
                    strrchr(dli.dli_fname, '/') ?                                                                   \
                    strrchr(dli.dli_fname, '/')+1 : dli.dli_fname,                                                  \
                    dli.dli_fbase, dli.dli_sname, dli.dli_saddr);                                                   \
    dladdr(__builtin_return_address(1), &dli);                                                                      \
    fprintf(stdout, "%*s "                                                                                          \
                    "called by %p [ %s(%p) %s(%p) ]\n",                                                             \
                    strlen(__func__), "...",                                                                        \
                    __builtin_return_address(1),                                                                    \
                    strrchr(dli.dli_fname, '/') ?                                                                   \
                    strrchr(dli.dli_fname, '/')+1 : dli.dli_fname,                                                  \
                    dli.dli_fbase, dli.dli_sname, dli.dli_saddr);                                                   \
    fprintf(stdout, "*************************** End of backtrace [%d] ***************************\\nn", getpid()); \
}
#endif


#else

#define TRACE(...)
#define CALLSTACK
#define MEMDUMP(addr, size)
#define RETADDR(level)
#define BACKTRACE

#endif

#endif
