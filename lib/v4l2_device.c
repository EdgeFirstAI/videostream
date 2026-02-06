// SPDX-License-Identifier: Apache-2.0
// Copyright © 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * @file v4l2_device.c
 * @brief V4L2 device discovery and enumeration implementation
 */

#include "v4l2_device.h"

#include "common.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/dma-heap.h>
#include <linux/videodev2.h>
#endif

/* ============================================================================
 * Internal Helpers
 * ============================================================================
 */

// EINTR-safe ioctl wrapper (pattern from v4l-utils)
static int
xioctl(int fd, unsigned long request, void* arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

// Check if a fourcc is a compressed video format
static bool
is_compressed(uint32_t fourcc)
{
    switch (fourcc) {
#ifdef __linux__
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_JPEG:
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_H264_NO_SC:
    case V4L2_PIX_FMT_H264_MVC:
    case V4L2_PIX_FMT_HEVC:
    case V4L2_PIX_FMT_VP8:
    case V4L2_PIX_FMT_VP9:
    case V4L2_PIX_FMT_MPEG1:
    case V4L2_PIX_FMT_MPEG2:
    case V4L2_PIX_FMT_MPEG4:
#endif
        return true;
    default:
        // Check for common fourcc codes not in all kernel headers
        if (fourcc == VSL_FOURCC('H', '2', '6', '4') ||
            fourcc == VSL_FOURCC('H', 'E', 'V', 'C') ||
            fourcc == VSL_FOURCC('V', 'P', '8', '0') ||
            fourcc == VSL_FOURCC('V', 'P', '9', '0') ||
            fourcc == VSL_FOURCC('M', 'J', 'P', 'G') ||
            fourcc == VSL_FOURCC('J', 'P', 'E', 'G')) {
            return true;
        }
        return false;
    }
}

#ifdef __linux__

// Get device capabilities, handling V4L2_CAP_DEVICE_CAPS
static uint32_t
get_device_caps(const struct v4l2_capability* cap)
{
    return (cap->capabilities & V4L2_CAP_DEVICE_CAPS) ? cap->device_caps
                                                      : cap->capabilities;
}

// Check if device has M2M capability
static bool
has_m2m_cap(uint32_t caps)
{
    return caps & (V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE);
}

// Check if device has capture capability
static bool
has_capture_cap(uint32_t caps)
{
    return caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE |
                   V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE);
}

// Check if device has output capability
static bool
has_output_cap(uint32_t caps)
{
    return caps & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
                   V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE);
}

// Check if device uses multiplanar API
static bool
is_multiplanar(uint32_t caps)
{
    return caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
                   V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE);
}

// Get buffer type for capture queue
static uint32_t
get_capture_buf_type(uint32_t caps)
{
    if (caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE))
        return V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    return V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

// Get buffer type for output queue
static uint32_t
get_output_buf_type(uint32_t caps)
{
    if (caps & (V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE))
        return V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    return V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

// Detect memory capabilities for a buffer type
static VSLMemoryType
detect_memory_caps(int fd, uint32_t buf_type)
{
    VSLMemoryType              mem_caps = 0;
    struct v4l2_requestbuffers req;

    // Try MMAP
    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = buf_type;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == 0) {
        mem_caps |= VSL_V4L2_MEM_MMAP;
        // Clean up
        req.count = 0;
        xioctl(fd, VIDIOC_REQBUFS, &req);
    }

    // Try USERPTR
    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = buf_type;
    req.memory = V4L2_MEMORY_USERPTR;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == 0) {
        mem_caps |= VSL_V4L2_MEM_USERPTR;
        req.count = 0;
        xioctl(fd, VIDIOC_REQBUFS, &req);
    }

    // Try DMABUF
    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = buf_type;
    req.memory = V4L2_MEMORY_DMABUF;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == 0) {
        mem_caps |= VSL_V4L2_MEM_DMABUF;
        req.count = 0;
        xioctl(fd, VIDIOC_REQBUFS, &req);
    }

    return mem_caps;
}

// Enumerate formats for a buffer type
static int
enum_formats_for_type(int         fd,
                      uint32_t    buf_type,
                      VSLFormat** out_formats,
                      size_t*     out_count)
{
    VSLFormat*          formats  = NULL;
    size_t              count    = 0;
    size_t              capacity = 16;
    struct v4l2_fmtdesc fmtdesc;

    formats = calloc(capacity, sizeof(VSLFormat));
    if (!formats) { return -1; }

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type  = buf_type;
    fmtdesc.index = 0;

    while (xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        // Grow array if needed
        if (count >= capacity) {
            capacity *= 2;
            VSLFormat* tmp = realloc(formats, capacity * sizeof(VSLFormat));
            if (!tmp) {
                free(formats);
                return -1;
            }
            formats = tmp;
        }

        // Fill format descriptor
        VSLFormat* fmt = &formats[count];
        memset(fmt, 0, sizeof(*fmt));
        fmt->fourcc = fmtdesc.pixelformat;
        vsl_strcpy_s(fmt->description,
                     sizeof(fmt->description),
                     (char*) fmtdesc.description);
        fmt->flags      = fmtdesc.flags;
        fmt->compressed = is_compressed(fmtdesc.pixelformat);

        count++;
        fmtdesc.index++;
    }

    // EINVAL means we've enumerated all formats (normal termination)
    if (errno != EINVAL && count == 0) {
        free(formats);
        return -1;
    }

    *out_formats = formats;
    *out_count   = count;
    return 0;
}

// Check if format list has compressed formats
static bool
has_compressed_formats(const VSLFormat* formats, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (formats[i].compressed) { return true; }
    }
    return false;
}

// Classify device type based on capabilities and formats
static VSLDeviceType
classify_device(uint32_t         caps,
                const VSLFormat* capture_fmts,
                size_t           capture_count,
                const VSLFormat* output_fmts,
                size_t           output_count)
{
    if (has_m2m_cap(caps)) {
        bool capture_compressed =
            has_compressed_formats(capture_fmts, capture_count);
        bool output_compressed =
            has_compressed_formats(output_fmts, output_count);

        if (capture_compressed && !output_compressed) {
            // Compressed output, raw input → Encoder
            return VSL_V4L2_TYPE_ENCODER;
        } else if (output_compressed && !capture_compressed) {
            // Compressed input, raw output → Decoder
            return VSL_V4L2_TYPE_DECODER;
        } else if (!capture_compressed && !output_compressed) {
            // Raw on both sides → ISP/scaler
            return VSL_V4L2_TYPE_ISP;
        } else {
            // Both compressed? Generic M2M
            return VSL_V4L2_TYPE_M2M;
        }
    }

    if (caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        return VSL_V4L2_TYPE_CAMERA;
    }

    if (caps & (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE)) {
        return VSL_V4L2_TYPE_OUTPUT;
    }

    return 0;
}

// Probe a single device and fill VSLDevice structure
static int
probe_device(const char* path, VSLDevice* device)
{
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        // EBUSY is not an error - device is in use
        return (errno == EBUSY) ? 0 : -1;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close(fd);
        return -1;
    }

    // Fill basic info
    memset(device, 0, sizeof(*device));
    vsl_strcpy_s(device->path, sizeof(device->path), path);
    vsl_strcpy_s(device->driver, sizeof(device->driver), (char*) cap.driver);
    vsl_strcpy_s(device->card, sizeof(device->card), (char*) cap.card);
    vsl_strcpy_s(device->bus_info, sizeof(device->bus_info), (char*) cap.bus_info);

    device->caps        = get_device_caps(&cap);
    device->multiplanar = is_multiplanar(device->caps);

    // Check if this is a video device we care about
    if (!has_capture_cap(device->caps) && !has_output_cap(device->caps)) {
        close(fd);
        return 0; // Not a video device we handle
    }

    // Enumerate formats
    if (has_capture_cap(device->caps)) {
        uint32_t buf_type = get_capture_buf_type(device->caps);
        enum_formats_for_type(fd,
                              buf_type,
                              &device->capture_formats,
                              &device->num_capture_formats);
        device->capture_mem = detect_memory_caps(fd, buf_type);
    }

    if (has_output_cap(device->caps)) {
        uint32_t buf_type = get_output_buf_type(device->caps);
        enum_formats_for_type(fd,
                              buf_type,
                              &device->output_formats,
                              &device->num_output_formats);
        device->output_mem = detect_memory_caps(fd, buf_type);
    }

    // Classify device type
    device->device_type = classify_device(device->caps,
                                          device->capture_formats,
                                          device->num_capture_formats,
                                          device->output_formats,
                                          device->num_output_formats);

    close(fd);
    return 1; // Device successfully probed
}

// Compare function for sorting devices by path
static int
device_path_cmp(const void* a, const void* b)
{
    const VSLDevice* da = a;
    const VSLDevice* db = b;
    return strcmp(da->path, db->path);
}

#endif /* __linux__ */

/* ============================================================================
 * Public API Implementation
 * ============================================================================
 */

VSL_API
VSLDeviceList*
vsl_v4l2_enumerate(void)
{
    return vsl_v4l2_enumerate_type(VSL_V4L2_TYPE_ANY);
}

VSL_API
VSLDeviceList*
vsl_v4l2_enumerate_type(VSLDeviceType type_mask)
{
#ifndef __linux__
    errno = ENOTSUP;
    return NULL;
#else
    VSLDeviceList* list = calloc(1, sizeof(VSLDeviceList));
    if (!list) { return NULL; }

    // Scan /dev for video devices
    DIR* dir = opendir("/dev");
    if (!dir) {
        free(list);
        return NULL;
    }

    // First pass: count devices
    size_t     capacity = 32;
    VSLDevice* devices  = calloc(capacity, sizeof(VSLDevice));
    if (!devices) {
        closedir(dir);
        free(list);
        return NULL;
    }

    size_t         count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Look for video* devices
        if (strncmp(entry->d_name, "video", 5) != 0) { continue; }

        // Skip non-numeric suffixes
        const char* suffix = entry->d_name + 5;
        char*       endptr;
        strtol(suffix, &endptr, 10);
        if (*endptr != '\0') { continue; }

        // Build full path
        char path[64];
        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);

        // Check if it's a character device
        struct stat st;
        if (stat(path, &st) < 0 || !S_ISCHR(st.st_mode)) { continue; }

        // Grow array if needed
        if (count >= capacity) {
            capacity *= 2;
            VSLDevice* tmp = realloc(devices, capacity * sizeof(VSLDevice));
            if (!tmp) { break; }
            devices = tmp;
        }

        // Probe the device
        int result = probe_device(path, &devices[count]);
        if (result > 0) {
            // Filter by type mask
            if (type_mask == VSL_V4L2_TYPE_ANY ||
                (devices[count].device_type & type_mask)) {
                count++;
            } else {
                // Free formats if filtered out
                free(devices[count].capture_formats);
                free(devices[count].output_formats);
            }
        }
    }

    closedir(dir);

    // Sort by path for consistent ordering
    if (count > 1) {
        qsort(devices, count, sizeof(VSLDevice), device_path_cmp);
    }

    list->devices = devices;
    list->count   = count;
    return list;
#endif
}

VSL_API
void
vsl_v4l2_device_list_free(VSLDeviceList* list)
{
    if (!list) { return; }

    for (size_t i = 0; i < list->count; i++) {
        VSLDevice* dev = &list->devices[i];

        // Free capture formats and their resolutions
        if (dev->capture_formats) {
            for (size_t j = 0; j < dev->num_capture_formats; j++) {
                free(dev->capture_formats[j].resolutions);
            }
            free(dev->capture_formats);
        }

        // Free output formats and their resolutions
        if (dev->output_formats) {
            for (size_t j = 0; j < dev->num_output_formats; j++) {
                free(dev->output_formats[j].resolutions);
            }
            free(dev->output_formats);
        }
    }

    free(list->devices);
    free(list);
}

// Static buffer for find functions (thread-local for safety)
static _Thread_local char found_path[64];

VSL_API
const char*
vsl_v4l2_find_encoder(uint32_t codec_fourcc)
{
    VSLDeviceList* list = vsl_v4l2_enumerate_type(VSL_V4L2_TYPE_ENCODER);
    if (!list) { return NULL; }

    const char* result = NULL;
    for (size_t i = 0; i < list->count && !result; i++) {
        VSLDevice* dev = &list->devices[i];
        // Check capture formats (encoder output = capture queue)
        for (size_t j = 0; j < dev->num_capture_formats; j++) {
            if (dev->capture_formats[j].fourcc == codec_fourcc) {
                vsl_strcpy_s(found_path, sizeof(found_path), dev->path);
                result = found_path;
                break;
            }
        }
    }

    vsl_v4l2_device_list_free(list);
    return result;
}

VSL_API
const char*
vsl_v4l2_find_decoder(uint32_t codec_fourcc)
{
    VSLDeviceList* list = vsl_v4l2_enumerate_type(VSL_V4L2_TYPE_DECODER);
    if (!list) { return NULL; }

    const char* result = NULL;
    for (size_t i = 0; i < list->count && !result; i++) {
        VSLDevice* dev = &list->devices[i];
        // Check output formats (decoder input = output queue)
        for (size_t j = 0; j < dev->num_output_formats; j++) {
            if (dev->output_formats[j].fourcc == codec_fourcc) {
                vsl_strcpy_s(found_path, sizeof(found_path), dev->path);
                result = found_path;
                break;
            }
        }
    }

    vsl_v4l2_device_list_free(list);
    return result;
}

VSL_API
const char*
vsl_v4l2_find_camera(uint32_t format_fourcc)
{
    return vsl_v4l2_find_camera_with_resolution(format_fourcc, 0, 0);
}

VSL_API
const char*
vsl_v4l2_find_camera_with_resolution(uint32_t format_fourcc,
                                     uint32_t width,
                                     uint32_t height)
{
    VSLDeviceList* list = vsl_v4l2_enumerate_type(VSL_V4L2_TYPE_CAMERA);
    if (!list) { return NULL; }

    const char* result = NULL;
    for (size_t i = 0; i < list->count && !result; i++) {
        VSLDevice* dev = &list->devices[i];
        // Check capture formats
        for (size_t j = 0; j < dev->num_capture_formats; j++) {
            if (dev->capture_formats[j].fourcc == format_fourcc) {
                // If no resolution specified, accept any
                if (width == 0 && height == 0) {
                    vsl_strcpy_s(found_path, sizeof(found_path), dev->path);
                    result = found_path;
                    break;
                }
                // TODO: Check resolutions when resolution enumeration is
                // implemented
                vsl_strcpy_s(found_path, sizeof(found_path), dev->path);
                result = found_path;
                break;
            }
        }
    }

    vsl_v4l2_device_list_free(list);
    return result;
}

VSL_API
int
vsl_v4l2_device_enum_formats(VSLDevice* device)
{
    // Formats are already enumerated during probe
    // This function is provided for explicit re-enumeration if needed
    if (!device || !device->path[0]) {
        errno = EINVAL;
        return -1;
    }

#ifndef __linux__
    errno = ENOTSUP;
    return -1;
#else
    int fd = open(device->path, O_RDWR | O_NONBLOCK);
    if (fd < 0) { return -1; }

    // Free existing formats
    if (device->capture_formats) {
        for (size_t i = 0; i < device->num_capture_formats; i++) {
            free(device->capture_formats[i].resolutions);
        }
        free(device->capture_formats);
        device->capture_formats     = NULL;
        device->num_capture_formats = 0;
    }
    if (device->output_formats) {
        for (size_t i = 0; i < device->num_output_formats; i++) {
            free(device->output_formats[i].resolutions);
        }
        free(device->output_formats);
        device->output_formats     = NULL;
        device->num_output_formats = 0;
    }

    // Re-enumerate
    if (has_capture_cap(device->caps)) {
        uint32_t buf_type = get_capture_buf_type(device->caps);
        enum_formats_for_type(fd,
                              buf_type,
                              &device->capture_formats,
                              &device->num_capture_formats);
    }

    if (has_output_cap(device->caps)) {
        uint32_t buf_type = get_output_buf_type(device->caps);
        enum_formats_for_type(fd,
                              buf_type,
                              &device->output_formats,
                              &device->num_output_formats);
    }

    close(fd);
    return 0;
#endif
}

VSL_API
VSLResolution*
vsl_v4l2_enum_resolutions(const VSLDevice* device,
                          uint32_t         fourcc,
                          size_t*          count)
{
    if (!device || !count) {
        errno = EINVAL;
        return NULL;
    }
    *count = 0;

#ifndef __linux__
    errno = ENOTSUP;
    return NULL;
#else
    int fd = open(device->path, O_RDWR | O_NONBLOCK);
    if (fd < 0) { return NULL; }

    VSLResolution* resolutions = NULL;
    size_t         res_count   = 0;
    size_t         capacity    = 16;

    resolutions = calloc(capacity, sizeof(VSLResolution));
    if (!resolutions) {
        close(fd);
        return NULL;
    }

    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = fourcc;
    frmsize.index        = 0;

    while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            // Grow array if needed
            if (res_count >= capacity) {
                capacity *= 2;
                VSLResolution* tmp =
                    realloc(resolutions, capacity * sizeof(VSLResolution));
                if (!tmp) { break; }
                resolutions = tmp;
            }

            VSLResolution* res = &resolutions[res_count];
            memset(res, 0, sizeof(*res));
            res->width  = frmsize.discrete.width;
            res->height = frmsize.discrete.height;

            // Enumerate frame intervals for this resolution
            struct v4l2_frmivalenum frmival;
            memset(&frmival, 0, sizeof(frmival));
            frmival.pixel_format = fourcc;
            frmival.width        = res->width;
            frmival.height       = res->height;
            frmival.index        = 0;

            while (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0 &&
                   res->num_frame_rates < VSL_V4L2_MAX_FRAMERATES) {
                if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    res->frame_rates[res->num_frame_rates].numerator =
                        frmival.discrete.numerator;
                    res->frame_rates[res->num_frame_rates].denominator =
                        frmival.discrete.denominator;
                    res->num_frame_rates++;
                }
                frmival.index++;
            }

            res_count++;
        } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                   frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
            // For stepwise/continuous, just report min and max
            if (res_count + 2 > capacity) {
                capacity = res_count + 16;
                VSLResolution* tmp =
                    realloc(resolutions, capacity * sizeof(VSLResolution));
                if (!tmp) { break; }
                resolutions = tmp;
            }

            // Min resolution
            VSLResolution* res_min = &resolutions[res_count++];
            memset(res_min, 0, sizeof(*res_min));
            res_min->width  = frmsize.stepwise.min_width;
            res_min->height = frmsize.stepwise.min_height;

            // Max resolution
            VSLResolution* res_max = &resolutions[res_count++];
            memset(res_max, 0, sizeof(*res_max));
            res_max->width  = frmsize.stepwise.max_width;
            res_max->height = frmsize.stepwise.max_height;

            break; // Only one stepwise entry
        }
        frmsize.index++;
    }

    close(fd);

    if (res_count == 0) {
        free(resolutions);
        return NULL;
    }

    *count = res_count;
    return resolutions;
#endif
}

VSL_API
bool
vsl_v4l2_device_supports_format(const VSLDevice* device,
                                uint32_t         fourcc,
                                bool             capture)
{
    if (!device) { return false; }

    const VSLFormat* formats =
        capture ? device->capture_formats : device->output_formats;
    size_t count =
        capture ? device->num_capture_formats : device->num_output_formats;

    for (size_t i = 0; i < count; i++) {
        if (formats[i].fourcc == fourcc) { return true; }
    }
    return false;
}

/* ============================================================================
 * Memory Allocation
 * ============================================================================
 */

VSL_API
void*
vsl_v4l2_alloc_userptr(size_t size, int* dma_fd)
{
    if (!dma_fd || size == 0) {
        errno = EINVAL;
        return NULL;
    }
    *dma_fd = -1;

#ifndef __linux__
    errno = ENOTSUP;
    return NULL;
#else
    // Try DMA heap paths in order of preference
    static const char* heap_paths[] = {
        "/dev/dma_heap/linux,cma-uncached",
        "/dev/dma_heap/linux,cma",
        "/dev/dma_heap/system",
        NULL,
    };

    int heap_fd = -1;
    for (int i = 0; heap_paths[i] && heap_fd < 0; i++) {
        heap_fd = open(heap_paths[i], O_RDWR);
    }

    if (heap_fd < 0) {
        // No DMA heap available
        return NULL;
    }

    // Allocate from DMA heap
    struct dma_heap_allocation_data alloc = {
        .len        = size,
        .fd_flags   = O_RDWR | O_CLOEXEC,
        .heap_flags = 0,
    };

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
        close(heap_fd);
        return NULL;
    }

    close(heap_fd);

    // Map the buffer
    void* ptr =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
    if (ptr == MAP_FAILED) {
        close(alloc.fd);
        return NULL;
    }

    *dma_fd = alloc.fd;
    return ptr;
#endif
}

VSL_API
void
vsl_v4l2_free_userptr(void* ptr, size_t size, int dma_fd)
{
    if (ptr && size > 0) { munmap(ptr, size); }
    if (dma_fd >= 0) { close(dma_fd); }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================
 */

VSL_API
const char*
vsl_v4l2_device_type_name(VSLDeviceType type)
{
    switch (type) {
    case VSL_V4L2_TYPE_CAMERA:
        return "Camera";
    case VSL_V4L2_TYPE_OUTPUT:
        return "Output";
    case VSL_V4L2_TYPE_ENCODER:
        return "Encoder";
    case VSL_V4L2_TYPE_DECODER:
        return "Decoder";
    case VSL_V4L2_TYPE_ISP:
        return "ISP";
    case VSL_V4L2_TYPE_M2M:
        return "M2M";
    default:
        return "Unknown";
    }
}

VSL_API
bool
vsl_v4l2_is_compressed_format(uint32_t fourcc)
{
    return is_compressed(fourcc);
}

VSL_API
char*
vsl_v4l2_fourcc_to_string(uint32_t fourcc, char buf[5])
{
    buf[0] = (fourcc >> 0) & 0xFF;
    buf[1] = (fourcc >> 8) & 0xFF;
    buf[2] = (fourcc >> 16) & 0xFF;
    buf[3] = (fourcc >> 24) & 0xFF;
    buf[4] = '\0';
    return buf;
}
