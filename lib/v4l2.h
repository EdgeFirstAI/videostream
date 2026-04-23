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
    /* Colorimetry cached at VIDIOC_S_FMT time. Raw V4L2 UAPI enum values
     * (V4L2_COLORSPACE_*, V4L2_XFER_FUNC_*, V4L2_YCBCR_ENC_*,
     * V4L2_QUANTIZATION_*); 0 == _DEFAULT (driver did not resolve). */
    u_int32_t                 color_space;
    u_int32_t                 color_transfer;
    u_int32_t                 color_encoding;
    u_int32_t                 color_range;
};

struct vsl_camera_buffer {
    void*          mmap;
    int            dmafd;
    u_int64_t      phys_addr;
    u_int32_t      length;
    u_int32_t      fourcc;
    u_int32_t      bytes_per_line;
    u_int32_t      sequence;
    int            bufID;
    struct timeval timestamp;
};

#ifdef __cplusplus
}
#endif

#endif /* buffer_v4l2 */