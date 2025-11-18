// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSL_COMMON_H
#define VSL_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#ifndef _WIN32
#define SOCKET int
#endif

extern int
socket_blocking(SOCKET sock, int blocking);

extern int
socket_timeout(SOCKET sock, int recvtime, int sendtime);

extern int
socket_signals(SOCKET sock, int signals);

extern int
get_numerator_framerate(char* framerate);

extern int
get_denominator_framerate(char* framerate);

extern int
sockaddr_from_path(const char*         path,
                   struct sockaddr_un* addr,
                   socklen_t*          addrlen);

#endif /* VSL_COMMON_H */