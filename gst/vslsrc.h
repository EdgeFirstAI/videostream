// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSLSRC_H
#define VSLSRC_H

#include <stdint.h>

#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_VSL_SRC (vsl_src_get_type())
#define VSL_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_VSL_SRC, VslSrc))
#define VSL_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_VSL_SRC, VslSrcClass))
#define IS_VSL_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_VSL_SRC))
#define IS_VSL_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_VSL_SRC))
#define VSL_SRC_CAST(obj) ((VslSrc*) obj)

typedef struct vsl_client VSLClient;

typedef struct {
    GstPushSrc   parent;
    GRecMutex    mutex;
    VSLClient*   client;
    gchar*       path;
    int          width;
    int          height;
    int          framerate_num;
    int          framerate_den;
    gboolean     dts;
    gboolean     pts;
    uint32_t     fourcc;
    GstClockTime last_time;
    int64_t      last_serial;
    float        socket_timeout_secs;
    gboolean     reconnect;
} VslSrc;

typedef struct {
    GstPushSrcClass parent_class;
} VslSrcClass;

GType
vsl_src_get_type(void);

#ifdef __cplusplus
}
#endif

#endif /* VSLSRC_H */
