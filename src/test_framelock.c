// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.
//
// TESTING LAYER: 2 (Cross-Process IPC)
// REQUIREMENTS:
//   - A running VSL host (e.g. vsl-camhost) publishing frames
// DESCRIPTION:
//   Verifies that vsl_frame_trylock() keeps a frame's contents stable past the
//   host's frame lifespan, which is what lets a consumer hold a source frame
//   while it waits on a slow event (a detection, a disk write) instead of
//   having to consume it within the lifespan window.
//
//   Receives one frame, checksums it, optionally locks it, holds it for a
//   given number of seconds without calling vsl_frame_wait(), then checksums
//   again.  A locked frame must be byte-identical; an unlocked frame is
//   expected to be recycled by the producer once it expires.
//
// Usage:
//   vsl-test-framelock <socket_path> <hold_seconds> <lock|nolock> [timeout_s]

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <videostream.h>

// Sample the buffer on a stride coprime with any plausible row length so a
// partial overwrite cannot slip between sample points.
#define SAMPLE_STRIDE 4093

static uint64_t
checksum(const unsigned char* data, size_t size)
{
    uint64_t sum = 1469598103934665603ULL;

    for (size_t i = 0; i < size; i += SAMPLE_STRIDE) {
        sum ^= data[i];
        sum *= 1099511628211ULL;
    }

    return sum;
}

int
main(int argc, char* argv[])
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <socket_path> <hold_seconds> <lock|nolock> "
                "[timeout_s]\n",
                argv[0]);
        return 2;
    }

    const char* path      = argv[1];
    int         hold_secs = atoi(argv[2]);
    bool        do_lock   = (strcmp(argv[3], "lock") == 0);
    float       timeout_s = (argc > 4) ? (float) atof(argv[4]) : 0.0F;

    printf("path=%s hold=%ds mode=%s timeout=%s\n",
           path,
           hold_secs,
           do_lock ? "lock" : "nolock",
           (argc > 4) ? argv[4] : "default");

    VSLClient* client = vsl_client_init(path, NULL, false);
    if (!client) {
        fprintf(stderr, "connect failed: %s\n", strerror(errno));
        return 1;
    }

    if (timeout_s > 0.0F) { vsl_client_set_timeout(client, timeout_s); }

    VSLFrame* frame = vsl_frame_wait(client, 0);
    if (!frame) {
        fprintf(stderr, "no frame: %s\n", strerror(errno));
        return 1;
    }

    printf("frame serial=%ld %dx%d size=%d\n",
           vsl_frame_serial(frame),
           vsl_frame_width(frame),
           vsl_frame_height(frame),
           vsl_frame_size(frame));

    size_t size = 0;
    void*  map  = vsl_frame_mmap(frame, &size);
    if (!map) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return 1;
    }

    if (do_lock) {
        errno  = 0;
        int rc = vsl_frame_trylock(frame);
        printf("trylock rc=%d errno=%s\n", rc, rc ? strerror(errno) : "-");
        if (rc) { return 1; }
    }

    uint64_t before = checksum(map, size);
    printf("checksum before = %016lx\n", before);

    // Deliberately do NOT call vsl_frame_wait() while holding the frame; that
    // is what a consumer blocked on a slow event looks like.
    sleep(hold_secs);

    uint64_t after = checksum(map, size);
    printf("checksum after  = %016lx\n", after);
    printf("RESULT: frame contents %s after %ds\n",
           (before == after) ? "INTACT" : "OVERWRITTEN",
           hold_secs);

    if (do_lock) {
        errno  = 0;
        int rc = vsl_frame_unlock(frame);
        printf("unlock rc=%d errno=%s\n", rc, rc ? strerror(errno) : "-");
    }

    return (before == after) ? 0 : 3;
}
