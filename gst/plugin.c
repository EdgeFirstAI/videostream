// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <gst/gst.h>
#include <stdio.h>

#include "videostream.h"
#include "vslsink.h"
#include "vslsrc.h"

#define PACKAGE "videostream"

static gboolean
plugin_init(GstPlugin* plugin)
{
    return gst_element_register(plugin,
                                "vslsink",
                                GST_RANK_PRIMARY,
                                TYPE_VSL_SINK) &&
           gst_element_register(plugin,
                                "vslsrc",
                                GST_RANK_PRIMARY,
                                TYPE_VSL_SRC);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  videostream,
                  "VideoStream GStreamer Plugin",
                  plugin_init,
                  VSL_VERSION,
                  "Proprietary",
                  "videostream",
                  "embeddedml.com")
