// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <errno.h>
#include <getopt.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <gst/allocators/allocators.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "videostream.h"

static pthread_mutex_t vsl_mutex = PTHREAD_MUTEX_INITIALIZER;

void
vsl_release_memory(void* data)
{
    VSLFrame* frame = data;

    if (pthread_mutex_lock(&vsl_mutex)) {
        fprintf(stderr,
                "%s failed to acquire videostream mutex: %s\n",
                __FUNCTION__,
                strerror(errno));
    }

    close(vsl_frame_handle(frame));
    if (vsl_frame_unlock(frame)) {
        fprintf(stderr,
                "%s failed to unlock frame %ld: %s\n",
                __FUNCTION__,
                vsl_frame_serial(frame),
                strerror(errno));
    }

    printf("%s release frame serial: %ld timestamp: %ld\n",
           __FUNCTION__,
           vsl_frame_serial(frame),
           vsl_timestamp());

    vsl_frame_release(frame);
    pthread_mutex_unlock(&vsl_mutex);
}

void*
vsl_task(void* data)
{
    VSLClient*  client     = data;
    GstElement* appsrc     = vsl_client_userptr(client);
    GQuark      vsl_quark  = g_quark_from_string("VSLFrame");
    int64_t     last_frame = 0;

    while (1) {
        usleep(1000);

        if (pthread_mutex_lock(&vsl_mutex)) {
            fprintf(stderr,
                    "%s failed to acquire videostream mutex: %s\n",
                    __FUNCTION__,
                    strerror(errno));
        }

        VSLFrame* frame = vsl_frame_wait(client, 0);
        if (!frame) {
            fprintf(stderr, "failed to acquire a frame: %s\n", strerror(errno));
            pthread_mutex_unlock(&vsl_mutex);
            return NULL;
        }

        int      width  = vsl_frame_width(frame);
        int      height = vsl_frame_height(frame);
        uint32_t fourcc = vsl_frame_fourcc(frame);

        printf("acquired video frame %dx%d format:%c%c%c%c frame_time: %ld\n",
               width,
               height,
               fourcc,
               fourcc >> 8,
               fourcc >> 16,
               fourcc >> 24,
               vsl_frame_timestamp(frame) - last_frame);

        if (vsl_frame_trylock(frame)) {
            fprintf(stderr,
                    "failed to lock frame %ld: %s\n",
                    vsl_frame_serial(frame),
                    strerror(errno));
            vsl_frame_release(frame);
            pthread_mutex_unlock(&vsl_mutex);
            return NULL;
        }

        printf("locked frame %ld\n", vsl_frame_serial(frame));
        pthread_mutex_unlock(&vsl_mutex);

        GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                            "format",
                                            G_TYPE_STRING,
                                            "YUY2",
                                            "width",
                                            G_TYPE_INT,
                                            width,
                                            "height",
                                            G_TYPE_INT,
                                            height,
                                            "framerate",
                                            GST_TYPE_FRACTION,
                                            30,
                                            1,
                                            NULL);

        size_t size   = vsl_frame_size(frame);
        int    dmabuf = vsl_frame_handle(frame);
        if (dmabuf == -1) {
            fprintf(stderr,
                    "%s failed to retrieve dmabuf descriptor: %s\n",
                    __FUNCTION__,
                    strerror(errno));
            vsl_frame_release(frame);
            continue;
        }

        GstBuffer* buffer      = gst_buffer_new();
        GST_BUFFER_PTS(buffer) = vsl_frame_timestamp(frame) * 1000 * 1000;
        GST_BUFFER_DURATION(buffer) =
            gst_util_uint64_scale_int(1, GST_SECOND, 30);
        GstAllocator* allocator = gst_dmabuf_allocator_new();
        GstMemory* memory = gst_dmabuf_allocator_alloc(allocator, dmabuf, size);
        gst_buffer_append_memory(buffer, memory);
        gst_object_unref(allocator);

        gst_mini_object_set_qdata(GST_MINI_OBJECT(buffer),
                                  vsl_quark,
                                  frame,
                                  vsl_release_memory);

        GstSample* sample = gst_sample_new(buffer, caps, NULL, NULL);
        gst_buffer_unref(buffer);

        GstFlowReturn ret =
            gst_app_src_push_sample(GST_APP_SRC(appsrc), sample);
        gst_sample_unref(sample);

        if (ret != GST_FLOW_OK) {
            fprintf(stderr, "%s push-buffer error: %d\n", __FUNCTION__, ret);
        }

        last_frame = vsl_frame_timestamp(frame);
    }
}

int
main(int argc, char** argv)
{
    gst_init(&argc, &argv);

    GstElement* source = gst_element_factory_make("appsrc", "source");
    if (!source) {
        fprintf(stderr, "failed to create appsrc element\n");
        return EXIT_FAILURE;
    }

    GstElement* fps = gst_element_factory_make("fpsdisplaysink", "fps");
    if (!fps) {
        fprintf(stderr, "failed to create fps element\n");
        return EXIT_FAILURE;
    }

    GstElement* sink = gst_element_factory_make("fakesink", "sink");
    if (!sink) {
        fprintf(stderr, "failed to create sink element\n");
        return EXIT_FAILURE;
    }

    g_object_set(G_OBJECT(fps), "video-sink", sink, NULL);

    GstElement* pipeline = gst_pipeline_new("display");
    if (!pipeline) {
        fprintf(stderr, "failed to create gstreamer pipeline\n");
        return EXIT_FAILURE;
    }

    VSLClient* client = vsl_client_init("/tmp/camhost.0", source, false);
    if (!client) {
        fprintf(stderr,
                "failed to connect to videostream host /tmp/camhost.0: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    printf("connected to /tmp/camhost.0\n");

    gst_bin_add_many(GST_BIN(pipeline), source, fps, sink, NULL);
    if (!gst_element_link_many(source, fps, NULL)) {
        fprintf(stderr, "failed to link gstreamer pipeline\n");
        return EXIT_FAILURE;
    }

    GThread* vsl_thread = g_thread_new("vsl_task", vsl_task, client);

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    if (!loop) {
        fprintf(stderr, "failed to create gstreamer loop\n");
        return EXIT_FAILURE;
    }

    printf("starting vsl display...\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);
    printf("...completed.\n");

    g_thread_join(vsl_thread);

    return EXIT_SUCCESS;
}
