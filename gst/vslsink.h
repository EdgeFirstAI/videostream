// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSLSINK_H
#define VSLSINK_H

#include <stdbool.h>

#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_VSL_SINK (vsl_sink_get_type())
#define VSL_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_VSL_SINK, VslSink))
#define VSL_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_VSL_SINK, VslSinkClass))
#define IS_VSL_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_VSL_SINK))
#define IS_VSL_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_VSL_SINK))
#define VSL_SINK_CAST(obj) ((VslSink*) obj)

typedef struct vsl_host VSLHost;

// DMA buffer pool entry - pre-allocated and reused
typedef struct {
    int    dmabuf_fd; // DMA buffer file descriptor
    void*  map_ptr;   // mmap'd memory for CPU copy
    size_t map_size;  // Size of buffer
    bool   in_use;    // Currently referenced by a frame
} DmaBufPoolEntry;

// DMA buffer pool - avoids per-frame allocation
typedef struct {
    DmaBufPoolEntry* entries;
    size_t           count;    // Number of entries in pool
    size_t           next_idx; // Round-robin index for allocation
    GMutex           lock;
    bool             initialized;
} DmaBufPool;

typedef struct {
    GstVideoSink parent;
    GRecMutex    mutex;
    GstTask*     task;
    VSLHost*     host;
    gchar*       path;
    GstClockTime last_frame;
    int64_t      frame_number;
    int64_t      lifespan;
    size_t       n_sockets;
    int*         sockets;
    DmaBufPool   dmabuf_pool; // Pre-allocated DMA buffer pool
    size_t       pool_size;   // Configured pool size
} VslSink;

typedef struct {
    GstVideoSinkClass parent_class;
} VslSinkClass;

GType
vsl_sink_get_type(void);

#ifdef __cplusplus
}
#endif

#endif /* VSLSINK_H */
