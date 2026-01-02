// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "encoder_v4l2.h"
#include "codec_backend.h"
#include "common.h"
#include "frame.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// V4L2 pixel formats for compressed output (not in all kernel headers)
#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264 v4l2_fourcc('H', '2', '6', '4')
#endif
#ifndef V4L2_PIX_FMT_HEVC
#define V4L2_PIX_FMT_HEVC v4l2_fourcc('H', 'E', 'V', 'C')
#endif

// Helper for ioctl with retry on EINTR
static int
xioctl(int fd, unsigned long request, void* arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}


// Convert VSL fourcc to V4L2 fourcc for output codec
static uint32_t
vsl_to_v4l2_codec(uint32_t fourcc)
{
    switch (fourcc) {
    case VSL_FOURCC('H', '2', '6', '4'):
        return V4L2_PIX_FMT_H264;
    case VSL_FOURCC('H', 'E', 'V', 'C'):
        return V4L2_PIX_FMT_HEVC;
    default:
        return 0;
    }
}

// V4L2 fourcc codes for vsi_v4l2enc driver (non-standard)
// The driver uses different fourcc codes than standard kernel defines
#define VSI_V4L2_PIX_FMT_BGR4 v4l2_fourcc('B', 'G', 'R', '4') // BGRA/X 8-8-8-8
#define VSI_V4L2_PIX_FMT_AB24 v4l2_fourcc('A', 'B', '2', '4') // RGBA 8-8-8-8
#define VSI_V4L2_PIX_FMT_AR24 v4l2_fourcc('A', 'R', '2', '4') // BGRA 8-8-8-8

// Convert VSL fourcc to V4L2 input format and number of planes
// Uses vsi_v4l2enc driver-specific fourcc codes
static uint32_t
vsl_to_v4l2_input_format(uint32_t fourcc, int* num_planes)
{
    switch (fourcc) {
    case VSL_FOURCC('B', 'G', 'R', 'A'):
        *num_planes = 1;
        return VSI_V4L2_PIX_FMT_BGR4; // BGR4 for BGRA on vsi_v4l2enc
    case VSL_FOURCC('R', 'G', 'B', 'A'):
        *num_planes = 1;
        return VSI_V4L2_PIX_FMT_AB24; // AB24 for RGBA on vsi_v4l2enc
    case VSL_FOURCC('A', 'R', 'G', 'B'):
        *num_planes = 1;
        return VSI_V4L2_PIX_FMT_AR24; // AR24 for ARGB on vsi_v4l2enc
    case VSL_FOURCC('N', 'V', '1', '2'):
        *num_planes = 2;
        return V4L2_PIX_FMT_NV12;
    case VSL_FOURCC('Y', 'U', 'Y', 'V'):
    case VSL_FOURCC('Y', 'U', 'Y', '2'):
        *num_planes = 1;
        return V4L2_PIX_FMT_YUYV;
    case VSL_FOURCC('I', '4', '2', '0'):
        *num_planes = 1; // Treated as contiguous buffer
        return V4L2_PIX_FMT_YUV420;
    default:
        *num_planes = 0;
        return 0;
    }
}

// Convert profile to bitrate in bps
static uint32_t
profile_to_bitrate(VSLEncoderProfile profile)
{
    switch (profile) {
    case VSL_ENCODE_PROFILE_5000_KBPS:
        return 5000000;
    case VSL_ENCODE_PROFILE_25000_KBPS:
        return 25000000;
    case VSL_ENCODE_PROFILE_50000_KBPS:
        return 50000000;
    case VSL_ENCODE_PROFILE_100000_KBPS:
        return 100000000;
    case VSL_ENCODE_PROFILE_AUTO:
    default:
        return 5000000; // Default 5 Mbps
    }
}

// Set encoder control value
static int
set_ctrl(int fd, uint32_t id, int32_t value)
{
    struct v4l2_control ctrl = {
        .id    = id,
        .value = value,
    };

    if (xioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_S_CTRL (0x%x) failed: %s\n",
                id,
                strerror(errno));
        return -1;
    }
    return 0;
}

// Set up OUTPUT queue (raw input frames) with DMABUF import
static int
setup_output_queue(struct vsl_encoder_v4l2* enc,
                   int                      width,
                   int                      height,
                   uint32_t                 input_fourcc)
{
    // Convert input fourcc to V4L2 format
    int      num_planes   = 0;
    uint32_t v4l2_input_fmt = vsl_to_v4l2_input_format(input_fourcc, &num_planes);
    if (v4l2_input_fmt == 0) {
        fprintf(stderr,
                "V4L2 encoder: unsupported input format 0x%08x ('%c%c%c%c')\n",
                input_fourcc,
                (char) (input_fourcc & 0xFF),
                (char) ((input_fourcc >> 8) & 0xFF),
                (char) ((input_fourcc >> 16) & 0xFF),
                (char) ((input_fourcc >> 24) & 0xFF));
        return -1;
    }

    // Set OUTPUT format
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width       = width;
    fmt.fmt.pix_mp.height      = height;
    fmt.fmt.pix_mp.pixelformat = v4l2_input_fmt;
    fmt.fmt.pix_mp.num_planes  = num_planes;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;

    // Set plane sizes based on format
    if (num_planes == 1) {
        // Packed format (BGRA, YUYV, etc.)
        int bytes_per_pixel = 4; // Default for RGBA/BGRA
        if (v4l2_input_fmt == V4L2_PIX_FMT_YUYV) {
            bytes_per_pixel = 2;
        } else if (v4l2_input_fmt == V4L2_PIX_FMT_YUV420) {
            // I420/YUV420 is 1.5 bytes per pixel (YUV 4:2:0)
            fmt.fmt.pix_mp.plane_fmt[0].sizeimage = width * height * 3 / 2;
        } else {
            fmt.fmt.pix_mp.plane_fmt[0].sizeimage = width * height * bytes_per_pixel;
        }
    } else if (num_planes == 2) {
        // Semi-planar format (NV12)
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage = width * height;     // Y plane
        fmt.fmt.pix_mp.plane_fmt[1].sizeimage = width * height / 2; // UV plane
    }

    if (xioctl(enc->fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_S_FMT OUTPUT failed: %s\n",
                strerror(errno));
        return -1;
    }

    enc->width            = fmt.fmt.pix_mp.width;
    enc->height           = fmt.fmt.pix_mp.height;
    enc->stride           = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    enc->input_fourcc     = input_fourcc;
    enc->v4l2_input_fmt   = v4l2_input_fmt;
    enc->num_input_planes = fmt.fmt.pix_mp.num_planes;

    fprintf(stderr,
            "V4L2 encoder: configured OUTPUT format '%c%c%c%c' %dx%d, %d plane(s)\n",
            (char) (v4l2_input_fmt & 0xFF),
            (char) ((v4l2_input_fmt >> 8) & 0xFF),
            (char) ((v4l2_input_fmt >> 16) & 0xFF),
            (char) ((v4l2_input_fmt >> 24) & 0xFF),
            enc->width,
            enc->height,
            enc->num_input_planes);

    // Request DMABUF import buffers
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = VSL_V4L2_ENC_OUTPUT_BUFFERS;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_DMABUF;

    if (xioctl(enc->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_REQBUFS OUTPUT failed: %s\n",
                strerror(errno));
        return -1;
    }

    enc->output.count = req.count;
    fprintf(stderr,
            "V4L2 encoder: allocated %d OUTPUT buffers (DMABUF import)\n",
            enc->output.count);

    // Initialize buffer tracking
    for (int i = 0; i < enc->output.count; i++) {
        enc->output.buffers[i].dmabuf_fd = -1;
        enc->output.buffers[i].queued    = false;
    }

    return 0;
}

// Set up CAPTURE queue (compressed output) with MMAP
static int
setup_capture_queue(struct vsl_encoder_v4l2* enc)
{
    // Set CAPTURE format (compressed output)
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width       = enc->width;
    fmt.fmt.pix_mp.height      = enc->height;
    fmt.fmt.pix_mp.pixelformat = vsl_to_v4l2_codec(enc->output_fourcc);
    fmt.fmt.pix_mp.num_planes  = 1;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = VSL_V4L2_ENC_CAPTURE_BUF_SIZE;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;

    if (xioctl(enc->fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_S_FMT CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    // Request MMAP buffers
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = VSL_V4L2_ENC_CAPTURE_BUFFERS;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(enc->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_REQBUFS CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    enc->capture.count = req.count;
    fprintf(stderr,
            "V4L2 encoder: allocated %d CAPTURE buffers (MMAP)\n",
            enc->capture.count);

    // Query and mmap each CAPTURE buffer
    for (int i = 0; i < enc->capture.count; i++) {
        struct v4l2_buffer buf;
        struct v4l2_plane  planes[1];

        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = i;
        buf.length   = 1;
        buf.m.planes = planes;

        if (xioctl(enc->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr,
                    "V4L2 encoder: VIDIOC_QUERYBUF CAPTURE[%d] failed: %s\n",
                    i,
                    strerror(errno));
            return -1;
        }

        enc->capture.buffers[i].mmap_size = planes[0].length;
        enc->capture.buffers[i].mmap_ptr  = mmap(NULL,
                                                planes[0].length,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED,
                                                enc->fd,
                                                planes[0].m.mem_offset);

        if (enc->capture.buffers[i].mmap_ptr == MAP_FAILED) {
            fprintf(stderr,
                    "V4L2 encoder: mmap CAPTURE[%d] failed: %s\n",
                    i,
                    strerror(errno));
            return -1;
        }

        enc->capture.buffers[i].queued = false;
    }

    return 0;
}

// Queue all CAPTURE buffers
static int
queue_capture_buffers(struct vsl_encoder_v4l2* enc)
{
    for (int i = 0; i < enc->capture.count; i++) {
        if (enc->capture.buffers[i].queued) {
            continue;
        }

        struct v4l2_buffer buf;
        struct v4l2_plane  planes[1];

        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        buf.length      = 1;
        buf.m.planes    = planes;
        planes[0].length = enc->capture.buffers[i].mmap_size;

        if (xioctl(enc->fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr,
                    "V4L2 encoder: VIDIOC_QBUF CAPTURE[%d] failed: %s\n",
                    i,
                    strerror(errno));
            return -1;
        }

        enc->capture.buffers[i].queued = true;
    }

    return 0;
}

// Start streaming on both queues
static int
start_streaming(struct vsl_encoder_v4l2* enc)
{
    if (enc->streaming) {
        return 0;
    }

    // Queue all CAPTURE buffers first
    if (queue_capture_buffers(enc) < 0) {
        return -1;
    }

    // Start OUTPUT streaming
    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(enc->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_STREAMON OUTPUT failed: %s\n",
                strerror(errno));
        return -1;
    }

    // Start CAPTURE streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(enc->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_STREAMON CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    enc->streaming = true;
    fprintf(stderr, "V4L2 encoder: streaming started\n");

    return 0;
}

// Stop streaming on both queues
static void
stop_streaming(struct vsl_encoder_v4l2* enc)
{
    if (!enc->streaming) {
        return;
    }

    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    xioctl(enc->fd, VIDIOC_STREAMOFF, &type);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    xioctl(enc->fd, VIDIOC_STREAMOFF, &type);

    // Mark all buffers as not queued
    for (int i = 0; i < enc->output.count; i++) {
        enc->output.buffers[i].queued = false;
    }
    for (int i = 0; i < enc->capture.count; i++) {
        enc->capture.buffers[i].queued = false;
    }

    enc->streaming = false;
}

// Configure encoder controls (bitrate, GOP, etc.)
static int
configure_encoder(struct vsl_encoder_v4l2* enc)
{
    uint32_t bitrate = profile_to_bitrate(enc->profile);

    // Set bitrate
    if (set_ctrl(enc->fd, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate) < 0) {
        fprintf(stderr,
                "V4L2 encoder: failed to set bitrate %u\n",
                bitrate);
        // Continue anyway, driver may use default
    }

    // Set GOP size (keyframe interval)
    int gop_size = enc->fps; // One keyframe per second
    if (set_ctrl(enc->fd, V4L2_CID_MPEG_VIDEO_GOP_SIZE, gop_size) < 0) {
        fprintf(stderr, "V4L2 encoder: failed to set GOP size %d\n", gop_size);
    }

    // Set codec-specific parameters
    if (enc->output_fourcc == VSL_FOURCC('H', '2', '6', '4')) {
        // H.264 profile: High
        set_ctrl(enc->fd,
                 V4L2_CID_MPEG_VIDEO_H264_PROFILE,
                 V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);

        // H.264 level: 4.0 (suitable for 1080p30)
        set_ctrl(enc->fd,
                 V4L2_CID_MPEG_VIDEO_H264_LEVEL,
                 V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
    } else if (enc->output_fourcc == VSL_FOURCC('H', 'E', 'V', 'C')) {
        // HEVC profile: Main
        set_ctrl(enc->fd,
                 V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
                 V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN);

        // HEVC level: 4.0
        set_ctrl(enc->fd,
                 V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
                 V4L2_MPEG_VIDEO_HEVC_LEVEL_4);
    }

    fprintf(stderr,
            "V4L2 encoder: configured bitrate=%u bps, GOP=%d\n",
            bitrate,
            gop_size);

    return 0;
}

VSLEncoder*
vsl_encoder_create_v4l2(VSLEncoderProfile profile, uint32_t output_fourcc, int fps)
{
    // Validate codec
    uint32_t v4l2_codec = vsl_to_v4l2_codec(output_fourcc);
    if (v4l2_codec == 0) {
        fprintf(stderr,
                "V4L2 encoder: unsupported codec fourcc 0x%08x\n",
                output_fourcc);
        return NULL;
    }

    // Open V4L2 device
    int fd = open(VSL_V4L2_ENCODER_DEV, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr,
                "V4L2 encoder: failed to open %s: %s\n",
                VSL_V4L2_ENCODER_DEV,
                strerror(errno));
        return NULL;
    }

    // Check device capabilities
    struct v4l2_capability cap;
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_QUERYCAP failed: %s\n",
                strerror(errno));
        close(fd);
        return NULL;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
        fprintf(stderr,
                "V4L2 encoder: device lacks V4L2_CAP_VIDEO_M2M_MPLANE\n");
        close(fd);
        return NULL;
    }

    fprintf(stderr,
            "V4L2 encoder: opened %s (%s)\n",
            VSL_V4L2_ENCODER_DEV,
            cap.card);

    // Allocate encoder structure
    struct vsl_encoder_v4l2* enc = calloc(1, sizeof(struct vsl_encoder_v4l2));
    if (!enc) {
        close(fd);
        return NULL;
    }

    enc->backend       = VSL_CODEC_BACKEND_V4L2;
    enc->fd            = fd;
    enc->profile       = profile;
    enc->output_fourcc = output_fourcc;
    enc->fps           = fps;

    return (VSLEncoder*) enc;
}

void
vsl_encoder_release_v4l2(VSLEncoder* encoder)
{
    if (!encoder) {
        return;
    }

    struct vsl_encoder_v4l2* enc = (struct vsl_encoder_v4l2*) encoder;

    // Stop streaming
    stop_streaming(enc);

    // Unmap CAPTURE buffers
    for (int i = 0; i < enc->capture.count; i++) {
        if (enc->capture.buffers[i].mmap_ptr &&
            enc->capture.buffers[i].mmap_ptr != MAP_FAILED) {
            munmap(enc->capture.buffers[i].mmap_ptr,
                   enc->capture.buffers[i].mmap_size);
        }
    }

    // Close device
    if (enc->fd >= 0) {
        close(enc->fd);
    }

    fprintf(stderr,
            "V4L2 encoder: released (%lu frames encoded)\n",
            enc->frames_encoded);

    free(enc);
}

int
vsl_encode_frame_v4l2(VSLEncoder*    encoder,
                      VSLFrame*      source,
                      VSLFrame*      destination,
                      const VSLRect* crop_region,
                      int*           keyframe)
{
    if (!encoder || !source || !destination) {
        return -1;
    }

    struct vsl_encoder_v4l2* enc = (struct vsl_encoder_v4l2*) encoder;
    uint64_t                 start_time = vsl_timestamp_us();

    // Initialize encoder on first frame (need dimensions and format from source)
    if (!enc->initialized) {
        int      width        = vsl_frame_width(source);
        int      height       = vsl_frame_height(source);
        uint32_t input_fourcc = vsl_frame_fourcc(source);

        if (setup_output_queue(enc, width, height, input_fourcc) < 0) {
            return -1;
        }

        if (setup_capture_queue(enc) < 0) {
            return -1;
        }

        configure_encoder(enc);

        if (start_streaming(enc) < 0) {
            return -1;
        }

        enc->initialized = true;
        fprintf(stderr,
                "V4L2 encoder: initialized %dx%d, format '%c%c%c%c'\n",
                enc->width,
                enc->height,
                (char) (input_fourcc & 0xFF),
                (char) ((input_fourcc >> 8) & 0xFF),
                (char) ((input_fourcc >> 16) & 0xFF),
                (char) ((input_fourcc >> 24) & 0xFF));
    }

    // Find available OUTPUT buffer
    int out_idx = -1;
    for (int i = 0; i < enc->output.count; i++) {
        if (!enc->output.buffers[i].queued) {
            out_idx = i;
            break;
        }
    }

    // If no OUTPUT buffer available, try to dequeue one
    if (out_idx < 0) {
        struct v4l2_buffer buf;
        struct v4l2_plane  planes[VSL_V4L2_ENC_MAX_PLANES];

        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory   = V4L2_MEMORY_DMABUF;
        buf.length   = enc->num_input_planes;
        buf.m.planes = planes;

        if (xioctl(enc->fd, VIDIOC_DQBUF, &buf) == 0) {
            enc->output.buffers[buf.index].queued = false;
            out_idx = buf.index;
        }
    }

    if (out_idx < 0) {
        fprintf(stderr, "V4L2 encoder: no OUTPUT buffer available\n");
        return -1;
    }

    // Get source frame's DMA-BUF fd
    int src_fd = source->handle;
    if (src_fd < 0) {
        fprintf(stderr, "V4L2 encoder: source frame has no DMA-BUF fd\n");
        return -1;
    }

    // Queue source frame to OUTPUT queue
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[VSL_V4L2_ENC_MAX_PLANES];

    memset(&buf, 0, sizeof(buf));
    memset(planes, 0, sizeof(planes));
    buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory   = V4L2_MEMORY_DMABUF;
    buf.index    = out_idx;
    buf.length   = enc->num_input_planes;
    buf.m.planes = planes;

    // Set up plane data based on format
    if (enc->num_input_planes == 1) {
        // Single-plane packed format (BGRA, YUYV, etc.)
        size_t frame_size = vsl_frame_size(source);
        planes[0].m.fd      = src_fd;
        planes[0].length    = frame_size;
        planes[0].bytesused = frame_size;
    } else {
        // Multi-plane format (NV12: plane 0 = Y, plane 1 = UV)
        size_t y_size  = enc->width * enc->height;
        size_t uv_size = enc->width * enc->height / 2;

        planes[0].m.fd      = src_fd;
        planes[0].length    = y_size;
        planes[0].bytesused = y_size;

        planes[1].m.fd        = src_fd;
        planes[1].length      = uv_size;
        planes[1].bytesused   = uv_size;
        planes[1].data_offset = y_size; // UV plane starts after Y
    }

    if (xioctl(enc->fd, VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_QBUF OUTPUT[%d] failed: %s\n",
                out_idx,
                strerror(errno));
        return -1;
    }
    enc->output.buffers[out_idx].queued = true;

    // Poll for encoded output
    struct pollfd pfd = {
        .fd     = enc->fd,
        .events = POLLIN,
    };

    int ret = poll(&pfd, 1, VSL_V4L2_ENC_POLL_TIMEOUT_MS * 10);
    if (ret < 0) {
        fprintf(stderr, "V4L2 encoder: poll failed: %s\n", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        fprintf(stderr, "V4L2 encoder: poll timeout waiting for output\n");
        return -1;
    }

    // Dequeue encoded frame from CAPTURE queue
    struct v4l2_buffer cap_buf;
    struct v4l2_plane  cap_planes[1];

    memset(&cap_buf, 0, sizeof(cap_buf));
    memset(cap_planes, 0, sizeof(cap_planes));
    cap_buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    cap_buf.memory   = V4L2_MEMORY_MMAP;
    cap_buf.length   = 1;
    cap_buf.m.planes = cap_planes;

    if (xioctl(enc->fd, VIDIOC_DQBUF, &cap_buf) < 0) {
        if (errno == EAGAIN) {
            return 0; // No frame ready yet
        }
        fprintf(stderr,
                "V4L2 encoder: VIDIOC_DQBUF CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    int cap_idx   = cap_buf.index;
    int encoded_size = cap_planes[0].bytesused;

    enc->capture.buffers[cap_idx].queued = false;

    // Copy encoded data to destination frame
    void* dst_ptr = vsl_frame_mmap(destination, NULL);
    if (!dst_ptr) {
        fprintf(stderr, "V4L2 encoder: failed to mmap destination frame\n");
        // Re-queue capture buffer
        if (xioctl(enc->fd, VIDIOC_QBUF, &cap_buf) == 0) {
            enc->capture.buffers[cap_idx].queued = true;
        }
        return -1;
    }

    memcpy(dst_ptr, enc->capture.buffers[cap_idx].mmap_ptr, encoded_size);

    // Update destination frame info
    destination->info.width  = enc->width;
    destination->info.height = enc->height;
    destination->info.stride = enc->stride;
    destination->info.fourcc = enc->output_fourcc;
    destination->info.size   = encoded_size;

    // Check if keyframe
    if (keyframe) {
        *keyframe = (cap_buf.flags & V4L2_BUF_FLAG_KEYFRAME) ? 1 : 0;
    }

    // Re-queue capture buffer
    if (xioctl(enc->fd, VIDIOC_QBUF, &cap_buf) < 0) {
        fprintf(stderr,
                "V4L2 encoder: failed to re-queue CAPTURE[%d]: %s\n",
                cap_idx,
                strerror(errno));
    } else {
        enc->capture.buffers[cap_idx].queued = true;
    }

    // Also try to dequeue used OUTPUT buffer
    memset(&buf, 0, sizeof(buf));
    memset(planes, 0, sizeof(planes));
    buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory   = V4L2_MEMORY_DMABUF;
    buf.length   = enc->num_input_planes;
    buf.m.planes = planes;

    while (xioctl(enc->fd, VIDIOC_DQBUF, &buf) == 0) {
        enc->output.buffers[buf.index].queued = false;
    }

    // Update statistics
    enc->frames_encoded++;
    enc->total_encode_time_us += vsl_timestamp_us() - start_time;

    return encoded_size;
}

VSLFrame*
vsl_encoder_new_output_frame_v4l2(const VSLEncoder* encoder,
                                  int               width,
                                  int               height,
                                  int64_t           duration,
                                  int64_t           pts,
                                  int64_t           dts)
{
    if (!encoder) {
        return NULL;
    }

    const struct vsl_encoder_v4l2* enc =
        (const struct vsl_encoder_v4l2*) encoder;

    // Create frame with compressed output fourcc
    VSLFrame* frame = vsl_frame_init(width,
                                     height,
                                     width, // stride not relevant for encoded
                                     enc->output_fourcc,
                                     NULL, // no userptr
                                     NULL); // no cleanup
    if (!frame) {
        return NULL;
    }

    // Set the frame size for allocation (2MB for compressed data)
    frame->info.size = VSL_V4L2_ENC_CAPTURE_BUF_SIZE;

    // Allocate backing memory
    if (vsl_frame_alloc(frame, NULL) < 0) {
        vsl_frame_release(frame);
        return NULL;
    }

    frame->info.duration = duration;
    frame->info.pts      = pts;
    frame->info.dts      = dts;

    return frame;
}
