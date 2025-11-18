// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSLSINK_H
#define VSLSINK_H

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
