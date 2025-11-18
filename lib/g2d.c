// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <stdlib.h>

#define LIBG2D_IMPLEMENTATION
#include "libg2d.h"

static struct g2d* g2d = NULL;

void
g2d_init()
{
    const char* debug = getenv("VSL_DEBUG");
    if (debug && *debug == '1') {
        fprintf(stderr, "[DEBUG] %s\n", __FUNCTION__);
    }

    char* error = NULL;
    g2d         = g2d_initialize(NULL, &error);
    if (error) {
        fprintf(stderr, "[WARNING] g2d: %s\n", error);
        free(error);
    }
}

void
g2d_release()
{
    if (g2d) {
        const char* debug = getenv("VSL_DEBUG");
        if (debug && *debug == '1') {
            fprintf(stderr, "[DEBUG] %s\n", __FUNCTION__);
        }

        dlclose(g2d->library);
        free(g2d);
        g2d = NULL;
    }
}
