// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "decoder_v4l2.h"
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

// Convert VSL codec to V4L2 pixel format
static uint32_t
vsl_codec_to_v4l2_fmt(uint32_t codec)
{
    switch (codec) {
    case VSL_FOURCC('H', '2', '6', '4'):
        return V4L2_PIX_FMT_H264;
    case VSL_FOURCC('H', 'E', 'V', 'C'):
        return V4L2_PIX_FMT_HEVC;
    default:
        return 0;
    }
}

// Wrapper for V4L2 ioctl with retry on EINTR
static int
xioctl(int fd, unsigned long request, void* arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

// Find a free OUTPUT buffer
static int
find_free_output_buffer(struct vsl_decoder_v4l2* dec)
{
    for (int i = 0; i < dec->output.count; i++) {
        if (!dec->output.buffers[i].queued) { return i; }
    }
    return -1;
}

// Setup OUTPUT queue (compressed data input)
static int
setup_output_queue(struct vsl_decoder_v4l2* dec, uint32_t v4l2_codec)
{
    // Set OUTPUT format (compressed) - single-planar
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.pixelformat = v4l2_codec;
    fmt.fmt.pix.sizeimage   = VSL_V4L2_DEC_OUTPUT_BUF_SIZE;

    if (xioctl(dec->fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_S_FMT OUTPUT failed: %s\n",
                strerror(errno));
        return -1;
    }

    // Request OUTPUT buffers (MMAP)
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = VSL_V4L2_DEC_OUTPUT_BUFFERS;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(dec->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_REQBUFS OUTPUT failed: %s\n",
                strerror(errno));
        return -1;
    }

    dec->output.count = (int) req.count;
#ifndef NDEBUG
    fprintf(stderr,
            "[decoder_v4l2] OUTPUT queue: %d buffers allocated\n",
            dec->output.count);
#endif

    // Query and mmap each OUTPUT buffer
    for (int i = 0; i < dec->output.count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (xioctl(dec->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr,
                    "[decoder_v4l2] VIDIOC_QUERYBUF OUTPUT[%d] failed: %s\n",
                    i,
                    strerror(errno));
            return -1;
        }

        dec->output.buffers[i].mmap_size = buf.length;
        dec->output.buffers[i].mmap_ptr  = mmap(NULL,
                                               buf.length,
                                               PROT_READ | PROT_WRITE,
                                               MAP_SHARED,
                                               dec->fd,
                                               buf.m.offset);

        if (dec->output.buffers[i].mmap_ptr == MAP_FAILED) {
            fprintf(stderr,
                    "[decoder_v4l2] mmap OUTPUT[%d] failed: %s\n",
                    i,
                    strerror(errno));
            return -1;
        }

        dec->output.buffers[i].queued = false;
    }

    return 0;
}

// Allocate CAPTURE frame buffer from DMA heap
static int
alloc_capture_frame(struct vsl_decoder_v4l2* dec, int index)
{
    struct vsl_v4l2_capture_buffer* cap = &dec->capture.buffers[index];

    // Create frame for decoded output
    cap->frame = vsl_frame_init(dec->width,
                                dec->height,
                                dec->capture.stride,
                                dec->out_fourcc,
                                NULL,
                                NULL);
    if (!cap->frame) {
        fprintf(stderr,
                "[decoder_v4l2] vsl_frame_init CAPTURE[%d] failed\n",
                index);
        return -1;
    }

    // Use driver's reported buffer size (may be larger due to alignment)
    // This ensures the DMABUF is large enough for the driver
    size_t alloc_size = dec->capture.plane_sizes[0];
    if (alloc_size == 0) {
        // Fallback to calculated NV12 size if driver didn't report
        alloc_size = (size_t) dec->capture.stride * dec->height * 3 / 2;
    }
    cap->frame->info.size = alloc_size;

    // Allocate from DMA heap
    if (vsl_frame_alloc(cap->frame, NULL) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] vsl_frame_alloc CAPTURE[%d] failed: %s\n",
                index,
                strerror(errno));
        vsl_frame_release(cap->frame);
        cap->frame = NULL;
        return -1;
    }

    cap->dmabuf_fd = vsl_frame_handle(cap->frame);
    cap->queued    = false;

#ifndef NDEBUG
    fprintf(stderr,
            "[decoder_v4l2] CAPTURE[%d] allocated: fd=%d size=%zu\n",
            index,
            cap->dmabuf_fd,
            alloc_size);
#endif

    return 0;
}

// Setup CAPTURE queue (decoded frames output)
static int
setup_capture_queue(struct vsl_decoder_v4l2* dec)
{
    // Get negotiated CAPTURE format
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(dec->fd, VIDIOC_G_FMT, &fmt) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_G_FMT CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    dec->width          = fmt.fmt.pix.width;
    dec->height         = fmt.fmt.pix.height;
    dec->capture.stride = fmt.fmt.pix.bytesperline;
    dec->out_fourcc     = VSL_FOURCC('N', 'V', '1', '2'); // NV12 output

    // Store buffer size for single-planar format
    dec->capture.plane_sizes[0] = fmt.fmt.pix.sizeimage;
    dec->capture.plane_sizes[1] = 0; // Not used in single-planar

#ifndef NDEBUG
    fprintf(stderr,
            "[decoder_v4l2] CAPTURE format: %dx%d stride=%d\n",
            dec->width,
            dec->height,
            dec->capture.stride);
#endif

    // Get crop/selection info
    struct v4l2_selection sel;
    memset(&sel, 0, sizeof(sel));
    sel.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sel.target = V4L2_SEL_TGT_COMPOSE;

    if (xioctl(dec->fd, VIDIOC_G_SELECTION, &sel) == 0) {
        dec->crop_region.x      = sel.r.left;
        dec->crop_region.y      = sel.r.top;
        dec->crop_region.width  = sel.r.width;
        dec->crop_region.height = sel.r.height;
    } else {
        // No crop info, use full frame
        dec->crop_region.x      = 0;
        dec->crop_region.y      = 0;
        dec->crop_region.width  = dec->width;
        dec->crop_region.height = dec->height;
    }

    // Request CAPTURE buffers (DMABUF import mode)
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = VSL_V4L2_DEC_CAPTURE_BUFFERS;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_DMABUF;

    if (xioctl(dec->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_REQBUFS CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    dec->capture.count = (int) req.count;
#ifndef NDEBUG
    fprintf(stderr,
            "[decoder_v4l2] CAPTURE queue: %d buffers allocated\n",
            dec->capture.count);
#endif

    // Allocate DMA heap buffers and queue them
    for (int i = 0; i < dec->capture.count; i++) {
        if (alloc_capture_frame(dec, i) < 0) { return -1; }
    }

    return 0;
}

// Queue a CAPTURE buffer
static int
queue_capture_buffer(struct vsl_decoder_v4l2* dec, int index)
{
    struct vsl_v4l2_capture_buffer* cap = &dec->capture.buffers[index];

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index  = index;
    buf.length = dec->capture.plane_sizes[0];
    buf.m.fd   = cap->dmabuf_fd;

    if (xioctl(dec->fd, VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_QBUF CAPTURE[%d] failed: %s\n",
                index,
                strerror(errno));
        return -1;
    }

    cap->queued = true;
    return 0;
}

// Queue all CAPTURE buffers
static int
queue_all_capture_buffers(struct vsl_decoder_v4l2* dec)
{
    for (int i = 0; i < dec->capture.count; i++) {
        // Merge nested if (S1066): short-circuit ensures queue only if not
        // queued
        if (!dec->capture.buffers[i].queued &&
            queue_capture_buffer(dec, i) < 0) {
            return -1;
        }
    }
    return 0;
}

// Start CAPTURE streaming (OUTPUT already started in create)
static int
start_capture_streaming(struct vsl_decoder_v4l2* dec)
{
    if (dec->streaming) { return 0; }

    // OUTPUT streaming should already be started
    if (!dec->output_streaming) {
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (xioctl(dec->fd, VIDIOC_STREAMON, &type) < 0) {
            fprintf(stderr,
                    "[decoder_v4l2] VIDIOC_STREAMON OUTPUT failed: %s\n",
                    strerror(errno));
            return -1;
        }
        dec->output_streaming = true;
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(dec->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_STREAMON CAPTURE failed: %s\n",
                strerror(errno));
        return -1;
    }

    dec->streaming = true;
#ifndef NDEBUG
    fprintf(stderr, "[decoder_v4l2] CAPTURE streaming started\n");
#endif

    return 0;
}

// Stop streaming on both queues
static int
stop_streaming(struct vsl_decoder_v4l2* dec)
{
    int type;

    // Stop CAPTURE streaming
    if (dec->streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(dec->fd, VIDIOC_STREAMOFF, &type);
        dec->streaming = false;
    }

    // Stop OUTPUT streaming
    if (dec->output_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        xioctl(dec->fd, VIDIOC_STREAMOFF, &type);
        dec->output_streaming = false;
    }

    // Mark all buffers as not queued
    for (int i = 0; i < dec->output.count; i++) {
        dec->output.buffers[i].queued = false;
    }
    for (int i = 0; i < dec->capture.count; i++) {
        dec->capture.buffers[i].queued = false;
    }

#ifndef NDEBUG
    fprintf(stderr, "[decoder_v4l2] streaming stopped\n");
#endif

    return 0;
}

// Handle resolution change event
static int
handle_resolution_change(struct vsl_decoder_v4l2* dec)
{
#ifndef NDEBUG
    fprintf(stderr, "[decoder_v4l2] handling resolution change\n");
#endif

    // Stop CAPTURE streaming
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(dec->fd, VIDIOC_STREAMOFF, &type);

    // Free old CAPTURE buffers
    for (int i = 0; i < dec->capture.count; i++) {
        if (dec->capture.buffers[i].frame) {
            vsl_frame_release(dec->capture.buffers[i].frame);
            dec->capture.buffers[i].frame     = NULL;
            dec->capture.buffers[i].dmabuf_fd = -1;
            dec->capture.buffers[i].queued    = false;
        }
    }

    // Release old buffers
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = 0;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_DMABUF;
    xioctl(dec->fd, VIDIOC_REQBUFS, &req);

    // Setup new CAPTURE queue with new resolution
    if (setup_capture_queue(dec) < 0) { return -1; }

    // Queue all new buffers
    if (queue_all_capture_buffers(dec) < 0) { return -1; }

    // Restart CAPTURE streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(dec->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_STREAMON CAPTURE failed after "
                "resolution change: %s\n",
                strerror(errno));
        return -1;
    }

    dec->source_change_pending = false;
    dec->initialized           = true;
    dec->streaming             = true;

#ifndef NDEBUG
    fprintf(stderr,
            "[decoder_v4l2] resolution change complete: %dx%d\n",
            dec->width,
            dec->height);
#endif

    return 0;
}

// Cleanup callback for decoded frames
struct v4l2_frame_cleanup_data {
    struct vsl_decoder_v4l2* decoder;
    int                      buffer_index;
};

static void
v4l2_frame_cleanup(VSLFrame* frame)
{
    if (!frame || !frame->userptr) { return; }

    struct v4l2_frame_cleanup_data* data = frame->userptr;

    // Re-queue the buffer for reuse
    if (data->decoder && data->buffer_index >= 0) {
        queue_capture_buffer(data->decoder, data->buffer_index);
    }

    free(data);
}

// Helper: Create output frame from dequeued capture buffer
// Returns frame on success, NULL on failure (buffer is re-queued on failure)
static VSLFrame*
create_output_frame(struct vsl_decoder_v4l2* dec, int cap_idx)
{
    VSLFrame* existing = dec->capture.buffers[cap_idx].frame;

    struct v4l2_frame_cleanup_data* cleanup_data =
        calloc(1, sizeof(*cleanup_data));
    if (!cleanup_data) {
        queue_capture_buffer(dec, cap_idx);
        return NULL;
    }

    cleanup_data->decoder      = dec;
    cleanup_data->buffer_index = cap_idx;

    VSLFrame* out = vsl_frame_init(dec->width,
                                   dec->height,
                                   dec->capture.stride,
                                   dec->out_fourcc,
                                   cleanup_data,
                                   v4l2_frame_cleanup);
    if (!out) {
        free(cleanup_data);
        queue_capture_buffer(dec, cap_idx);
        return NULL;
    }

    out->handle      = dec->capture.buffers[cap_idx].dmabuf_fd;
    out->info.width  = dec->width;
    out->info.height = dec->height;
    out->info.stride = dec->capture.stride;
    out->info.size   = dec->capture.plane_sizes[0];
    out->info.paddr  = vsl_frame_paddr(existing);

    return out;
}

// Helper: Start OUTPUT streaming if not already started
// Returns 0 on success, -1 on error
static int
start_output_streaming(struct vsl_decoder_v4l2* dec)
{
    if (dec->output_streaming) { return 0; }

    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (xioctl(dec->fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_STREAMON OUTPUT failed: %s\n",
                strerror(errno));
        return -1;
    }
    dec->output_streaming = true;
#ifndef NDEBUG
    fprintf(stderr, "[decoder_v4l2] OUTPUT streaming started\n");
#endif
    return 0;
}

// Helper: Try to dequeue a capture buffer and create output frame
// Returns frame on success, NULL if no frame available
static VSLFrame*
try_dequeue_capture_frame(struct vsl_decoder_v4l2* dec)
{
    if (!dec->streaming) { return NULL; }

    struct v4l2_buffer cap_buf;
    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_DMABUF;

    if (xioctl(dec->fd, VIDIOC_DQBUF, &cap_buf) != 0) { return NULL; }

    int cap_idx                          = (int) cap_buf.index;
    dec->capture.buffers[cap_idx].queued = false;

    if (cap_buf.flags & V4L2_BUF_FLAG_ERROR) {
        queue_capture_buffer(dec, cap_idx);
        return NULL;
    }

    return create_output_frame(dec, cap_idx);
}

// Helper: Drain events from V4L2 device
static void
drain_v4l2_events(struct vsl_decoder_v4l2* dec)
{
    struct v4l2_event event;
    memset(&event, 0, sizeof(event));
    while (xioctl(dec->fd, VIDIOC_DQEVENT, &event) == 0) {
        if (event.type == V4L2_EVENT_SOURCE_CHANGE) {
            dec->source_change_pending = true;
        }
    }
}

// Helper: Ensure CAPTURE streaming is started
static void
ensure_capture_streaming(struct vsl_decoder_v4l2* dec)
{
    if (dec->streaming || dec->capture.count == 0) { return; }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(dec->fd, VIDIOC_STREAMON, &type) == 0) {
        dec->streaming = true;
#ifndef NDEBUG
        fprintf(stderr, "[decoder_v4l2] CAPTURE streaming started\n");
#endif
    }
}

VSLDecoder*
vsl_decoder_create_v4l2(uint32_t codec, int fps)
{
    // Convert codec to V4L2 format
    uint32_t v4l2_codec = vsl_codec_to_v4l2_fmt(codec);
    if (v4l2_codec == 0) {
        fprintf(stderr, "[decoder_v4l2] unsupported codec: 0x%08x\n", codec);
        errno = EINVAL;
        return NULL;
    }

    // Allocate decoder structure
    // Note: We allocate space for the larger of the two decoder types
    // The actual structure used depends on which create function was called
    struct vsl_decoder_v4l2* dec = calloc(1, sizeof(struct vsl_decoder_v4l2));
    if (!dec) {
        fprintf(stderr,
                "[decoder_v4l2] failed to allocate decoder: %s\n",
                strerror(errno));
        return NULL;
    }

    dec->backend    = VSL_CODEC_BACKEND_V4L2;
    dec->fd         = -1;
    dec->fps        = fps;
    dec->out_fourcc = VSL_FOURCC('N', 'V', '1', '2');

    // Store codec type
    if (codec == VSL_FOURCC('H', '2', '6', '4')) {
        dec->codec = VSL_DEC_H264;
    } else {
        dec->codec = VSL_DEC_HEVC;
    }

    // Initialize capture buffer fds
    for (int i = 0; i < VSL_V4L2_DEC_CAPTURE_BUFFERS; i++) {
        dec->capture.buffers[i].dmabuf_fd = -1;
    }

    // Open V4L2 device
    dec->fd = open(VSL_V4L2_DECODER_DEV, O_RDWR | O_NONBLOCK);
    if (dec->fd < 0) {
        fprintf(stderr,
                "[decoder_v4l2] failed to open %s: %s\n",
                VSL_V4L2_DECODER_DEV,
                strerror(errno));
        free(dec);
        return NULL;
    }

    // Verify device capabilities
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if (xioctl(dec->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_QUERYCAP failed: %s\n",
                strerror(errno));
        close(dec->fd);
        free(dec);
        return NULL;
    }

    // Use device_caps if V4L2_CAP_DEVICE_CAPS is set, otherwise capabilities
    __u32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps
                                                           : cap.capabilities;

    if (!(caps & (V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE))) {
        fprintf(stderr, "[decoder_v4l2] device lacks M2M capability\n");
        close(dec->fd);
        free(dec);
        errno = ENODEV;
        return NULL;
    }

#ifndef NDEBUG
    fprintf(stderr,
            "[decoder_v4l2] opened %s: %s\n",
            VSL_V4L2_DECODER_DEV,
            cap.card);
#endif

    // Setup OUTPUT queue
    if (setup_output_queue(dec, v4l2_codec) < 0) {
        close(dec->fd);
        free(dec);
        return NULL;
    }

    // Subscribe to source change events BEFORE feeding any data
    // This is critical for proper resolution detection
    struct v4l2_event_subscription sub;
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_SOURCE_CHANGE;
    if (xioctl(dec->fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
#ifndef NDEBUG
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_SUBSCRIBE_EVENT failed: %s\n",
                strerror(errno));
#endif
        // Non-fatal - some drivers may not support events
    }

    // The vsi_v4l2 driver requires CAPTURE queue to be set up before
    // it will process OUTPUT buffers. Set up a preliminary CAPTURE queue
    // with whatever default resolution the driver reports. We'll reallocate
    // when we detect the actual resolution via SOURCE_CHANGE event.
    struct v4l2_format cap_fmt;
    memset(&cap_fmt, 0, sizeof(cap_fmt));
    cap_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(dec->fd, VIDIOC_G_FMT, &cap_fmt) == 0) {
        // Store default dimensions (may be placeholder like 48x48)
        dec->width          = cap_fmt.fmt.pix.width;
        dec->height         = cap_fmt.fmt.pix.height;
        dec->capture.stride = cap_fmt.fmt.pix.bytesperline;
        if (dec->capture.stride == 0) { dec->capture.stride = dec->width; }
        dec->capture.plane_sizes[0] = cap_fmt.fmt.pix.sizeimage;
        if (dec->capture.plane_sizes[0] == 0) {
            // Compute NV12 size: Y plane + UV plane (half height)
            dec->capture.plane_sizes[0] =
                dec->capture.stride * dec->height * 3 / 2;
        }

#ifndef NDEBUG
        fprintf(stderr,
                "[decoder_v4l2] default CAPTURE format: %dx%d stride=%d "
                "size=%zu\n",
                dec->width,
                dec->height,
                dec->capture.stride,
                dec->capture.plane_sizes[0]);
#endif

        // Request CAPTURE buffers (DMABUF import mode)
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count  = VSL_V4L2_DEC_CAPTURE_BUFFERS;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_DMABUF;

        if (xioctl(dec->fd, VIDIOC_REQBUFS, &req) == 0) {
            dec->capture.count = (int) req.count;
#ifndef NDEBUG
            fprintf(stderr,
                    "[decoder_v4l2] preliminary CAPTURE queue: %d buffers\n",
                    dec->capture.count);
#endif

            // Allocate and queue CAPTURE buffers
            int all_ok     = 1;
            int alloc_fail = -1; // Index where allocation failed
            for (int i = 0; i < dec->capture.count && all_ok; i++) {
                if (alloc_capture_frame(dec, i) < 0) {
                    all_ok     = 0;
                    alloc_fail = i;
                } else if (queue_capture_buffer(dec, i) < 0) {
                    all_ok     = 0;
                    alloc_fail = i + 1; // Frame allocated but queue failed
                }
            }

            // Cleanup on failure: release already-allocated frames
            if (!all_ok && alloc_fail > 0) {
                for (int i = 0; i < alloc_fail; i++) {
                    if (dec->capture.buffers[i].frame) {
                        vsl_frame_release(dec->capture.buffers[i].frame);
                        dec->capture.buffers[i].frame     = NULL;
                        dec->capture.buffers[i].dmabuf_fd = -1;
                        dec->capture.buffers[i].queued    = false;
                    }
                }
            }

            if (all_ok) {
                // Start both OUTPUT and CAPTURE streaming
                int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
                if (xioctl(dec->fd, VIDIOC_STREAMON, &type) == 0) {
                    dec->output_streaming = true;
#ifndef NDEBUG
                    fprintf(stderr,
                            "[decoder_v4l2] OUTPUT streaming started in "
                            "create\n");
#endif
                }

                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (xioctl(dec->fd, VIDIOC_STREAMON, &type) == 0) {
                    dec->streaming = true;
#ifndef NDEBUG
                    fprintf(stderr,
                            "[decoder_v4l2] CAPTURE streaming started in "
                            "create\n");
#endif
                }
            }
        }
    }

    return (VSLDecoder*) dec;
}

int
vsl_decoder_release_v4l2(VSLDecoder* decoder)
{
    if (!decoder) { return 0; }

    struct vsl_decoder_v4l2* dec = (struct vsl_decoder_v4l2*) decoder;

    // Stop streaming
    stop_streaming(dec);

    // Unmap OUTPUT buffers
    for (int i = 0; i < dec->output.count; i++) {
        if (dec->output.buffers[i].mmap_ptr &&
            dec->output.buffers[i].mmap_ptr != MAP_FAILED) {
            munmap(dec->output.buffers[i].mmap_ptr,
                   dec->output.buffers[i].mmap_size);
        }
    }

    // Release CAPTURE frame buffers
    for (int i = 0; i < dec->capture.count; i++) {
        if (dec->capture.buffers[i].frame) {
            vsl_frame_release(dec->capture.buffers[i].frame);
        }
    }

    // Close device
    if (dec->fd >= 0) { close(dec->fd); }

    free(dec);
    return 0;
}

VSLDecoderRetCode
vsl_decode_frame_v4l2(VSLDecoder*  decoder,
                      const void*  data,
                      unsigned int data_length,
                      size_t*      bytes_used,
                      VSLFrame**   output_frame)
{
    struct vsl_decoder_v4l2* dec      = (struct vsl_decoder_v4l2*) decoder;
    int                      ret_code = 0;

    *bytes_used   = 0;
    *output_frame = NULL;

    int64_t t_start = vsl_timestamp_us();

    // Handle pending resolution change (S1066: merged nested if)
    if (dec->source_change_pending && handle_resolution_change(dec) < 0) {
        return VSL_DEC_ERR;
    }

    // Find free OUTPUT buffer
    int out_idx = find_free_output_buffer(dec);
    if (out_idx < 0) {
        // All buffers queued - if streaming, try to dequeue
        if (dec->output_streaming) {
            // First, try to drain any decoded frames to free up CAPTURE buffers
            struct pollfd pfd = {.fd = dec->fd, .events = POLLIN | POLLOUT};
            poll(&pfd, 1, VSL_V4L2_POLL_TIMEOUT_MS);

            // If CAPTURE buffer available, return frame immediately
            if ((pfd.revents & POLLIN)) {
                VSLFrame* out = try_dequeue_capture_frame(dec);
                if (out) {
                    *output_frame = out;
                    ret_code |= VSL_DEC_FRAME_DEC;
                    dec->frames_decoded++;
                    return ret_code;
                }
            }

            // Try to dequeue an OUTPUT buffer
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buf.memory = V4L2_MEMORY_MMAP;

            if (xioctl(dec->fd, VIDIOC_DQBUF, &buf) == 0) {
                dec->output.buffers[buf.index].queued = false;
                out_idx                               = (int) buf.index;
            }
        }

        if (out_idx < 0) {
            // If not streaming yet, start streaming to let driver consume
            if (!dec->output_streaming && !dec->initialized) {
                if (start_output_streaming(dec) < 0) { return VSL_DEC_ERR; }
                return VSL_DEC_SUCCESS;
            }

            fprintf(stderr, "[decoder_v4l2] no OUTPUT buffer available\n");
            return VSL_DEC_ERR;
        }
    }

    // Copy compressed data to OUTPUT buffer
    size_t copy_len = data_length;
    if (copy_len > dec->output.buffers[out_idx].mmap_size) {
        copy_len = dec->output.buffers[out_idx].mmap_size;
    }
    memcpy(dec->output.buffers[out_idx].mmap_ptr, data, copy_len);

    // Queue OUTPUT buffer (works even before STREAMON)
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory    = V4L2_MEMORY_MMAP;
    buf.index     = out_idx;
    buf.bytesused = copy_len;
    buf.length    = dec->output.buffers[out_idx].mmap_size;

    if (xioctl(dec->fd, VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr,
                "[decoder_v4l2] VIDIOC_QBUF OUTPUT failed: %s\n",
                strerror(errno));
        return VSL_DEC_ERR;
    }
    dec->output.buffers[out_idx].queued = true;
    *bytes_used                         = copy_len;

    // Start OUTPUT streaming if not already started (fallback if create failed)
    if (start_output_streaming(dec) < 0) { return VSL_DEC_ERR; }

    // Check for events (resolution change)
    drain_v4l2_events(dec);

    // Handle resolution change - reallocate CAPTURE buffers with correct size
    if (dec->source_change_pending) {
        // Get actual resolution from driver
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (xioctl(dec->fd, VIDIOC_G_FMT, &fmt) == 0) {
            int new_width  = fmt.fmt.pix.width;
            int new_height = fmt.fmt.pix.height;

            // Only reallocate if resolution actually changed
            if (new_width != dec->width || new_height != dec->height ||
                !dec->initialized) {
#ifndef NDEBUG
                fprintf(stderr,
                        "[decoder_v4l2] resolution change: %dx%d -> %dx%d\n",
                        dec->width,
                        dec->height,
                        new_width,
                        new_height);
#endif
                if (handle_resolution_change(dec) < 0) { return VSL_DEC_ERR; }
                ret_code |= VSL_DEC_INIT_INFO;
            } else {
                // Resolution same, just clear the flag
                dec->source_change_pending = false;
            }
        }
    }

    // If not initialized and no source change yet, check format periodically
    if (!dec->initialized && !dec->source_change_pending) {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (xioctl(dec->fd, VIDIOC_G_FMT, &fmt) == 0) {
            // Check if resolution changed from initial default
            if ((int) fmt.fmt.pix.width != dec->width ||
                (int) fmt.fmt.pix.height != dec->height) {
                // Trigger resolution change handling
                dec->source_change_pending = true;
            }
        }
    }

    // Make sure CAPTURE streaming is on
    ensure_capture_streaming(dec);

    // Poll for decoded frame
    if (dec->streaming) {
        struct pollfd pfd      = {.fd = dec->fd, .events = POLLIN};
        int           poll_ret = poll(&pfd, 1, VSL_V4L2_POLL_TIMEOUT_MS);

        if (poll_ret > 0 && (pfd.revents & POLLIN)) {
            VSLFrame* out = try_dequeue_capture_frame(dec);
            if (out) {
                *output_frame = out;
                ret_code |= VSL_DEC_FRAME_DEC;
                dec->frames_decoded++;

                int64_t decode_time = vsl_timestamp_us() - t_start;
                dec->total_decode_time_us += decode_time;

#ifndef NDEBUG
                if (dec->frames_decoded <= 10 ||
                    dec->frames_decoded % 30 == 0) {
                    fprintf(stderr,
                            "[decoder_v4l2] frame %lu decoded in %lld us\n",
                            (unsigned long) dec->frames_decoded,
                            (long long) decode_time);
                }
#endif
            }
        }

        // Also check for and handle any events
        drain_v4l2_events(dec);
    }

    return ret_code;
}

// Accessor functions for V4L2 decoder
int
vsl_decoder_width_v4l2(const VSLDecoder* decoder_)
{
    const struct vsl_decoder_v4l2* dec =
        (const struct vsl_decoder_v4l2*) decoder_;
    return dec->width;
}

int
vsl_decoder_height_v4l2(const VSLDecoder* decoder_)
{
    const struct vsl_decoder_v4l2* dec =
        (const struct vsl_decoder_v4l2*) decoder_;
    return dec->height;
}

VSLRect
vsl_decoder_crop_v4l2(const VSLDecoder* decoder_)
{
    const struct vsl_decoder_v4l2* dec =
        (const struct vsl_decoder_v4l2*) decoder_;
    return dec->crop_region;
}
