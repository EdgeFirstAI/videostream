// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSL_FRAME_H
#define VSL_FRAME_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include "videostream.h"

#ifndef _WIN32
#define SOCKET int
#endif

enum vsl_frame_error {
    VSL_FRAME_SUCCESS = 0,
    VSL_FRAME_ERROR_EXPIRED,
    VSL_FRAME_ERROR_INVALID_CONTROL,
    VSL_FRAME_TOO_MANY_FRAMES_LOCKED,
};

enum vsl_frame_message {
    VSL_FRAME_TRYLOCK,
    VSL_FRAME_UNLOCK,
};

enum vsl_frame_allocator {
    VSL_FRAME_ALLOCATOR_EXTERNAL = 0,
    VSL_FRAME_ALLOCATOR_DMAHEAP,
    VSL_FRAME_ALLOCATOR_SHM,
};

struct vsl_frame_info {
    int64_t  serial;
    int64_t  timestamp;
    int64_t  duration;
    int64_t  pts;
    int64_t  dts;
    int64_t  expires;
    int      locked;
    uint32_t fourcc;
    int      width;
    int      height;
    intptr_t paddr;
    size_t   size;
    off_t    offset;
    int      stride;
};

struct vsl_frame {
    void*                    _parent; // deprecated for host/client.
    void*                    userptr;
    vsl_frame_cleanup        cleanup;
    int                      handle;
    off_t                    offset_deprecated;
    void*                    map;
    size_t                   mapsize;
    struct vsl_frame_info    info;
    VSLHost*                 host;
    VSLClient*               client;
    enum vsl_frame_allocator allocator;
    char*                    path;
};

struct vsl_frame_control {
    enum vsl_frame_message message;
    int64_t                serial;
};

struct vsl_frame_event {
    enum vsl_frame_error  error;
    struct vsl_frame_info info;
};

struct vsl_aux {
    struct cmsghdr hdr;
    int            handle;
};

int
frame_stride(uint32_t fourcc, int width);

#endif /* VSL_FRAME_H */
