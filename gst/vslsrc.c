// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <gst/allocators/allocators.h>
#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "videostream.h"
#include "vslsrc.h"

static GstStateChangeReturn (*ptr_to_default_change_state)(
    GstElement*    element,
    GstStateChange transition);

GST_DEBUG_CATEGORY_STATIC(vsl_src_debug_category);
#define GST_CAT_DEFAULT vsl_src_debug_category

#define vsl_src_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE(
    VslSrc,
    vsl_src,
    GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT(vsl_src_debug_category,
                            "vslsrc",
                            0,
                            "debug category for vslsrc element"));

typedef enum {
    PROP_0,
    PROP_PATH,
    PROP_TIMEOUT,
    PROP_DTS,
    PROP_PTS,
    PROP_RECONNECT,
    N_PROPERTIES
} VslSrcProperty;

static GParamSpec* properties[N_PROPERTIES] = {
    NULL,
};

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS_ANY);

static void
vsl_src_init(VslSrc* vslsrc)
{
    GstBaseSrc* src = GST_BASE_SRC(vslsrc);
    gst_base_src_set_live(src, TRUE);
    gst_base_src_set_format(src, GST_FORMAT_TIME);
    gst_base_src_set_do_timestamp(src, TRUE);
    g_rec_mutex_init(&vslsrc->mutex);
}

static void
set_property(GObject*      object,
             guint         property_id,
             const GValue* value,
             GParamSpec*   pspec)
{
    VslSrc* vslsrc = VSL_SRC(object);

    switch ((VslSrcProperty) property_id) {
    case PROP_PATH:
        if (vslsrc->path) {
            GST_WARNING_OBJECT(vslsrc,
                               "cannot adjust path once set (currently: %s)",
                               vslsrc->path);
            break;
        }

        vslsrc->path = g_value_dup_string(value);
        break;
    case PROP_TIMEOUT:
        vslsrc->socket_timeout_secs = g_value_get_float(value);
        break;
    case PROP_DTS:
        vslsrc->dts = g_value_get_boolean(value);
        break;
    case PROP_PTS:
        vslsrc->pts = g_value_get_boolean(value);
        break;
    case PROP_RECONNECT:
        vslsrc->reconnect = g_value_get_boolean(value);
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

    VslSrc* vslsrc = VSL_SRC(object);

    switch ((VslSrcProperty) property_id) {
    case PROP_PATH:
        g_value_set_string(value, vslsrc->path);
        break;
    case PROP_TIMEOUT:
        g_value_set_float(value, vslsrc->socket_timeout_secs);
        break;
    case PROP_DTS:
        g_value_set_boolean(value, vslsrc->dts);
        break;
    case PROP_PTS:
        g_value_set_boolean(value, vslsrc->pts);
        break;
    case PROP_RECONNECT:
        g_value_set_boolean(value, vslsrc->reconnect);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
dispose(GObject* object)
{
    VslSrc* vslsrc = VSL_SRC(object);
    GST_LOG_OBJECT(vslsrc, "dispose");

    if (vslsrc->client) {
        g_rec_mutex_lock(&vslsrc->mutex);
        vsl_client_release(vslsrc->client);
        vslsrc->client = NULL;
        g_rec_mutex_unlock(&vslsrc->mutex);
    }

    G_OBJECT_CLASS(vsl_src_parent_class)->dispose(object);
}

static void
finalize(GObject* object)
{
    VslSrc* vslsrc = VSL_SRC(object);
    GST_LOG_OBJECT(vslsrc, "finalize");
    G_OBJECT_CLASS(vsl_src_parent_class)->finalize(object);
}

static gboolean
start(GstBaseSrc* src)
{
    VslSrc* vslsrc = VSL_SRC(src);

    if (!vslsrc->path) {
        gchar* name;
        g_object_get(vslsrc, "name", &name, NULL);
        vslsrc->path =
            g_strdup_printf("/tmp/%s.%ld", name, syscall(SYS_gettid));
        g_free(name);
    }

    GST_INFO_OBJECT(vslsrc, "creating vsl client to %s", vslsrc->path);

    vslsrc->client = vsl_client_init(vslsrc->path, NULL, vslsrc->reconnect);

    if (!vslsrc->client) {
        GST_ERROR_OBJECT(vslsrc,
                         "failed to initialize vsl client: %s",
                         strerror(errno));
        return FALSE;
    }

    if (vslsrc->socket_timeout_secs > 0.0F) {
        vsl_client_set_timeout(vslsrc->client, vslsrc->socket_timeout_secs);
    }

    GstClock* clock = gst_element_get_clock(GST_ELEMENT(src));
    if (clock) { vslsrc->last_time = gst_clock_get_time(clock); }

    if (gst_base_src_is_async(GST_BASE_SRC(vslsrc))) {
        gst_base_src_start_complete(src, GST_FLOW_OK);
    }

    return TRUE;
}

static gboolean
stop(GstBaseSrc* src)
{
    VslSrc* vslsrc    = VSL_SRC(src);
    vslsrc->reconnect = FALSE;

    if (vslsrc->client) {
        g_rec_mutex_lock(&vslsrc->mutex);
        vsl_client_release(vslsrc->client);
        vslsrc->client = NULL;
        g_rec_mutex_unlock(&vslsrc->mutex);
    }

    return TRUE;
}

/**
 * @brief extends gst_video_format_from_fourcc with RGB color formats
 * gst_video_format_from_fourcc only supports YUV formats
 * @param fourcc 
 * @return GstVideoFormat 
 */
static GstVideoFormat
gst_video_format_from_fourcc_extended (guint32 fourcc)
{
  switch (fourcc) {
    case GST_MAKE_FOURCC ('R', 'G', 'B', 'x'):
      return GST_VIDEO_FORMAT_RGBx;
    case GST_MAKE_FOURCC ('B', 'G', 'R', 'x'):
      return GST_VIDEO_FORMAT_BGRx;
    case GST_MAKE_FOURCC ('R', 'G', 'B', 'A'):
      return GST_VIDEO_FORMAT_RGBA;
    case GST_MAKE_FOURCC ('B', 'G', 'R', 'A'):
      return GST_VIDEO_FORMAT_BGRA;
    // Fix gstreamer and v4l2 fourcc mismatch
    case GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'):
      return GST_VIDEO_FORMAT_YUY2;
    default:
      return gst_video_format_from_fourcc (fourcc);
  }
}

static GstCaps*
get_caps(GstBaseSrc* src, GstCaps* filter)
{
    (void) filter;
    
    VslSrc* vslsrc = VSL_SRC(src);

    if (!vslsrc->client) {
        GST_WARNING_OBJECT(vslsrc,
                           "cannot send caps - videostream not connected");
        return gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    }

    g_rec_mutex_lock(&vslsrc->mutex);
    VSLFrame* frame = vsl_frame_wait(vslsrc->client, 0);
    g_rec_mutex_unlock(&vslsrc->mutex);

    if (!frame) {
        GST_ERROR_OBJECT(vslsrc,
                         "failed to acquire a frame: %s",
                         strerror(errno));
        return gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    }

    vslsrc->width         = vsl_frame_width(frame);
    vslsrc->height        = vsl_frame_height(frame);
    vslsrc->fourcc        = vsl_frame_fourcc(frame);
    GstVideoFormat format = gst_video_format_from_fourcc_extended(vslsrc->fourcc);

    GST_INFO_OBJECT(vslsrc,
                    "videostream frame %dx%d format: %c%c%c%c, gst enum: %d",
                    vslsrc->width,
                    vslsrc->height,
                    vslsrc->fourcc,
                    vslsrc->fourcc >> 8,
                    vslsrc->fourcc >> 16,
                    vslsrc->fourcc >> 24,
                    format);

    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
        
        GST_ERROR_OBJECT(vslsrc, "unknown video format");
        return gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    }

    GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                        "format",
                                        G_TYPE_STRING,
                                        gst_video_format_to_string(format),
                                        "width",
                                        G_TYPE_INT,
                                        vslsrc->width,
                                        "height",
                                        G_TYPE_INT,
                                        vslsrc->height,
                                        // "framerate",
                                        // GST_TYPE_FRACTION,
                                        // vslsrc->framerate_num,
                                        // vslsrc->framerate_den,
                                        NULL);
    GST_DEBUG_OBJECT(vslsrc, "caps:\n%" GST_PTR_FORMAT, caps);

    vsl_frame_release(frame);

    return caps;
}

void
release_frame(void* data)
{
    VSLFrame* frame  = data;
    VslSrc*   vslsrc = vsl_frame_userptr(frame);

    if (!frame) { return; }
    if (!vslsrc->client) {
        g_rec_mutex_lock(&vslsrc->mutex);
        close(vsl_frame_handle(frame));
        vsl_frame_munmap(frame);
        g_rec_mutex_unlock(&vslsrc->mutex);
        return;
    }

    GST_TRACE_OBJECT(vslsrc,
                     "release frame serial: %ld timestamp: %ld",
                     vsl_frame_serial(frame),
                     vsl_timestamp());

    g_rec_mutex_lock(&vslsrc->mutex);
    int err = vsl_frame_unlock(frame);
    g_rec_mutex_unlock(&vslsrc->mutex);

    if (err) {
        GST_ERROR_OBJECT(vslsrc,
                         "failed to unlock frame %ld: %s",
                         vsl_frame_serial(frame),
                         strerror(errno));
    }

    g_rec_mutex_lock(&vslsrc->mutex);
    vsl_frame_release(frame);
    g_rec_mutex_unlock(&vslsrc->mutex);
}

static GstFlowReturn
create(GstPushSrc* src, GstBuffer** buf)
{
    VslSrc* vslsrc = VSL_SRC(src);

    if (!vslsrc->client) {
        GST_ERROR_OBJECT(vslsrc, "client is disconnected!");
        return GST_FLOW_ERROR;
    }

    GQuark vsl_quark = g_quark_from_string("VSLFrame");

    GstClock*    clock = gst_element_get_clock(GST_ELEMENT(src));
    GstClockTime now   = clock ? gst_clock_get_time(clock) : 0;

    GST_TRACE_OBJECT(vslsrc,
                     "waiting for frame %ld - last frame %" GST_STIME_FORMAT,
                     vslsrc->last_serial + 1,
                     GST_STIME_ARGS(GST_CLOCK_DIFF(vslsrc->last_time, now)));

    g_rec_mutex_lock(&vslsrc->mutex);
    VSLFrame* frame = vsl_frame_wait(vslsrc->client, 0);
    g_rec_mutex_unlock(&vslsrc->mutex);

    if (!frame) {
        GST_ERROR_OBJECT(vslsrc,
                         "failed to acquire a frame: %s",
                         strerror(errno));
        return GST_FLOW_ERROR;
    }

    GstClockTime now2 = clock ? gst_clock_get_time(clock) : 0;
    GST_TRACE_OBJECT(vslsrc,
                     "got frame %ld - waited %" GST_STIME_FORMAT,
                     vsl_frame_serial(frame),
                     GST_STIME_ARGS(GST_CLOCK_DIFF(now, now2)));

    if (vslsrc->last_serial) {
        int64_t diff = vsl_frame_serial(frame) - vslsrc->last_serial;

        if (diff < 0) {
            GST_WARNING_OBJECT(vslsrc, "received %ld stale frames", diff);
        } else if (diff > 1) {
            GST_WARNING_OBJECT(vslsrc, "missed %ld frames", diff);
        }
    }

    vslsrc->last_serial = vsl_frame_serial(frame);
    vsl_frame_set_userptr(frame, vslsrc);

    int      width  = vsl_frame_width(frame);
    int      height = vsl_frame_height(frame);
    uint32_t fourcc = vsl_frame_fourcc(frame);

    GST_LOG_OBJECT(vslsrc,
                   "videostream frame %dx%d format:%c%c%c%c",
                   width,
                   height,
                   fourcc,
                   fourcc >> 8,
                   fourcc >> 16,
                   fourcc >> 24);

    if (width != vslsrc->width || height != vslsrc->height ||
        fourcc != vslsrc->fourcc) {
        GST_ERROR_OBJECT(vslsrc, "videostream format change unsupported");
        vsl_frame_release(frame);
        return GST_FLOW_NOT_SUPPORTED;
    }

    now = clock ? gst_clock_get_time(clock) : 0;

    g_rec_mutex_lock(&vslsrc->mutex);
    int err = vsl_frame_trylock(frame);
    g_rec_mutex_unlock(&vslsrc->mutex);

    if (err) {
        GST_ERROR_OBJECT(vslsrc,
                         "failed to lock frame %ld: %s",
                         vsl_frame_serial(frame),
                         strerror(errno));
        vsl_frame_release(frame);
        return GST_FLOW_ERROR;
    }

    now2 = clock ? gst_clock_get_time(clock) : 0;
    GST_TRACE_OBJECT(vslsrc,
                     "locked frame %ld - delay %" GST_STIME_FORMAT,
                     vsl_frame_serial(frame),
                     GST_STIME_ARGS(GST_CLOCK_DIFF(now, now2)));

    size_t size   = vsl_frame_size(frame);
    int    dmabuf = vsl_frame_handle(frame);
    if (dmabuf == -1) {
        GST_ERROR_OBJECT(vslsrc,
                         "frame missing required dmabuf descriptor: %s",
                         strerror(errno));
        vsl_frame_release(frame);
        return GST_FLOW_NOT_SUPPORTED;
    }

    vslsrc->last_time = clock ? gst_clock_get_time(clock) : 0;
    GstBuffer* buffer = gst_buffer_new();

    if (vslsrc->pts) {
        GST_BUFFER_PTS(buffer) = vsl_frame_pts(frame);
    } else {
        GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    }

    if (vslsrc->dts) {
        GST_BUFFER_DTS(buffer) = vsl_frame_dts(frame);
    } else {
        GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
    }

    GST_BUFFER_DURATION(buffer)   = vsl_frame_duration(frame);
    GST_BUFFER_OFFSET(buffer)     = vsl_frame_serial(frame);
    GST_BUFFER_OFFSET_END(buffer) = vsl_frame_serial(frame) + 1;

    GstAllocator* allocator = gst_dmabuf_allocator_new();
    GstMemory*    memory = gst_dmabuf_allocator_alloc(allocator, dmabuf, size);
    gst_buffer_append_memory(buffer, memory);
    gst_object_unref(allocator);

    gst_mini_object_set_qdata(GST_MINI_OBJECT(memory),
                              vsl_quark,
                              frame,
                              release_frame);

    *buf = buffer;

    return GST_FLOW_OK;
}

static GstStateChangeReturn
change_state(GstElement* element, GstStateChange transition)
{

    VslSrc* vslsrc = VSL_SRC(element);

    switch (transition) {

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        vsl_client_disconnect(vslsrc->client);
        break;

    default:
        break;
    }

    return ptr_to_default_change_state(element, transition);
}

static void
vsl_src_class_init(VslSrcClass* klass)
{
    GObjectClass*    gobject_class  = G_OBJECT_CLASS(klass);
    GstBaseSrcClass* base_src_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass* push_src_class = GST_PUSH_SRC_CLASS(klass);

    GstElementClass* element = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "VideoStream Source",
                                          "Source/Video",
                                          "DMA-enabled cross-process "
                                          "GStreamer "
                                          "pipeline",
                                          "Au-Zone Technologies "
                                          "<info@au-zone.com>");

    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                              &src_template);

    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;
    gobject_class->dispose      = dispose;
    gobject_class->finalize     = finalize;

    base_src_class->start    = start;
    base_src_class->stop     = stop;
    base_src_class->get_caps = get_caps;

    push_src_class->create = create;

    ptr_to_default_change_state = element->change_state;

    element->change_state = change_state;

    properties[PROP_PATH] =
        g_param_spec_string("path",
                            "Path",
                            "Path to the VideoStream socket",
                            NULL,
                            G_PARAM_READWRITE);

    properties[PROP_TIMEOUT] =
        g_param_spec_float("timeout",
                           "Socket Timeout",
                           "Client socket timeout value (secs)",
                           0.0F,
                           // Ballparked max value of one day
                           86400.0F,
                           1.0F,
                           G_PARAM_READWRITE);

    properties[PROP_DTS] = g_param_spec_boolean("dts",
                                                "Decoding Timestamps",
                                                "Apply decoding timestamps "
                                                "from frame to GstBuffer",
                                                TRUE,
                                                G_PARAM_READWRITE);

    properties[PROP_PTS] = g_param_spec_boolean("pts",
                                                "Presentation Timestamps",
                                                "Apply presentation timestamps "
                                                "from frame to GstBuffer",
                                                TRUE,
                                                G_PARAM_READWRITE);

    properties[PROP_RECONNECT] =
        g_param_spec_boolean("reconnect",
                             "Reconnect to Host",
                             "Automatically reconnect to the host if "
                             "connection is lost.",
                             TRUE,
                             G_PARAM_READWRITE);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, properties);
}