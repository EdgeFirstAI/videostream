// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#endif

#define DEFAULT_FRAMERATE_NUM 30
#define DEFAULT_FRAMERATE_DEN 1

int
socket_blocking(SOCKET sock, int blocking)
{
    int err;

#ifdef _WIN32
    unsigned long noblock = blocking ? 0 : 1;
    err                   = ioctlsocket(sock, FIONBIO, &noblock);
    if (err) {
        fprintf(stderr,
                "failed to set socket non-blocking: %s\n",
                strerror(errno));
        return -1;
    }
#elif defined(O_NONBLOCK)
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "failed to get socket flags: %s\n", strerror(errno));
        return -1;
    }

    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    err = fcntl(sock, F_SETFL, flags);
    if (err) {
        fprintf(stderr,
                "failed to set socket non-blocking: %s\n",
                strerror(errno));
        return -1;
    }
#else
    int flags = blocking ? 0 : 1;
    err       = ioctl(sock, FIONBIO, &flags);
    if (err) {
        fprintf(stderr,
                "failed to set socket non-blocking: %s\n",
                strerror(errno));
        return -1;
    }
#endif

    return 0;
}

int
socket_timeout(SOCKET sock, int recvtime, int sendtime)
{
    int err;

#ifdef _WIN32
    DWORD rt = recvtime;
    DWORD st = sendtime;
#else
    struct timeval rt, st;

    rt.tv_sec  = recvtime / 1000;
    rt.tv_usec = (recvtime - (rt.tv_sec * 1000)) * 1000;

    st.tv_sec  = sendtime / 1000;
    st.tv_usec = (sendtime - (st.tv_sec * 1000)) * 1000;
#endif

    err = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof(rt));
    if (err) {
        fprintf(stderr,
                "failed to set socket recv timeout: %s\r\n",
                strerror(errno));
        return -1;
    }

    err = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &st, sizeof(st));
    if (err) {
        fprintf(stderr,
                "failed to set socket send timeout: %s\r\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

int
socket_signals(SOCKET sock, int signals)
{
#ifndef __APPLE__
    (void) sock;
    (void) signals;
#else
    int err;

    err = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &signals, sizeof(int));
    if (err) {
        fprintf(stderr,
                "failed to %s socket signals: %s\r\n",
                signals ? "enable" : "disable",
                strerror(errno));
        return -1;
    }
#endif
    return 0;
}

int
get_numerator_framerate(char* framerate)
{

    char* ptr;
    ptr = strchr(framerate, '/');

    if (ptr == NULL) {
        // some error occured, return default value
        return DEFAULT_FRAMERATE_NUM;
    }

    char* pEnd;
    char  bkp = *ptr;

    // end the string so strtol works
    *ptr = 0;

    int value = strtol(framerate, &pEnd, 10);

    // putting the char back to the original value
    *ptr = bkp;

    if (pEnd == framerate) {
        // some error occured, return default value
        return DEFAULT_FRAMERATE_NUM;
    }

    return value;
}

int
get_denominator_framerate(char* framerate)
{

    char* ptr;
    ptr = strchr(framerate, '/');

    if (ptr == NULL) {
        // some error occured, return default value
        return DEFAULT_FRAMERATE_DEN;
    }

    char* pEnd;

    int value = strtol(ptr + 1, &pEnd, 10);

    if (pEnd == ptr + 1) {
        // some error occured, return default value
        return DEFAULT_FRAMERATE_DEN;
    }

    return value;
}

int
sockaddr_from_path(const char*         path,
                   struct sockaddr_un* addr,
                   socklen_t*          addrlen)
{
    if (!path || !path[0]) {
        errno = EINVAL;
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;

    size_t path_len = strlen(path);
    if (path_len > sizeof(addr->sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(addr->sun_path, path, path_len);

    if (addrlen) {
        *addrlen = (socklen_t) (path_len + sizeof(addr->sun_family));
    }

    return 0;
}