// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <gst/allocators/allocators.h>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include "videostream.h"
#include "vslsink.h"

#define NANOS_PER_MILLI 1000000
#define DEFAULT_LIFESPAN 100
#define POLL_TIMEOUT_MS 1000

GST_DEBUG_CATEGORY_STATIC(vsl_sink_debug_category);
#define GST_CAT_DEFAULT vsl_sink_debug_category

#define vsl_sink_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE(
    VslSink,
    vsl_sink,
    GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT(vsl_sink_debug_category,
                            "vslsink",
                            0,
                            "debug category for vslsink element"));

#define VIDEO_SINK_CAPS                                                      \
    GST_VIDEO_CAPS_MAKE("{ NV12, YV12, I420, YUY2, YUYV, UYVY, RGBA, RGBx, " \
                        "RGB, BGRA, BGRx, BGR }")

typedef enum { PROP_0, PROP_PATH, PROP_LIFESPAN, N_PROPERTIES } VslSinkProperty;

static GParamSpec* properties[N_PROPERTIES] = {
    NULL,
};

static void
vsl_task(void* data)
{
    VslSink* vslsink = VSL_SINK(data);

    if (!vslsink->host) {
        GST_WARNING_OBJECT(vslsink, "vsl host unavailable");
        return;
    }

    if (!vslsink->sockets) {
        GST_ERROR_OBJECT(vslsink, "sockets buffer unavailable");
        return;
    }

    if (vsl_host_poll(vslsink->host, POLL_TIMEOUT_MS)) {
        size_t max_sockets;
        if (vsl_host_sockets(vslsink->host,
                             vslsink->n_sockets,
                             vslsink->sockets,
                             &max_sockets)) {
            if (errno == ENOBUFS) {
                vslsink->n_sockets = max_sockets * 2;
                vslsink->sockets =
                    realloc(vslsink->sockets, vslsink->n_sockets);
                return;
            } else {
                GST_ERROR_OBJECT(vslsink,
                                 "failed to query "
                                 "sockets: %s\n",
                                 strerror(errno));
                return;
            }
        }

        for (int i = 1; i < max_sockets; i++) {
            int err = vsl_host_service(vslsink->host, vslsink->sockets[i]);
            if (!err || errno == EPIPE) { continue; }
            GST_WARNING_OBJECT(vslsink,
                               "client %d error - %s",
                               vslsink->sockets[i],
                               strerror(errno));
        }
    }
}

static void
vsl_sink_init(VslSink* vslsink)
{
    vslsink->n_sockets = 32;
    vslsink->sockets   = calloc(vslsink->n_sockets, sizeof(int));
    vslsink->lifespan  = DEFAULT_LIFESPAN;
    g_rec_mutex_init(&vslsink->mutex);
    vslsink->task = gst_task_new(vsl_task, vslsink, NULL);
    gst_task_set_lock(vslsink->task, &vslsink->mutex);
}

static void
set_property(GObject*      object,
             guint         property_id,
             const GValue* value,
             GParamSpec*   pspec)
{
    VslSink* vslsink = VSL_SINK(object);

    switch ((VslSinkProperty) property_id) {
    case PROP_PATH:
        if (vslsink->path) {
            GST_WARNING_OBJECT(vslsink,
                               "cannot adjust path once set (currently: %s)",
                               vslsink->path);
            break;
        }

        vslsink->path = g_value_dup_string(value);
        break;
    case PROP_LIFESPAN:
        vslsink->lifespan = g_value_get_int64(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
get_property(GObject*    object,
             guint       property_id,
             GValue*     value,
             GParamSpec* pspec)
{
    VslSink* vslsink = VSL_SINK(object);

    switch ((VslSinkProperty) property_id) {
    case PROP_PATH:
        g_value_set_string(value, vslsink->path);
        break;
    case PROP_LIFESPAN:
        g_value_set_int64(value, vslsink->lifespan);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
dispose(GObject* object)
{
    VslSink* vslsink = VSL_SINK(object);
    GST_LOG_OBJECT(vslsink, "dispose");

    if (vslsink->host) {
        gst_task_stop(vslsink->task);
        gst_task_join(vslsink->task);
        vsl_host_release(vslsink->host);
        vslsink->host = NULL;
    }

    gst_object_unref(vslsink->task);

    if (vslsink->sockets) { free(vslsink->sockets); }

    if (vslsink->path) { free(vslsink->path); }

    g_rec_mutex_clear(&vslsink->mutex);

    G_OBJECT_CLASS(vsl_sink_parent_class)->dispose(object);
}

static void
finalize(GObject* object)
{
    VslSink* vslsink = VSL_SINK(object);
    GST_LOG_OBJECT(vslsink, "finalize");
    G_OBJECT_CLASS(vsl_sink_parent_class)->finalize(object);
}

static gboolean
start(GstBaseSink* sink)
{
    VslSink* vslsink = VSL_SINK(sink);

    GstClock* clock = gst_element_get_clock(GST_ELEMENT(sink));
    if (clock) { vslsink->last_frame = gst_clock_get_time(clock); }

    if (!vslsink->path) {
        gchar* name;
        g_object_get(vslsink, "name", &name, NULL);
        vslsink->path =
            g_strdup_printf("/tmp/%s.%ld", name, syscall(SYS_gettid));
        g_free(name);
    }

    GST_INFO_OBJECT(vslsink, "creating vsl host on %s", vslsink->path);

    vslsink->host = vsl_host_init(vslsink->path);
    if (!vslsink->host) {
        GST_ERROR_OBJECT(vslsink,
                         "failed to initialize vsl host: %s",
                         strerror(errno));
        return FALSE;
    }

    gst_task_start(vslsink->task);

    return TRUE;
}

static gboolean
stop(GstBaseSink* sink)
{
    VslSink* vslsink = VSL_SINK(sink);
    gst_task_stop(vslsink->task);
    return TRUE;
}

static void
frame_cleanup(VSLFrame* frame)
{
    GstMemory* memory = vsl_frame_userptr(frame);
    GST_TRACE("%p serial:%ld timestamp:%ld expires:%ld now:%ld",
              frame,
              vsl_frame_serial(frame),
              vsl_frame_timestamp(frame),
              vsl_frame_expires(frame),
              vsl_timestamp());
    gst_memory_unref(memory);
}

static GstFlowReturn
show_frame(GstVideoSink* sink, GstBuffer* buffer)
{
    VslSink*       vslsink = VSL_SINK(sink);
    int            width, height;
    gchar*         framerate;
    const char*    format;
    uint32_t       fourcc;
    GstMemory*     memory;
    GstCaps*       caps;
    GstStructure*  structure;
    GstVideoFormat videoformat;

    vslsink->frame_number++;
    GST_TRACE_OBJECT(vslsink, "frame_number:%ld", vslsink->frame_number);

    memory = gst_buffer_get_all_memory(buffer);
    if (!gst_is_dmabuf_memory(memory)) {
        GST_WARNING_OBJECT(vslsink, "vslsink requires dmabuf enabled memory");
        return GST_FLOW_NOT_SUPPORTED;
    }

    int    fd = gst_dmabuf_memory_get_fd(memory);
    size_t offset, size;
    gst_memory_get_sizes(memory, &offset, &size);

    caps      = gst_pad_get_current_caps(GST_VIDEO_SINK_PAD(sink));
    structure = gst_caps_get_structure(caps, 0);

    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    framerate =
        gst_value_serialize(gst_structure_get_value(structure, "framerate"));

    format      = gst_structure_get_string(structure, "format");
    videoformat = gst_video_format_from_string(format);
    fourcc      = gst_video_format_to_fourcc(videoformat);

    if (!fourcc) {
        switch (videoformat) {
        case GST_VIDEO_FORMAT_RGB:
            fourcc = GST_MAKE_FOURCC('R', 'G', 'B', '3');
            break;
        case GST_VIDEO_FORMAT_BGR:
            fourcc = GST_MAKE_FOURCC('B', 'G', 'R', '3');
            break;
        case GST_VIDEO_FORMAT_RGBA:
            fourcc = GST_MAKE_FOURCC('R', 'G', 'B', 'A');
            break;
        case GST_VIDEO_FORMAT_BGRA:
            fourcc = GST_MAKE_FOURCC('B', 'G', 'R', 'A');
            break;
        default:
            GST_WARNING_OBJECT(vslsink,
                               "format %s has no fourcc code - leaving empty",
                               format);
            break;
        }
    }

    GstClockTime now   = 0;
    GstClock*    clock = gst_element_get_clock(GST_ELEMENT(sink));
    if (clock) { now = gst_clock_get_time(clock); }

    GST_LOG_OBJECT(vslsink,
                   "dmabuf fd:%d size:%zu offset:%zu %dx%d framerate=%s %s "
                   "fourcc:%c%c%c%c frame:%ld " GST_STIME_FORMAT,
                   fd,
                   size,
                   offset,
                   width,
                   height,
                   framerate,
                   format,
                   fourcc,
                   fourcc >> 8,
                   fourcc >> 16,
                   fourcc >> 24,
                   vslsink->frame_number,
                   GST_STIME_ARGS(GST_CLOCK_DIFF(vslsink->last_frame, now)));
    vslsink->last_frame = now;

    g_free(framerate);

    while (1) {
        if (vsl_host_process(vslsink->host) == 0) {
            break;
        } else {
            if (errno != ETIMEDOUT) {
                GST_ERROR_OBJECT(vslsink,
                                 "vsl host processing error: %s",
                                 strerror(errno));
                gst_memory_unref(memory);
                return GST_FLOW_ERROR;
            }
        }
    }

    int64_t duration = GST_BUFFER_DURATION(buffer);
    int64_t pts      = GST_BUFFER_PTS(buffer);
    int64_t dts      = GST_BUFFER_DTS(buffer);
    int64_t serial   = GST_BUFFER_OFFSET(buffer);

    GST_LOG_OBJECT(vslsink,
                   "FRAME:%ld PTS:%ld DTS:%ld DURATION:%lu\n",
                   serial,
                   pts,
                   dts,
                   duration);

    gst_task_pause(vslsink->task);
    // g_rec_mutex_lock(&vslsink->mutex);
    VSLFrame* frame = vsl_frame_register(vslsink->host,
                                         serial,
                                         fd,
                                         width,
                                         height,
                                         fourcc,
                                         size,
                                         offset,
                                         vsl_timestamp() + vslsink->lifespan *
                                                               NANOS_PER_MILLI,
                                         duration,
                                         pts,
                                         dts,
                                         frame_cleanup,
                                         memory);
    // g_rec_mutex_unlock(&vslsink->mutex);
    gst_task_start(vslsink->task);

    if (!frame) {
        gst_memory_unref(memory);
        GST_ERROR_OBJECT(vslsink,
                         "vsl frame register error: %s",
                         strerror(errno));
        return GST_FLOW_ERROR;
    }

    GST_TRACE_OBJECT(vslsink, "frame %ld broadcast", vsl_frame_serial(frame));

    return GST_FLOW_OK;
}

static void
vsl_sink_class_init(VslSinkClass* klass)
{
    GObjectClass*      gobject_class    = G_OBJECT_CLASS(klass);
    GstBaseSinkClass*  base_sink_class  = GST_BASE_SINK_CLASS(klass);
    GstVideoSinkClass* video_sink_class = GST_VIDEO_SINK_CLASS(klass);

    gst_element_class_add_pad_template(
        GST_ELEMENT_CLASS(klass),
        gst_pad_template_new("sink",
                             GST_PAD_SINK,
                             GST_PAD_ALWAYS,
                             gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "VideoStream Sink",
                                          "Sink/Video",
                                          "DMA-enabled cross-process GStreamer "
                                          "pipeline",
                                          "Au-Zone Technologies "
                                          "<info@au-zone.com>");

    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;
    gobject_class->dispose      = dispose;
    gobject_class->finalize     = finalize;

    base_sink_class->start = start;
    base_sink_class->stop  = stop;

    video_sink_class->show_frame = show_frame;

    properties[PROP_PATH] =
        g_param_spec_string("path",
                            "Path",
                            "Path to the VideoStream socket",
                            NULL,
                            G_PARAM_READWRITE);

    properties[PROP_LIFESPAN] =
        g_param_spec_int64("lifespan",
                           "lifespan",
                           "The lifespan of unlocked frames in milliseconds",
                           0,
                           INT64_MAX,
                           DEFAULT_LIFESPAN,
                           G_PARAM_READWRITE);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, properties);
}
