// SPDX-License-Identifier: Apache-2.0
// Copyright © 2025 Au-Zone Technologies. All Rights Reserved.

/**
 * @file vsl-v4l2-info.c
 * @brief V4L2 device enumeration utility
 *
 * Lists all V4L2 video devices with their capabilities and supported formats.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <videostream.h>

static void
print_formats(const VSLFormat* formats, size_t count, const char* indent)
{
    for (size_t i = 0; i < count; i++) {
        char fourcc_str[5];
        vsl_v4l2_fourcc_to_string(formats[i].fourcc, fourcc_str);
        printf("%s  %s: %s%s\n",
               indent,
               fourcc_str,
               formats[i].description,
               formats[i].compressed ? " (compressed)" : "");
    }
}

static void
print_memory_caps(VSLMemoryType mem, const char* label)
{
    printf("    %s: ", label);
    int first = 1;
    if (mem & VSL_V4L2_MEM_MMAP) {
        printf("MMAP");
        first = 0;
    }
    if (mem & VSL_V4L2_MEM_USERPTR) {
        printf("%sUSERPTR", first ? "" : ", ");
        first = 0;
    }
    if (mem & VSL_V4L2_MEM_DMABUF) { printf("%sDMABUF", first ? "" : ", "); }
    if (first) { printf("none"); }
    printf("\n");
}

int
main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    printf("V4L2 Device Enumeration\n");
    printf("=======================\n\n");

    VSLDeviceList* list = vsl_v4l2_enumerate();
    if (!list) {
        perror("vsl_v4l2_enumerate");
        return 1;
    }

    if (list->count == 0) {
        printf("No V4L2 video devices found.\n");
        vsl_v4l2_device_list_free(list);
        return 0;
    }

    printf("Found %zu device(s):\n\n", list->count);

    for (size_t i = 0; i < list->count; i++) {
        VSLDevice* dev = &list->devices[i];

        printf("%s: %s\n", dev->path, dev->card);
        printf("  Driver: %s\n", dev->driver);
        printf("  Bus: %s\n", dev->bus_info);
        printf("  Type: %s%s\n",
               vsl_v4l2_device_type_name(dev->device_type),
               dev->multiplanar ? " (multiplanar)" : "");

        if (dev->num_capture_formats > 0) {
            printf("  Capture formats (%zu):\n", dev->num_capture_formats);
            print_formats(dev->capture_formats, dev->num_capture_formats, "  ");
            print_memory_caps(dev->capture_mem, "Capture memory");
        }

        if (dev->num_output_formats > 0) {
            printf("  Output formats (%zu):\n", dev->num_output_formats);
            print_formats(dev->output_formats, dev->num_output_formats, "  ");
            print_memory_caps(dev->output_mem, "Output memory");
        }

        printf("\n");
    }

    // Test find functions
    printf("Auto-detection tests:\n");
    printf("---------------------\n");

    const char* h264_encoder =
        vsl_v4l2_find_encoder(VSL_FOURCC('H', '2', '6', '4'));
    printf("H.264 encoder: %s\n", h264_encoder ? h264_encoder : "(not found)");

    const char* hevc_encoder =
        vsl_v4l2_find_encoder(VSL_FOURCC('H', 'E', 'V', 'C'));
    printf("HEVC encoder:  %s\n", hevc_encoder ? hevc_encoder : "(not found)");

    const char* h264_decoder =
        vsl_v4l2_find_decoder(VSL_FOURCC('H', '2', '6', '4'));
    printf("H.264 decoder: %s\n", h264_decoder ? h264_decoder : "(not found)");

    const char* nv12_camera =
        vsl_v4l2_find_camera(VSL_FOURCC('N', 'V', '1', '2'));
    printf("NV12 camera:   %s\n", nv12_camera ? nv12_camera : "(not found)");

    const char* yuyv_camera =
        vsl_v4l2_find_camera(VSL_FOURCC('Y', 'U', 'Y', 'V'));
    printf("YUYV camera:   %s\n", yuyv_camera ? yuyv_camera : "(not found)");

    vsl_v4l2_device_list_free(list);
    return 0;
}
