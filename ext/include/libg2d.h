/*
 * Copyright (C) 2013-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * The following programs are the sole property of Freescale Semiconductor
 * Inc., and contain its proprietary and confidential information.
 *
 * This file has been heavily modified by Au-Zone Technologies to support
 * opening libg2d.so through dlopen.
 *
 */

#ifndef __LIBG2D_H__
#define __LIBG2D_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_G2D_ERROR
#define MAX_G2D_ERROR 128
#endif

enum g2d_format {
    // rgb formats
    G2D_RGB565   = 0,
    G2D_RGBA8888 = 1,
    G2D_RGBX8888 = 2,
    G2D_BGRA8888 = 3,
    G2D_BGRX8888 = 4,
    G2D_BGR565   = 5,
    G2D_ARGB8888 = 6,
    G2D_ABGR8888 = 7,
    G2D_XRGB8888 = 8,
    G2D_XBGR8888 = 9,
    G2D_RGB888   = 10,
    G2D_BGR888   = 11,

    // yuv formats
    G2D_NV12 = 20,
    G2D_I420 = 21,
    G2D_YV12 = 22,
    G2D_NV21 = 23,
    G2D_YUYV = 24,
    G2D_YVYU = 25,
    G2D_UYVY = 26,
    G2D_VYUY = 27,
    G2D_NV16 = 28,
    G2D_NV61 = 29,
};

enum g2d_blend_func {
    // basic blend
    G2D_ZERO                = 0,
    G2D_ONE                 = 1,
    G2D_SRC_ALPHA           = 2,
    G2D_ONE_MINUS_SRC_ALPHA = 3,
    G2D_DST_ALPHA           = 4,
    G2D_ONE_MINUS_DST_ALPHA = 5,

    // extensive blend is set with basic blend together,
    // such as, G2D_ONE | G2D_PRE_MULTIPLIED_ALPHA
    G2D_PRE_MULTIPLIED_ALPHA = 0x10,
    G2D_DEMULTIPLY_OUT_ALPHA = 0x20,
};

enum g2d_cap_mode {
    G2D_BLEND        = 0,
    G2D_DITHER       = 1,
    G2D_GLOBAL_ALPHA = 2, // only support source global alpha
    G2D_BLEND_DIM    = 3, // support special blend effect
    G2D_BLUR         = 4, // blur effect
    G2D_YUV_BT_601   = 5, // yuv BT.601
    G2D_YUV_BT_709   = 6, // yuv BT.709
    G2D_YUV_BT_601FR = 7, // yuv BT.601 Full Range
    G2D_YUV_BT_709FR = 8, // yuv BT.709 Full Range
};

enum g2d_feature {
    G2D_SCALING = 0,
    G2D_ROTATION,
    G2D_SRC_YUV,
    G2D_DST_YUV,
    G2D_MULTI_SOURCE_BLT,
    G2D_FAST_CLEAR,
};

enum g2d_rotation {
    G2D_ROTATION_0   = 0,
    G2D_ROTATION_90  = 1,
    G2D_ROTATION_180 = 2,
    G2D_ROTATION_270 = 3,
    G2D_FLIP_H       = 4,
    G2D_FLIP_V       = 5,
};

enum g2d_cache_mode {
    G2D_CACHE_CLEAN      = 0,
    G2D_CACHE_FLUSH      = 1,
    G2D_CACHE_INVALIDATE = 2,
};

enum g2d_hardware_type {
    G2D_HARDWARE_2D = 0, // default type
    G2D_HARDWARE_VG = 1,
};

enum g2d_status {
    G2D_STATUS_FAIL          = -1,
    G2D_STATUS_OK            = 0,
    G2D_STATUS_NOT_SUPPORTED = 1,
};

struct g2d_surface {
    enum g2d_format format;

    int planes[3]; // surface buffer addresses are set in physical planes
                   // separately RGB:  planes[0] -
                   // RGB565/RGBA8888/RGBX8888/BGRA8888/BRGX8888 NV12: planes[0]
                   // - Y, planes[1] - packed UV I420: planes[0] - Y, planes[1]
                   // - U, planes[2] - V YV12: planes[0] - Y, planes[1] - V,
                   // planes[2] - U NV21: planes[0] - Y, planes[1] - packed VU
                   // YUYV: planes[0] - packed YUYV
                   // YVYU: planes[0] - packed YVYU
                   // UYVY: planes[0] - packed UYVY
                   // VYUY: planes[0] - packed VYUY
                   // NV16: planes[0] - Y, planes[1] - packed UV
                   // NV61: planes[0] - Y, planes[1] - packed VU

    // blit rectangle in surface
    int left;
    int top;
    int right;
    int bottom;

    int stride; // surface buffer stride

    int width;  // surface width
    int height; // surface height

    // alpha blending parameters
    enum g2d_blend_func blendfunc;

    // the global alpha value is 0 ~ 255
    int global_alpha;

    // clrcolor format is RGBA8888, used as dst for clear, as src for blend dim
    int clrcolor;

    // rotation degree
    enum g2d_rotation rot;
};

struct g2d_surface_pair {
    struct g2d_surface s;
    struct g2d_surface d;
};

struct g2d_buf {
    void* buf_handle;
    void* buf_vaddr;
    int   buf_paddr;
    int   buf_size;
};

typedef int (*g2d_open_t)(void**);
typedef int (*g2d_close_t)(void*);

typedef int (*g2d_make_current_t)(void* handle, enum g2d_hardware_type type);

typedef int (*g2d_clear_t)(void* handle, struct g2d_surface* area);
typedef int (*g2d_blit_t)(void*               handle,
                          struct g2d_surface* src,
                          struct g2d_surface* dst);
typedef int (*g2d_copy_t)(void*           handle,
                          struct g2d_buf* d,
                          struct g2d_buf* s,
                          int             size);
typedef int (*g2d_multi_blit_t)(void*                    handle,
                                struct g2d_surface_pair* sp[],
                                int                      layers);

typedef int (*g2d_query_hardware_t)(void*                  handle,
                                    enum g2d_hardware_type type,
                                    int*                   available);
typedef int (*g2d_query_feature_t)(void*            handle,
                                   enum g2d_feature feature,
                                   int*             available);
typedef int (*g2d_query_cap_t)(void*             handle,
                               enum g2d_cap_mode cap,
                               int*              enable);
typedef int (*g2d_enable_t)(void* handle, enum g2d_cap_mode cap);
typedef int (*g2d_disable_t)(void* handle, enum g2d_cap_mode cap);

typedef int (*g2d_cache_op_t)(struct g2d_buf* buf, enum g2d_cache_mode op);
typedef struct g2d_buf* (*g2d_alloc_t)(int size, int cacheable);
typedef struct g2d_buf* (*g2d_buf_from_virt_addr_t)(void*, int);
typedef struct g2d_buf* (*g2d_buf_from_fd_t)(int fd);
typedef int (*g2d_buf_export_fd_t)(struct g2d_buf*);
typedef int (*g2d_free_t)(struct g2d_buf*);

typedef int (*g2d_flush_t)(void* handle);
typedef int (*g2d_finish_t)(void* handle);

struct g2d {
    void* library;

    g2d_open_t  open;
    g2d_close_t close;

    g2d_make_current_t make_current;

    g2d_clear_t      clear;
    g2d_blit_t       blit;
    g2d_copy_t       copy;
    g2d_multi_blit_t multi_blit;

    g2d_query_hardware_t query_hardware;
    g2d_query_feature_t  query_feature;
    g2d_query_cap_t      query_cap;
    g2d_enable_t         enable;
    g2d_disable_t        disable;

    g2d_cache_op_t           cache_op;
    g2d_alloc_t              alloc;
    g2d_buf_from_virt_addr_t buf_from_virt_addr;
    g2d_buf_from_fd_t        buf_from_fd;
    g2d_buf_export_fd_t      buf_export_fd;

    g2d_free_t free;

    g2d_flush_t  flush;
    g2d_finish_t finish;
};

extern struct g2d*
g2d_initialize(const char* path, char** error);

#ifdef LIBG2D_IMPLEMENTATION

#include <dlfcn.h>
#include <string.h>

struct g2d_function {
    const char* name;
    void**      func;
};

struct g2d*
g2d_initialize(const char* path, char** error)
{
    static int  enable_g2d       = -1;
    const char* libname          = path ? path : "libg2d.so";
    const char* alternate_path   = "libg2d.so.1";
    const char* alternate_path_2 = "libg2d.so.2";

    if (enable_g2d == -1) {
        enable_g2d         = 1;
        const char* enable = getenv("ENABLE_G2D");

        if (enable) {
            if (strcmp(enable, "0") == 0) { enable_g2d = 0; }
        }
    }

    if (!enable_g2d) { return NULL; }

    struct g2d* g2d = calloc(1, sizeof(struct g2d));
    if (!g2d) return NULL;

    struct g2d_function functions[] = {
        {"g2d_open", (void**) &g2d->open},
        {"g2d_close", (void**) &g2d->close},

        {"g2d_make_current", (void**) &g2d->make_current},

        {"g2d_clear", (void**) &g2d->clear},
        {"g2d_blit", (void**) &g2d->blit},
        {"g2d_copy", (void**) &g2d->copy},
        {"g2d_multi_blit", (void**) &g2d->multi_blit},

        {"g2d_query_hardware", (void**) &g2d->query_hardware},
        {"g2d_query_feature", (void**) &g2d->query_feature},
        {"g2d_query_cap", (void**) &g2d->query_cap},
        {"g2d_enable", (void**) &g2d->enable},
        {"g2d_disable", (void**) &g2d->disable},

        {"g2d_cache_op", (void**) &g2d->cache_op},
        {"g2d_alloc", (void**) &g2d->alloc},
        {"g2d_buf_from_virt_addr", (void**) &g2d->buf_from_virt_addr},
        {"g2d_buf_from_fd", (void**) &g2d->buf_from_fd},
        {"g2d_buf_export_fd", (void**) &g2d->buf_export_fd},
        {"g2d_free", (void**) &g2d->free},

        {"g2d_flush", (void**) &g2d->flush},
        {"g2d_finish", (void**) &g2d->finish},

        {NULL, NULL},
    };

    g2d->library = dlopen(libname, RTLD_LAZY);
    if (!g2d->library) {
        g2d->library = dlopen(alternate_path, RTLD_LAZY);

        if (!g2d->library) {
            g2d->library = dlopen(alternate_path_2, RTLD_LAZY);
            if (!g2d->library) {
                free(g2d);
                if (error) {
                    *error = calloc(1, MAX_G2D_ERROR);
                    if (*error == NULL) return NULL;
                    snprintf(*error,
                             MAX_G2D_ERROR,
                             "%s: %s",
                             libname,
                             dlerror());
                }
                return NULL;
            }
        }
    }

    for (struct g2d_function* f = functions; f->name; f++) {
        *f->func = dlsym(g2d->library, f->name);
#ifndef NDEBUG
        if (!*f->func) { printf("[WARNING] missing symbol: %s\n", f->name); }
#endif
    }

    return g2d;
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* __LIBG2D_H__ */
