// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef buffer_v4l2
#define buffer_v4l2

#include "stdatomic.h"
#include "videostream.h"
#ifdef __cplusplus
extern "C" {
#endif

struct vsl_camera {
    const char*               dev_name;
    int                       fd;
    struct vsl_camera_buffer* buffers;
    struct v4l2_buffer*       v4l2_buffers;
    unsigned int              n_buffers;
    int                       not_plane;
    atomic_int                queued_buf_count;
};

struct vsl_camera_buffer {
    void*          mmap;
    int            dmafd;
    u_int64_t      phys_addr;
    u_int32_t      length;
    u_int32_t      fourcc;
    int            bufID;
    struct timeval timestamp;
};

#ifdef __cplusplus
}
#endif

#endif /* buffer_v4l2 */