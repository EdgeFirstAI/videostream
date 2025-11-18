// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../lib/common.h"
#include "videostream.h"

#define array_size(x) (sizeof(x) / sizeof(*x))

static pthread_mutex_t vsl_mutex = PTHREAD_MUTEX_INITIALIZER;

static vsl_camera* camera = NULL;
// The frame lifespan needs to be less than (buf_count-1) * (1e9/FPS)
static int64_t frame_lifespan = 90 * 1000000; // 90 ms
static int     cam_width      = 0;
static int     cam_height     = 0;
static int64_t last_frame     = 0;
static int64_t frame_count    = 0;
static bool    verbose        = false;

static void
frame_cleanup(VSLFrame* frame)
{
    const struct vsl_camera_buffer* buf = vsl_frame_userptr(frame);

    if (verbose) {
        printf("Frame cleanup on: dmafd %i\n", vsl_camera_buffer_dma_fd(buf));
    }

    vsl_camera_release_buffer(camera, buf);
}

static int
new_sample_v4l2(struct vsl_camera_buffer* buf, VSLHost* host, FILE* log)
{
    if (!vsl_camera_buffer_dma_fd(buf) && !vsl_camera_buffer_phys_addr(buf)) {
        fprintf(stderr, "recieved unsupported non-DMA buffer\n");
        return -1;
    }

    if (verbose) {
        printf("dma buffer fd: %d size: %u offset: %li width: %d height: %d "
               "fourcc: %c%c%c%c last_frame: %ld\n",
               vsl_camera_buffer_dma_fd(buf),
               vsl_camera_buffer_length(buf),
               0L,
               cam_width,
               cam_height,
               vsl_camera_buffer_fourcc(buf),
               vsl_camera_buffer_fourcc(buf) >> 8,
               vsl_camera_buffer_fourcc(buf) >> 16,
               vsl_camera_buffer_fourcc(buf) >> 24,
               vsl_timestamp() - last_frame);
    }

    last_frame = vsl_timestamp();

    if (pthread_mutex_lock(&vsl_mutex)) {
        fprintf(stderr,
                "failed to acquire videostream mutex: %s\n",
                strerror(errno));
        return 1;
    }

    if (vsl_host_process(host)) {
        fprintf(stderr, "failed to process host events: %s\n", strerror(errno));
        pthread_mutex_unlock(&vsl_mutex);
        return -1;
    }

    VSLFrame* frame = vsl_frame_init(cam_width,
                                     cam_height,
                                     0,
                                     vsl_camera_buffer_fourcc(buf),
                                     buf,
                                     frame_cleanup);
    vsl_frame_attach(frame, vsl_camera_buffer_dma_fd(buf), 0, 0);

    if (vsl_host_post(host,
                      frame,
                      vsl_timestamp() + frame_lifespan,
                      -1,
                      -1,
                      -1)) {
        fprintf(stderr, "failed to post frame: %s\n", strerror(errno));
        vsl_frame_release(frame);
    }

    frame_count = vsl_frame_serial(frame);

    if (log) {
        int64_t start_time = vsl_frame_timestamp(frame);
        int64_t end_time   = vsl_timestamp();
        int64_t duration   = end_time - start_time;

        // event, start_time, end_time, duration, serial, ex1, ex2, ex3,
        fprintf(log,
                "camhost, %ld, %ld, %ld, %ld, , , ,\n",
                start_time,
                end_time,
                duration,
                frame_count);
    }

    pthread_mutex_unlock(&vsl_mutex);

    return 0;
}

static volatile sig_atomic_t keep_running = 1;

static void
sig_handler(int _)
{
    (void) _;
    keep_running = 0;
}

static void*
host_process_wrapper(void* temp)
{
    VSLHost* host                    = temp;
    int      prev_buffer_count       = vsl_camera_get_queued_buf_count(camera);
    int64_t  buffer_starvation_start = 0;
    while (keep_running) {
        if (pthread_mutex_lock(&vsl_mutex)) {
            fprintf(stderr,
                    "failed to acquire videostream mutex: %s\n",
                    strerror(errno));
        }

        if (vsl_host_process(host)) {
            fprintf(stderr,
                    "failed to process host events: %s\n",
                    strerror(errno));
            pthread_mutex_unlock(&vsl_mutex);
        }
        int queued_bufs = vsl_camera_get_queued_buf_count(camera);
        if (queued_bufs < 1 && !(prev_buffer_count < 1)) {
            fprintf(stderr,
                    "WARNING: There are no queued buffers. There is buffer "
                    "starvation\n");
            buffer_starvation_start = vsl_timestamp();
        } else if (queued_bufs >= 1 && !(prev_buffer_count >= 1)) {
            fprintf(stderr,
                    "Exiting buffer starvation after %.2f ms\n",
                    (vsl_timestamp() - buffer_starvation_start) / 1e6);
        }
        prev_buffer_count = queued_bufs;
        pthread_mutex_unlock(&vsl_mutex);

        // Runs at 5000Hz
        usleep(1e6 / 5000);
    }
    return NULL;
}

const char* usage =
    "-h, --help\n"
    "    Display help information\n"
    "-v, --version\n"
    "    Display version information\n"
    "-V, --verbose\n"
    "    Print detailed information about frame captures\n"
    "-L FILE, --log FILE\n"
    "    Log frame events to the specified file\n"
    "-d DEVICE, --capture_device DEVICE\n"
    "    The capture device for streaming. (default /dev/video0)\n"
    "-M, --mirror\n"
    "    Mirrors the camera side-to-side\n"
    "-H, --mirror_v\n"
    "    Mirrors the camera up and down\n"
    "-r WxH, --camera_res WxH\n"
    "    Sets the camera resolution. Use [width]x[height] (default based on "
    "camera driver)\n"
    "-p PATH, --path PATH\n"
    "    The VSL camera stream host path (default: /tmp/camhost.0)\n"
    "-l LIFESPAN, --lifespan LIFESPAN\n"
    "    Sets the lifespan of the VSL frames in milliseconds (default 100ms)\n"
    "-b COUNT, --bufcount COUNT\n"
    "    Sets how many buffers to request from the device driver (default 6)\n"
    "-f FOURCC, --fourcc FOURCC\n"
    "    Sets the fourcc video format. (default based on camera driver)\n";

int
main(int argc, char** argv)
{
    const char*          device_name = "/dev/video0";
    bool                 mirror      = false;
    bool                 mirror_v    = false;
    int                  buf_count   = 6;
    u_int32_t            cam_fourcc  = 0;
    const char*          vsl_path    = "/tmp/camhost.0";
    FILE*                log         = NULL;
    static struct option options[] =
        {{"help", no_argument, NULL, 'h'},
         {"version", no_argument, NULL, 'v'},
         {"verbose", no_argument, NULL, 'V'},
         {"log", required_argument, NULL, 'L'},
         {"capture_device", required_argument, NULL, 'd'},
         {"mirror", no_argument, NULL, 'M'},
         {"mirror_v", no_argument, NULL, 'H'},
         {"camera_res", required_argument, NULL, 'r'},
         {"path", required_argument, NULL, 'p'},
         {"lifespan", required_argument, NULL, 'l'},
         {"bufcount", required_argument, NULL, 'b'},
         {"fourcc", required_argument, NULL, 'f'}};

    for (int i = 0; i < 100; i++) {
        int opt = getopt_long(argc, argv, "hvVL:d:MHr:p:l:b:f:", options, NULL);
        if (opt == -1) break;
        switch (opt) {
        case 'h':
            printf("%s\n%s", argv[0], usage);
            log ? fclose(log) : 0;
            return EXIT_SUCCESS;
        case 'v':
            printf("%s\n", vsl_version());
            log ? fclose(log) : 0;
            return EXIT_SUCCESS;
        case 'V':
            verbose = true;
            break;
        case 'L':
            log = fopen(optarg, "w");
            if (!log) {
                fprintf(stderr,
                        "failed to open log file: %s\n",
                        strerror(errno));
                return EXIT_FAILURE;
            }
            fprintf(log,
                    "event, start_time, end_time, duration, serial, "
                    "input_elapsed, model_elapsed, output_elapsed,\n");
            break;
        case 'd':
            device_name = optarg;
            break;
        case 'M':
            mirror = true;
            break;
        case 'H':
            mirror_v = true;
            break;
        case 'r': {
            const char* split = strchr(optarg, 'x');
            if (split) {
                cam_width  = atoi(optarg);
                cam_height = atoi(split + 1);
            } else {
                fprintf(stderr, "Resolution invalid: %s\n", optarg);
                log ? fclose(log) : 0;
                return EXIT_FAILURE;
            }
            break;
        }
        case 'p':
            vsl_path = optarg;
            break;
        case 'l':
            frame_lifespan = (int64_t) (atof(optarg) * 1e6);
            break;
        case 'b':
            buf_count = atoi(optarg);
            break;
        case 'f':
            if (strlen(optarg) != 4) {
                if (cam_fourcc == 0) {
                    printf("%s fourcc code was not 4 characters, using camera "
                           "default instead\n",
                           optarg);
                } else {
                    printf("%s fourcc code was not 4 characters, using "
                           "%c%c%c%c instead\n",
                           optarg,
                           cam_fourcc,
                           cam_fourcc >> 8,
                           cam_fourcc >> 16,
                           cam_fourcc >> 24);
                }
                break;
            }
            cam_fourcc = VSL_FOURCC(optarg[0], optarg[1], optarg[2], optarg[3]);
        }
    }

    signal(SIGHUP, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    VSLHost* host = vsl_host_init(vsl_path);
    if (!host) {
        fprintf(stderr,
                "failed to create videostream host: %s\n",
                strerror(errno));
        log ? fclose(log) : 0;
        return EXIT_FAILURE;
    }

    // The listener/accept socket is always the first and always available.
    int listener;
    vsl_host_sockets(host, 1, &listener, NULL);

    camera = vsl_camera_open_device(device_name);
    if (!camera) {
        printf("%s device could not be opened\n", device_name);
        vsl_host_release(host);
        return -1;
    }

    uint32_t requested_fourcc = cam_fourcc;

    int err = vsl_camera_init_device(camera,
                                     &cam_width,
                                     &cam_height,
                                     &buf_count,
                                     &cam_fourcc);
    if (err) {
        printf("Could not initialize device to stream\n");
        vsl_camera_close_device(camera);
        vsl_host_release(host);
        log ? fclose(log) : 0;
        return -1;
    }

    if (requested_fourcc != 0 && cam_fourcc != requested_fourcc) {
        printf("Could not initialize device to stream in %c%c%c%c\n",
               requested_fourcc,
               requested_fourcc >> 8,
               requested_fourcc >> 16,
               requested_fourcc >> 24);
        u_int32_t codes[100];
        int       fmts = vsl_camera_enum_fmts(camera, codes, array_size(codes));
        printf("Try one of the following video formats:\n");
        for (int i = 0; i < fmts; i++) {
            printf("\t%c%c%c%c\n",
                   codes[i],
                   codes[i] >> 8,
                   codes[i] >> 16,
                   codes[i] >> 24);
        }

        fmts = vsl_camera_enum_mplane_fmts(camera, codes, array_size(codes));
        for (int i = 0; i < fmts; i++) {
            printf("\tmultiplanar %c%c%c%c\n",
                   codes[i],
                   codes[i] >> 8,
                   codes[i] >> 16,
                   codes[i] >> 24);
        }
        vsl_camera_close_device(camera);
        vsl_host_release(host);
        log ? fclose(log) : 0;
        return -1;
    }

    vsl_camera_mirror(camera, mirror);
    vsl_camera_mirror_v(camera, mirror_v);

    vsl_camera_start_capturing(camera);

    pthread_t threadID = 0;
    pthread_create(&threadID, NULL, host_process_wrapper, host);

    while (keep_running) {
        struct vsl_camera_buffer* buf = vsl_camera_get_data(camera);
        if (buf) { new_sample_v4l2(buf, host, log); }
    }

    void* ret;
    pthread_join(threadID, &ret);

    vsl_host_release(host);

    vsl_camera_stop_capturing(camera);
    vsl_camera_uninit_device(camera);
    vsl_camera_close_device(camera);

    log ? fclose(log) : 0;

    return 0;
}
