// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "dma-buf.h"
#include "v4l2.h"
#include <linux/videodev2.h>
#include <stdatomic.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

VSL_API
void*
vsl_camera_buffer_mmap(vsl_camera_buffer* buffer)
{
    return buffer->mmap;
}

VSL_API
int
vsl_camera_buffer_dma_fd(const vsl_camera_buffer* buffer)
{
    return buffer->dmafd;
}

VSL_API
u_int64_t
vsl_camera_buffer_phys_addr(const vsl_camera_buffer* buffer)
{
    return buffer->phys_addr;
}

VSL_API
u_int32_t
vsl_camera_buffer_length(const vsl_camera_buffer* buffer)
{
    return buffer->length;
}

VSL_API
u_int32_t
vsl_camera_buffer_fourcc(const vsl_camera_buffer* buffer)
{
    return buffer->fourcc;
}

static int
xioctl(int fh, int request, void* arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static unsigned long
get_paddr(int dma_fd)
{
    struct dma_buf_phys dma_phys;
    CLEAR(dma_phys);
    if (ioctl(dma_fd, DMA_BUF_IOCTL_PHYS, &dma_phys)) {
        fprintf(stderr,
                "DMA_BUF_IOCTL_PHYS ioctl error: %s\n",
                strerror(errno));
        return 0;
    }
    return dma_phys.phys;
}

VSL_API
int
vsl_camera_get_queued_buf_count(const vsl_camera* ctx)
{
    return ctx->queued_buf_count;
}

static vsl_camera_buffer*
read_frame(vsl_camera* ctx)
{
    struct v4l2_buffer buf;
    struct v4l2_plane  mplanes;

    CLEAR(buf);
    CLEAR(mplanes);

    if (ctx->not_plane) {
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
    } else {
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.m.planes = &mplanes;
        buf.length   = 1;
        // buf.index should not be set for VIDIOC_DQBUF - it's an output
        // parameter buf.index    = 0;
    }

    if (-1 == xioctl(ctx->fd, VIDIOC_DQBUF, &buf)) {
        perror("VIDIOC_DQBUF");
        return NULL;
    }

    ctx->queued_buf_count--;
    assert(buf.index < ctx->n_buffers);

    vsl_camera_buffer* vslbuf = &ctx->buffers[buf.index];
    memcpy(&vslbuf->timestamp, &buf.timestamp, sizeof(struct timeval));

    return vslbuf;
}

VSL_API
void
vsl_camera_buffer_timestamp(const vsl_camera_buffer* buffer,
                            int64_t*                 seconds,
                            int64_t*                 nanoseconds)
{
    if (seconds) { *seconds = buffer->timestamp.tv_sec; }
    if (nanoseconds) { *nanoseconds = buffer->timestamp.tv_usec * 1000; }
}

VSL_API
int
vsl_camera_release_buffer(vsl_camera*                     ctx,
                          const struct vsl_camera_buffer* buffer)
{
    if (-1 ==
        xioctl(ctx->fd, VIDIOC_QBUF, &(ctx->v4l2_buffers[buffer->bufID]))) {
#ifndef NDEBUG
        perror("VIDIOC_QBUF");
#endif
        return -1;
    }
    ctx->queued_buf_count++;
    return 0;
}

VSL_API
vsl_camera_buffer*
vsl_camera_get_data(vsl_camera* ctx)
{
    fd_set         fds;
    struct timeval tv;
    int            r;

    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);

    /* Timeout. */
    tv.tv_sec  = 2;
    tv.tv_usec = 0;

    r = select(ctx->fd + 1, &fds, NULL, NULL, &tv);

    if (-1 == r) { return NULL; }

    if (0 == r) {
        fprintf(stderr, "Camera timeout");
        return NULL;
    }
    return read_frame(ctx);
    /* EAGAIN - continue select loop. */
}

VSL_API
int
vsl_camera_stop_capturing(const vsl_camera* ctx)
{
    enum v4l2_buf_type type;
    type = ctx->not_plane ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                          : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (-1 == xioctl(ctx->fd, VIDIOC_STREAMOFF, &type)) return -1;
    return 0;
}

VSL_API
int
vsl_camera_start_capturing(vsl_camera* ctx)
{
    for (unsigned int i = 0; i < ctx->n_buffers; ++i) {

        if (-1 == xioctl(ctx->fd, VIDIOC_QBUF, &(ctx->v4l2_buffers[i])))
            return -1;
        ctx->queued_buf_count++;
    }

    enum v4l2_buf_type type;
    if (ctx->not_plane)
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    else
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (-1 == xioctl(ctx->fd, VIDIOC_STREAMON, &type)) return -1;

    return 0;
}

VSL_API
void
vsl_camera_uninit_device(vsl_camera* ctx)
{
    const char* debug = getenv("VSL_DEBUG");

    for (unsigned int i = 0; i < ctx->n_buffers; i++) {
        if (ctx->buffers[i].dmafd >= 0 && -1 == close(ctx->buffers[i].dmafd) &&
            debug && *debug == '1') {
            perror("Could not close DMA file descriptor");
        }

        if (-1 == munmap(ctx->buffers[i].mmap, ctx->buffers[i].length) &&
            debug && *debug == '1') {
            perror("Could not munmap buffer");
        }
    }
    free(ctx->buffers);
    if (!ctx->not_plane) {
        for (unsigned int i = 0; i < ctx->n_buffers; i++) {
            free(ctx->v4l2_buffers[i].m.planes);
        }
    }
    free(ctx->v4l2_buffers);
}

static int
buffer_export_mp(int                v4lfd,
                 enum v4l2_buf_type bt,
                 int                index,
                 int                dmafd[],
                 int                n_planes)
{
    for (int i = 0; i < n_planes; ++i) {
        struct v4l2_exportbuffer expbuf;

        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type  = bt;
        expbuf.index = index;
        expbuf.plane = i;
        expbuf.flags = O_RDWR;
        if (ioctl(v4lfd, VIDIOC_EXPBUF, &expbuf) == -1) {
            perror("VIDIOC_EXPBUF");
            for (int j = 0; j < i; j++) { close(dmafd[j]); }
            return -1;
        }
        if (errno == ENOTTY) {
            // DMA buffers not supported
            return -1;
        }
        dmafd[i] = expbuf.fd;
    }

    return 0;
}

static int
buffer_export(int v4lfd, enum v4l2_buf_type bt, int index, int* dmafd)
{
    struct v4l2_exportbuffer expbuf;

    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type  = bt;
    expbuf.index = index;
    expbuf.flags = O_RDWR;
    if (ioctl(v4lfd, VIDIOC_EXPBUF, &expbuf) == -1) {
        perror("VIDIOC_EXPBUF");
        return -1;
    }
    if (errno == ENOTTY) {
        // DMA buffers not supported
        return -1;
    }

    *dmafd = expbuf.fd;
    return 0;
}

static int
init_dma_(vsl_camera* ctx, int* buf_count)
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count  = *buf_count;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(ctx->fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr,
                    "%s does not support "
                    "memory mapping\n",
                    ctx->dev_name);
            return -1;
        } else {
            return -1;
        }
    }

    *buf_count     = req.count;
    ctx->n_buffers = req.count;
    ctx->buffers =
        (struct vsl_camera_buffer*) calloc(req.count,
                                           sizeof(struct vsl_camera_buffer));
    if (!ctx->buffers) {
        perror("Out of memory");
        return -1;
    }

    for (unsigned int i = 0; i < req.count; i++) {
        CLEAR(ctx->buffers[i]);
        ctx->buffers[i].dmafd = -1;
    }

    ctx->v4l2_buffers =
        (struct v4l2_buffer*) calloc(req.count, sizeof(struct v4l2_buffer));
    if (!ctx->v4l2_buffers) {
        perror("Out of memory");
        return -1;
    }
    for (unsigned int i = 0; i < req.count; i++) {
        CLEAR(ctx->v4l2_buffers[i]);
    }

    for (unsigned int i = 0; i < req.count; ++i) {
        ctx->v4l2_buffers[i].type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ctx->v4l2_buffers[i].memory = V4L2_MEMORY_MMAP;
        ctx->v4l2_buffers[i].index  = i;

        if (-1 == xioctl(ctx->fd, VIDIOC_QUERYBUF, &ctx->v4l2_buffers[i]))
            return -1;
        ctx->buffers[i].length = ctx->v4l2_buffers[i].length;
        ctx->buffers[i].bufID  = ctx->v4l2_buffers[i].index;
        ctx->buffers[i].mmap   = mmap(NULL /* start anywhere */,
                                    ctx->buffers[i].length,
                                    PROT_READ | PROT_WRITE /* required */,
                                    MAP_SHARED /* recommended */,
                                    ctx->fd,
                                    ctx->v4l2_buffers[i].m.offset);

        if (MAP_FAILED == ctx->buffers[i].mmap) {
            perror("mmap failed");
            return -1;
        }
        int dmafd = 0;
        if (buffer_export(ctx->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i, &dmafd) ==
            0) {
            ctx->buffers[i].dmafd     = dmafd;
            ctx->buffers[i].phys_addr = get_paddr(dmafd);
        }
    }
    return 0;
}

static int
init_dma_mplane(vsl_camera* ctx, int* buf_count)
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count  = *buf_count;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(ctx->fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr,
                    "%s does not support "
                    "memory mapping\n",
                    ctx->dev_name);
            return -1;
        } else {
            return -1;
        }
    }

    *buf_count     = req.count;
    ctx->n_buffers = req.count;
    ctx->buffers   = calloc(req.count, sizeof(struct vsl_camera_buffer));
    if (!ctx->buffers) {
        perror("Out of memory");
        return -1;
    }

    for (unsigned int i = 0; i < req.count; i++) {
        CLEAR(ctx->buffers[i]);
        ctx->buffers[i].dmafd = -1;
    }

    ctx->v4l2_buffers = calloc(req.count, sizeof(struct v4l2_buffer));
    if (!ctx->v4l2_buffers) {
        perror("Out of memory");
        return -1;
    }
    for (unsigned int i = 0; i < req.count; i++) {
        CLEAR(ctx->v4l2_buffers[i]);
        ctx->v4l2_buffers[i].m.planes = calloc(1, sizeof(struct v4l2_plane));
    }

    for (unsigned int i = 0; i < req.count; ++i) {

        ctx->v4l2_buffers[i].type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        ctx->v4l2_buffers[i].memory = V4L2_MEMORY_MMAP;
        ctx->v4l2_buffers[i].index  = i;
        ctx->v4l2_buffers[i].length = 1;
        if (-1 == xioctl(ctx->fd, VIDIOC_QUERYBUF, &ctx->v4l2_buffers[i]))
            return -1;
        ctx->buffers[i].length = ctx->v4l2_buffers[i].m.planes[0].length;
        ctx->buffers[i].bufID  = ctx->v4l2_buffers[i].index;
        ctx->buffers[i].mmap =
            mmap(NULL /* start anywhere */,
                 ctx->buffers[i].length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 ctx->fd,
                 ctx->v4l2_buffers[i].m.planes[0].m.mem_offset);

        if (MAP_FAILED == ctx->buffers[i].mmap) {
            perror("mmap failed");
            return -1;
        }
        int dmafd = 0;
        if (buffer_export_mp(ctx->fd,
                             V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                             i,
                             &dmafd,
                             1) == 0) {
            ctx->buffers[i].dmafd     = dmafd;
            ctx->buffers[i].phys_addr = get_paddr(dmafd);
        }
    }
    return 0;
}

static int
init_dma(vsl_camera* ctx, int* buf_count)
{
    if (ctx->not_plane) { return init_dma_(ctx, buf_count); }
    return init_dma_mplane(ctx, buf_count);
}

VSL_API
int
vsl_camera_enum_fmts(const vsl_camera* ctx, u_int32_t* codes, int size)
{
    struct v4l2_fmtdesc fmtdesc;
    CLEAR(fmtdesc);
    // Use multiplanar type for multiplanar devices, single-planar otherwise
    fmtdesc.type = ctx->not_plane ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                                  : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    int i        = 0;
    while (i < size) {
        fmtdesc.index = i;

        if (!ioctl(ctx->fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
#ifndef NDEBUG
            printf("%c%c%c%c fmtdesc: %s\n",
                   fmtdesc.pixelformat,
                   fmtdesc.pixelformat >> 8,
                   fmtdesc.pixelformat >> 16,
                   fmtdesc.pixelformat >> 24,
                   fmtdesc.description);
#endif
            codes[i] = fmtdesc.pixelformat;
            i++;
        } else {
            break;
        }
    }
    return i;
}

int
vsl_camera_enum_mplane_fmts(const vsl_camera* ctx, u_int32_t* codes, int size)
{
    struct v4l2_fmtdesc fmtdesc;
    CLEAR(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    int i        = 0;
    while (i < size) {
        fmtdesc.index = i;

        if (!ioctl(ctx->fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
#ifndef NDEBUG
            printf("%c%c%c%c fmtdesc: %s\n",
                   fmtdesc.pixelformat,
                   fmtdesc.pixelformat >> 8,
                   fmtdesc.pixelformat >> 16,
                   fmtdesc.pixelformat >> 24,
                   fmtdesc.description);
#endif
            codes[i] = fmtdesc.pixelformat;
            i++;
        } else {
            break;
        }
    }
    return i;
}

VSL_API
int
vsl_camera_is_dmabuf_supported(const vsl_camera* ctx)
{
    if (ctx->buffers[0].dmafd == -1 && ctx->buffers[0].phys_addr == 0) {
        return 0;
    }
    return 1;
}

static int
check_caps(vsl_camera* ctx)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap    cropcap;
    if (-1 == xioctl(ctx->fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is not a V4L2 device\n", ctx->dev_name);
            return -1;
        } else {
            return -1;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is not a video capture device\n", ctx->dev_name);
        return -1;
    }

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ctx->not_plane = 1;
    } else {
        ctx->not_plane = 0;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n", ctx->dev_name);
        return -1;
    }

    /* Select video input, video standard and tune here. */

    CLEAR(cropcap);
    cropcap.type = ctx->not_plane ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                                  : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (0 == xioctl(ctx->fd, VIDIOC_CROPCAP, &cropcap)) {
        struct v4l2_crop crop;
        CLEAR(crop);
        crop.type = cropcap.type;
        crop.c    = cropcap.defrect; /* reset to default */
        if (-1 == xioctl(ctx->fd, VIDIOC_S_CROP, &crop)) {
            /* Errors ignored. */
        }
    } else {
        /* Errors ignored. */
    }
    return 0;
}

VSL_API
int
vsl_camera_init_device(vsl_camera* ctx,
                       int*        width,
                       int*        height,
                       int*        buf_count,
                       u_int32_t*  fourcc)
{
    int err = check_caps(ctx);
    if (err) return err;

    struct v4l2_format fmt;

    CLEAR(fmt);
    if (ctx->not_plane) {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }
    errno = 0;
    if (-1 == xioctl(ctx->fd, VIDIOC_G_FMT, &fmt)) {
        fprintf(stderr, "VIDIOC_G_FMT ioctl error: %s\n", strerror(errno));
        return -1;
    }
#ifndef NDEBUG
    if (ctx->not_plane) {
        printf("The camera default resolution is: %dx%d with fourcc "
               "%c%c%c%c\n",
               fmt.fmt.pix.width,
               fmt.fmt.pix.height,
               fmt.fmt.pix.pixelformat,
               fmt.fmt.pix.pixelformat >> 8,
               fmt.fmt.pix.pixelformat >> 16,
               fmt.fmt.pix.pixelformat >> 24);
    } else {
        printf("The camera default resolution is: %dx%d with fourcc "
               "%c%c%c%c\n ",
               fmt.fmt.pix_mp.width,
               fmt.fmt.pix_mp.height,
               fmt.fmt.pix_mp.pixelformat,
               fmt.fmt.pix_mp.pixelformat >> 8,
               fmt.fmt.pix_mp.pixelformat >> 16,
               fmt.fmt.pix_mp.pixelformat >> 24);
    }
#endif

    if (ctx->not_plane) {
        if (*width > 0) fmt.fmt.pix.width = *width;
        if (*height > 0) fmt.fmt.pix.height = *height;
        if (*fourcc != 0) fmt.fmt.pix.pixelformat = *fourcc;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;

        // the driver will set these values
        fmt.fmt.pix.sizeimage    = 0;
        fmt.fmt.pix.bytesperline = 0;

    } else {
        if (*width > 0) fmt.fmt.pix_mp.width = *width;
        if (*height > 0) fmt.fmt.pix_mp.height = *height;
        if (*fourcc != 0) fmt.fmt.pix_mp.pixelformat = *fourcc;
        fmt.fmt.pix_mp.field      = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.num_planes = 1;

        // the driver will set these values
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage    = 0;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
    }

    if (-1 == xioctl(ctx->fd, VIDIOC_S_FMT, &fmt)) {
        switch (errno) {
        case EINVAL:
            fprintf(stderr,
                    "Video format %c%c%c%c not supported on %s\n",
                    *fourcc,
                    *fourcc >> 8,
                    *fourcc >> 16,
                    *fourcc >> 24,
                    ctx->dev_name);
            return -1;
        case EBUSY:
            fprintf(stderr,
                    "Device %s is busy : %i %s\n",
                    ctx->dev_name,
                    errno,
                    strerror(errno));
            return -1;
        default:
            return -1;
        }
    }

    if (ctx->not_plane) {
        *width  = fmt.fmt.pix.width;
        *height = fmt.fmt.pix.height;
        *fourcc = fmt.fmt.pix.pixelformat;
    } else {
        *width  = fmt.fmt.pix_mp.width;
        *height = fmt.fmt.pix_mp.height;
        *fourcc = fmt.fmt.pix_mp.pixelformat;
    }

    err = init_dma(ctx, buf_count);
    if (err) return -1;

    for (unsigned int i = 0; i < ctx->n_buffers; i++) {
        ctx->buffers[i].fourcc = ctx->not_plane ? fmt.fmt.pix.pixelformat
                                                : fmt.fmt.pix_mp.pixelformat;
    }

    return 0;
}

#define VIV_CUSTOM_CID_BASE (V4L2_CID_USER_BASE | 0xf000)
#define V4L2_CID_VIV_EXTCTRL (VIV_CUSTOM_CID_BASE + 1)

static int
isp_cam_json(const vsl_camera* ctx, char* json, int size)
{
    struct v4l2_ext_controls ecs;
    struct v4l2_ext_control  ec;
    memset(&ecs, 0, sizeof(ecs));
    memset(&ec, 0, sizeof(ec));
    ec.string    = json;
    ec.id        = V4L2_CID_VIV_EXTCTRL;
    ec.size      = size;
    ecs.controls = &ec;
    ecs.count    = 1;

    int ret = ioctl(ctx->fd, VIDIOC_S_EXT_CTRLS, &ecs);
    if (ret != 0) {
#ifndef NDEBUG
        fprintf(stderr,
                "Failed to set ext ctrl: %d, %s\n",
                errno,
                strerror(errno));
#endif
        return -1;
    }

    ioctl(ctx->fd, VIDIOC_G_EXT_CTRLS, &ecs);
#ifndef NDEBUG
    printf("json response: %s", ec.string);
#endif
    json[size - 1] = 0;
    strncpy(json, ec.string, size);
    if (json[size - 1] != 0) {
        json[size - 1] = 0;
        fprintf(stderr,
                "%s: response of length %d did not fit inside json buffer "
                "of length %d and was truncated\n",
                __FUNCTION__,
                ec.size,
                size);
    }
    return 0;
}

VSL_API
int
vsl_camera_mirror(const vsl_camera* ctx, bool mirror)
{
    struct v4l2_control ctrl;
    CLEAR(ctrl);
    ctrl.id    = V4L2_CID_HFLIP;
    ctrl.value = mirror;
    if (ioctl(ctx->fd, VIDIOC_S_CTRL, &ctrl)) {
        // try the ISP cam:
        char* buf = calloc(1024, sizeof(char));
        snprintf(buf,
                 1024,
                 "{\"id\": \"dwe.s.hflip\", \"dwe\" : {\"hflip\": %s}}",
                 mirror ? "true" : "false");
        if (isp_cam_json(ctx, buf, 1024)) {
            free(buf);
            fprintf(stderr, "Mirror failed");
            return -1;
        }
        free(buf);
    }
    return 0;
}

VSL_API
int
vsl_camera_mirror_v(const vsl_camera* ctx, bool mirror)
{
    struct v4l2_control ctrl;
    CLEAR(ctrl);
    ctrl.id    = V4L2_CID_VFLIP;
    ctrl.value = mirror;
    if (ioctl(ctx->fd, VIDIOC_S_CTRL, &ctrl)) {
        // try the ISP cam:
        char* buf = calloc(1024, sizeof(char));
        snprintf(buf,
                 1024,
                 "{\"id\": \"dwe.s.vflip\", \"dwe\" : {\"vflip\": %s}}",
                 mirror ? "true" : "false");
        if (isp_cam_json(ctx, buf, 1024)) {
            free(buf);
            fprintf(stderr, "Mirror_v failed");
            return -1;
        }
        free(buf);
    }
    return 0;
}

VSL_API
void
vsl_camera_close_device(vsl_camera* ctx)
{
    if (ctx->fd >= 0) { close(ctx->fd); }
    free(ctx);
}

VSL_API
vsl_camera*
vsl_camera_open_device(const char* filename)
{
    struct stat st;
    vsl_camera* ctx = (vsl_camera*) calloc(1, sizeof(vsl_camera));
    if (!ctx) {
        perror("Out of memory");
        return NULL;
    }

    ctx->fd       = -1;
    ctx->dev_name = filename;

    if (-1 == stat(ctx->dev_name, &st)) {
        fprintf(stderr,
                "Cannot identify '%s': %d, %s\n",
                ctx->dev_name,
                errno,
                strerror(errno));
        return NULL;
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr,
                "%s is not a device : %d, %s\n",
                ctx->dev_name,
                errno,
                strerror(errno));
        return NULL;
    }

    ctx->fd = open(ctx->dev_name, O_RDWR | O_NONBLOCK, 0);
    if (ctx->fd == -1) {
        fprintf(stderr,
                "Cannot open '%s': %d, %s\n",
                ctx->dev_name,
                errno,
                strerror(errno));
        return NULL;
    }

    // Acquire exclusive lock on camera device to prevent concurrent access
    // from multiple tests/processes. Lock is automatically released on close().
    if (flock(ctx->fd, LOCK_EX | LOCK_NB) == -1) {
        fprintf(stderr,
                "Cannot acquire exclusive lock on '%s': %s\n",
                ctx->dev_name,
                strerror(errno));
        fprintf(stderr, "Another process may be using the camera. ");
        fprintf(stderr,
                "If running tests, use --test-threads=1 to serialize.\n");
        close(ctx->fd);
        return NULL;
    }

    return ctx;
}