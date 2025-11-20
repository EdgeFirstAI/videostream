// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "g2d.h"
#include "videostream.h"

#define NANOS_PER_SECOND 1000000000L

VSL_API
const char*
vsl_version()
{
    return VSL_VERSION;
}

VSL_API
int64_t
vsl_timestamp()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((int64_t) ts.tv_sec) * NANOS_PER_SECOND + ts.tv_nsec;
}

VSL_API
int
vsl_init()
{
#ifdef __aarch64__
    g2d_init();
#endif

    return 0;
}

VSL_API
void
vsl_release()
{
#ifdef __aarch64__
    g2d_release();
#endif
}

#if !defined(_MSC_VER) && !defined(_GHS_MULTI) && !defined(__ICCARM__)
VSL_API
void __attribute__((constructor))
vsl_init_constructor()
{
    if (vsl_init()) {
        fprintf(stderr, "[ERROR] vsl_init: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

VSL_API
void __attribute__((destructor))
vsl_init_destructor()
{
    vsl_release();
}
#endif

#ifdef _MSC_VER
#include <Windows.h>

BOOL WINAPI
DllMain(HINSTANCE dll, DWORD reason, void* reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        if (vsl_init()) {
            fprintf(stderr, "[ERROR] vsl_init: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        break;
    case DLL_PROCESS_DETACH:
        vsl_release();
        break;
    }

    return TRUE;
}
#endif