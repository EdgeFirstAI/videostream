// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "common.h"
#include "frame.h"
#include "videostream.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define NSEC_PER_SEC 1000000000
#define LOCK_TIMEOUT (250 * 1000 * 1000)
#define MAX_FRAMES_PER_CLIENT 20

// Timing instrumentation
static inline int64_t
get_timestamp_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

struct socket_and_frames {
    SOCKET    one_socket;
    VSLFrame* frames[MAX_FRAMES_PER_CLIENT];
};

struct vsl_host {
    char*                     path;
    int                       n_sockets;
    struct socket_and_frames* sockets;
    int                       n_frames;
    VSLFrame**                frames;
    int64_t                   serial;
    pthread_mutex_t           lock;
};

static inline void
timespec_add_nsec(struct timespec* ts, int64_t adj)
{
    ts->tv_sec  = ts->tv_sec + (adj / NSEC_PER_SEC);
    ts->tv_nsec = ts->tv_nsec + (adj % NSEC_PER_SEC);

    if (ts->tv_nsec >= NSEC_PER_SEC) {
        ts->tv_sec++;
        ts->tv_nsec -= NSEC_PER_SEC;
    } else if (ts->tv_nsec < 0) {
        ts->tv_sec--;
        ts->tv_nsec += NSEC_PER_SEC;
    }
}

static int
add_frame_to_socket(int socket, VSLHost* host, VSLFrame* frame)
{
    int sock_idx = -1;
    for (int i = 0; i < host->n_sockets; i++) {
        if (host->sockets[i].one_socket == socket) { sock_idx = i; }
    }

    if (sock_idx == -1) {
        // Should not happen
        return -1;
    }

    for (int j = 0; j < MAX_FRAMES_PER_CLIENT; j++) {
        if (host->sockets[sock_idx].frames[j] == NULL) {
            host->sockets[sock_idx].frames[j] = frame;
            return 0;
        } else if (host->sockets[sock_idx].frames[j] == frame) {
            fprintf(stderr,
                    "%s frame %ld already locked for socket %d\n",
                    __FUNCTION__,
                    vsl_frame_serial(frame),
                    socket);
            return 0;
        }
    }
    // The socket's locked frame list was full
    return -1;
}

static int
remove_frame_from_socket(int socket, VSLHost* host, VSLFrame* frame)
{
    bool found = false;

    for (int i = 0; i < host->n_sockets; i++) {
        if (host->sockets[i].one_socket == socket) {
            for (int j = 0; j < MAX_FRAMES_PER_CLIENT; j++) {
                if (host->sockets[i].frames[j] == frame) {
                    found                      = true;
                    host->sockets[i].frames[j] = NULL;
                    break;
                }
            }
        }
    }

    if (!found) { return -1; }

    return 0;
}

static void
disconnect_client_index(VSLHost* host, int index)
{
    if (index >= host->n_sockets) {
        fprintf(stderr, "%s invalid client index %d\n", __FUNCTION__, index);
        return;
    }

    struct socket_and_frames* client = &host->sockets[index];

    for (int j = 0; j < MAX_FRAMES_PER_CLIENT; j++) {
        if (client->frames[j] != NULL) {
            client->frames[j]->info.locked--;
            client->frames[j] = NULL;
        }
    }

    shutdown(client->one_socket, SHUT_RDWR);
    close(client->one_socket);
    client->one_socket = -1;

    memset(client->frames, 0, sizeof(VSLFrame*) * MAX_FRAMES_PER_CLIENT);
}

static void
expire_frames(VSLHost* host)
{
    int64_t now = vsl_timestamp();

    for (int i = 0; i < host->n_frames; i++) {
        if (host->frames[i]) {
            VSLFrame* old = host->frames[i];
            if (old->info.locked) { continue; }
            if (old->info.expires && old->info.expires < now) {
                vsl_frame_release(old);
            }
        }
    }
}

static int
insert_frame(VSLHost* host, VSLFrame* frame)
{
    expire_frames(host);

    int frame_idx = -1;

    for (int i = 0; i < host->n_frames; i++) {
        if (host->frames[i] == NULL) {
            frame_idx = i;
            break;
        }
    }

    if (frame_idx == -1) {
        VSLFrame** new_frames =
            realloc(host->frames, 2 * host->n_frames * sizeof(VSLFrame));
        if (!new_frames) {
            errno = ENOMEM;
            return -1;
        }

        // We always double the buffer array so we must memset to 0 from the end
        // of the new buffer for the length of the old buffer.
        memset(&new_frames[host->n_frames],
               0,
               host->n_frames * sizeof(VSLFrame*));
        frame_idx    = host->n_frames;
        host->frames = new_frames;
        host->n_frames *= 2;
    }

    if (frame_idx < 0 || frame_idx >= host->n_frames) {
        fprintf(stderr,
                "%s invalid frame index: %d\n",
                __FUNCTION__,
                frame_idx);
        errno = ENOBUFS;
        return -1;
    }

    host->frames[frame_idx] = frame;
    frame->host             = host;

    return 0;
}

VSL_API
VSLHost*
vsl_host_init(const char* path)
{
    int                sock;
    struct sockaddr_un addr;
    socklen_t          addrlen = 0;

    if (!path || !path[0]) {
        errno = EINVAL;
        return NULL;
    }

    if (sockaddr_from_path(path, &addr, &addrlen)) {
        fprintf(stderr,
                "%s invalid socket path: %s\n",
                __FUNCTION__,
                strerror(errno));
        return NULL;
    }

    sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

    if (sock == -1) {
        fprintf(stderr,
                "%s failed to create socket: %s\n",
                __FUNCTION__,
                strerror(errno));
        return NULL;
    }

    if (socket_blocking(sock, 0)) {
        fprintf(stderr,
                "%s failed to set socket non-blocking: %s\n",
                __FUNCTION__,
                strerror(errno));
        close(sock);
        return NULL;
    }

    int err = bind(sock, (struct sockaddr*) &addr, addrlen);
    if (err && errno == EADDRINUSE) {
        if (-1 == connect(sock, (struct sockaddr*) &addr, addrlen) &&
            errno == ECONNREFUSED) {
            unlink(path);
            err = bind(sock, (struct sockaddr*) &addr, addrlen);
        }
    }
    if (err) {
        fprintf(stderr,
                "%s failed to bind unix socket on %s %s: %s\n",
                __FUNCTION__,
                (path[0] == '/' ? "path" : "abstract address"),
                path,
                strerror(errno));
        close(sock);
        return NULL;
    }

    if (listen(sock, SOMAXCONN)) {
        fprintf(stderr,
                "%s failed to listen on socket: %s\n",
                __FUNCTION__,
                strerror(errno));
        close(sock);
        return NULL;
    }

    VSLHost* host = calloc(1, sizeof(VSLHost));
    if (!host) {
        close(sock);
        return NULL;
    }

    host->n_frames = MAX_FRAMES_PER_CLIENT * 2;
    host->frames   = calloc(host->n_frames, sizeof(VSLFrame*));
    if (!host->frames) {
        close(sock);
        free(host);
        errno = ENOMEM;
        return NULL;
    }

    host->n_sockets = 1;
    host->sockets = calloc(host->n_sockets, sizeof(struct socket_and_frames*));

    if (!host->sockets) {
        close(sock);
        free(host->frames);
        free(host);
        errno = ENOMEM;
        return NULL;
    }

    host->sockets->one_socket = sock;
    host->path                = strdup(path);

    pthread_mutexattr_t thread_attr;
    pthread_mutexattr_init(&thread_attr);
    pthread_mutexattr_settype(&thread_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&host->lock, &thread_attr);

    return host;
}

VSL_API
void
vsl_host_release(VSLHost* host)
{
    if (!host) { return; }
    pthread_mutex_lock(&host->lock);

    for (int i = 0; i < host->n_frames; i++) {
        vsl_frame_release(host->frames[i]);
        host->frames[i] = NULL;
    }

    for (int i = 0; i < host->n_sockets; i++) {
        shutdown(host->sockets[i].one_socket, SHUT_RDWR);
        close(host->sockets[i].one_socket);
    }

    if (host->sockets) { free(host->sockets); }
    if (host->frames) { free(host->frames); }
    if (host->path) {
        unlink(host->path);
        free(host->path);
    }

    pthread_mutex_unlock(&host->lock);
    pthread_mutex_destroy(&host->lock);

    free(host);
}

VSL_API
const char*
vsl_host_path(const VSLHost* host)
{
    if (host) { return host->path; }
    return NULL;
}

VSL_API
int
vsl_host_post(VSLHost*  host,
              VSLFrame* frame,
              int64_t   expires,
              int64_t   duration,
              int64_t   pts,
              int64_t   dts)
{
    if (!host || !frame) {
        errno = EINVAL;
        return -1;
    }

    struct timespec locktimeout;
    clock_gettime(CLOCK_REALTIME, &locktimeout);
    timespec_add_nsec(&locktimeout, LOCK_TIMEOUT);
    int err = pthread_mutex_timedlock(&host->lock, &locktimeout);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return -1;
    }

    if (insert_frame(host, frame)) {
        fprintf(stderr,
                "%s host unable to insert frame: %s\n",
                __FUNCTION__,
                strerror(err));
        pthread_mutex_unlock(&host->lock);
        return -1;
    }

    frame->info.serial    = ++host->serial;
    frame->info.timestamp = vsl_timestamp();
    frame->info.expires   = expires;
    frame->info.duration  = duration;
    frame->info.dts       = dts;
    frame->info.pts       = pts;

    struct vsl_frame_event event;
    event.error = VSL_FRAME_SUCCESS;
    memcpy(&event.info, &frame->info, sizeof(struct vsl_frame_info));

    struct vsl_aux aux;
    memset(&aux, 0, sizeof(aux));
    aux.handle         = frame->handle;
    aux.hdr.cmsg_level = SOL_SOCKET;
    aux.hdr.cmsg_type  = SCM_RIGHTS;
    aux.hdr.cmsg_len   = CMSG_LEN(sizeof(int));

    struct iovec iov;
    iov.iov_base = &event;
    iov.iov_len  = sizeof(event);

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = &aux;
    msg.msg_controllen = sizeof(aux);

    for (int i = 1; i < host->n_sockets; i++) {
        if (host->sockets[i].one_socket != -1) {
            int64_t before_sendmsg = get_timestamp_us();
            ssize_t ret = sendmsg(host->sockets[i].one_socket, &msg, 0);
            int64_t after_sendmsg = get_timestamp_us();
            int64_t duration_us   = after_sendmsg - before_sendmsg;

            if (ret == -1) {
                fprintf(stderr,
                        "[TIMING][HOST] sendmsg to socket %d FAILED after %lld "
                        "us: %s\n",
                        i,
                        (long long) duration_us,
                        strerror(errno));
                disconnect_client_index(host, i);
            } else if (duration_us > 1000) {
                fprintf(stderr,
                        "[TIMING][HOST] sendmsg to socket %d took %lld us "
                        "(%.2f ms)\n",
                        i,
                        (long long) duration_us,
                        duration_us / 1000.0);
            }
        }
    }

    pthread_mutex_unlock(&host->lock);
    return 0;
}

VSL_API
int
vsl_host_drop(VSLHost* host, VSLFrame* frame)
{
    if (!host || !frame) {
        errno = EINVAL;
        return -1;
    }

    struct timespec locktimeout;
    clock_gettime(CLOCK_REALTIME, &locktimeout);
    timespec_add_nsec(&locktimeout, LOCK_TIMEOUT);
    int err = pthread_mutex_timedlock(&host->lock, &locktimeout);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return -1;
    }

    for (int i = 0; i < host->n_frames; i++) {
        if (host->frames[i] == frame) {
            host->frames[i] = NULL;
#ifndef NDEBUG
            printf("%s serial: %ld timestamp: %ld\n",
                   __FUNCTION__,
                   vsl_frame_serial(frame),
                   vsl_timestamp());
#endif
            pthread_mutex_unlock(&host->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&host->lock);
    fprintf(stderr,
            "%s frame %p is not owned by host %p\n",
            __FUNCTION__,
            frame,
            host);
    return -1;
}

VSL_API
VSLFrame*
vsl_frame_register(VSLHost*          host,
                   int64_t           serial,
                   int               handle,
                   int               width,
                   int               height,
                   uint32_t          fourcc,
                   size_t            size,
                   size_t            offset,
                   int64_t           expires,
                   int64_t           duration,
                   int64_t           pts,
                   int64_t           dts,
                   vsl_frame_cleanup cleanup,
                   void*             userptr)
{
    // NOTE: we're now ignoring serial which breaks the ABI but the function
    // is deprecated and serial really is meant to be owned by the host.
    (void) serial;

    VSLFrame* frame =
        vsl_frame_init(width, height, 0, fourcc, userptr, cleanup);
    if (!frame) { return NULL; }

    if (vsl_frame_attach(frame, handle, size, offset)) {
        vsl_frame_release(frame);
        return NULL;
    }

    if (vsl_host_post(host, frame, expires, duration, pts, dts)) {
        vsl_frame_release(frame);
        return NULL;
    }

    return frame;
}

VSL_API
void
vsl_frame_unregister(VSLFrame* frame)
{
    // Deprecated API which used to drop and free the frame.  Now we simply
    // call vsl_frame_release which will call vsl_host_drop which will drop the
    // frame before freeing itself in vsl_frame_release.
    vsl_frame_release(frame);
}

VSL_API
int
vsl_host_poll(VSLHost* host, int64_t wait)
{
    size_t        n_fds = 128;
    size_t        max_sockets;
    int           sockets[128];
    struct pollfd fds[128];

    if (!host) {
        errno = EINVAL;
        return -1;
    }

    memset(sockets, 0, sizeof(sockets));
    memset(fds, 0, sizeof(fds));

    if (vsl_host_sockets(host, n_fds, sockets, &max_sockets)) {
        fprintf(stderr,
                "%s failed acquire active sockets: %s\n",
                __FUNCTION__,
                strerror(errno));
        return -1;
    }

    if (max_sockets > n_fds) {
        printf("%s cannot handle all %zu client sockets\n",
               __FUNCTION__,
               max_sockets);
    }

    max_sockets = n_fds < max_sockets ? n_fds : max_sockets;

    for (size_t i = 0; i < max_sockets; i++) {
        fds[i].fd     = sockets[i];
        fds[i].events = POLLIN | POLLERR | POLLHUP;
    }

#ifndef NDEBUG
    printf("POLL %zu SOCKETS\n", max_sockets);
#endif

    return poll(fds, max_sockets, wait);
}

static int
recv_client_control(int sock, struct vsl_frame_control* control)
{
    errno       = 0;
    ssize_t len = recv(sock, control, sizeof(*control), 0);

#ifndef NDEBUG
    printf("%s %d read bytes: %zd error: %s\n",
           __FUNCTION__,
           sock,
           len,
           strerror(errno));
#endif

    if (len == -1) {
        switch (errno) {
        case EAGAIN:
            errno = ENOMSG;
            return -1;
        case ECONNRESET:
            return -1;
        default:
            fprintf(stderr,
                    "%s %d read error: %s\n",
                    __FUNCTION__,
                    sock,
                    strerror(errno));
            return -1;
        }
    } else if (len == 0) {
        errno = ECONNRESET;
        return -1;
    } else if (len != sizeof(*control)) {
        fprintf(stderr,
                "%s %d partial read %zd of %zu\n",
                __FUNCTION__,
                sock,
                len,
                sizeof(*control));
        errno = EBADMSG;
        return -1;
    }

    return 0;
}

static void
service_client_trylock(VSLHost*                        host,
                       int                             sock,
                       const struct vsl_frame_control* control,
                       struct vsl_frame_event*         event)
{
    int frameidx = -1;

    for (int i = 0; i < host->n_frames; i++) {
        if (host->frames[i] &&
            vsl_frame_serial(host->frames[i]) == control->serial) {
            frameidx = i;

            if (add_frame_to_socket(sock, host, host->frames[i])) {
                event->error = VSL_FRAME_TOO_MANY_FRAMES_LOCKED;
            } else if (host->frames[i]->info.locked >= 0) {
                host->frames[i]->info.locked++;
                event->info.locked = 1;
            }
#ifndef NDEBUG
            printf("%s trylock from %d on frame %ld\n",
                   __FUNCTION__,
                   sock,
                   control->serial);
#endif
        }
    }

    if (frameidx == -1) {
#ifndef NDEBUG
        fprintf(stderr,
                "%s trylock from %d on expired frame %ld\n",
                __FUNCTION__,
                sock,
                control->serial);
#endif
        event->error = VSL_FRAME_ERROR_EXPIRED;
    }
}

static void
service_client_unlock(VSLHost*                        host,
                      int                             sock,
                      const struct vsl_frame_control* control,
                      struct vsl_frame_event*         event)
{
    int frameidx = -1;

    for (int i = 0; i < host->n_frames; i++) {
        if (host->frames[i] &&
            vsl_frame_serial(host->frames[i]) == control->serial) {
            frameidx = i;
            break;
#ifndef NDEBUG
            printf("%s unlock from %d on frame %ld\n",
                   __FUNCTION__,
                   sock,
                   control->serial);
#endif
        }
    }

    if (frameidx == -1) {
#ifndef NDEBUG
        fprintf(stderr,
                "%s unlock from %d on expired frame %ld\n",
                __FUNCTION__,
                sock,
                control->serial);
#endif
        event->error = VSL_FRAME_ERROR_EXPIRED;
        return;
    }

    if (host->frames[frameidx]->info.locked > 0) {
        if (remove_frame_from_socket(sock, host, host->frames[frameidx])) {
#ifndef NDEBUG
            fprintf(stderr,
                    "%s frame to unlock not found for socket %d %ld\n",
                    __FUNCTION__,
                    sock,
                    control->serial);
#endif
        } else {
            host->frames[frameidx]->info.locked--;
        }
        event->info.locked = 0;
    }
}

static int
service_client(VSLHost* host, int sock)
{
    struct vsl_frame_control control;
    struct vsl_frame_event   event;

    if (recv_client_control(sock, &control)) { return -1; }

    memset(&event, 0, sizeof(event));

#ifndef NDEBUG
    printf("%s %d: %s %ld\n",
           __FUNCTION__,
           sock,
           control.message == VSL_FRAME_TRYLOCK  ? "lock"
           : control.message == VSL_FRAME_UNLOCK ? "unlock"
                                                 : "invalid",
           control.serial);
#endif

    switch (control.message) {
    case VSL_FRAME_TRYLOCK:
        service_client_trylock(host, sock, &control, &event);
        break;
    case VSL_FRAME_UNLOCK:
        service_client_unlock(host, sock, &control, &event);
        break;
    default:
        event.error = VSL_FRAME_ERROR_INVALID_CONTROL;
    }

    ssize_t ret = send(sock, &event, sizeof(event), 0);
    if (ret == -1) {
        if (errno != ECONNRESET) {
#ifndef NDEBUG
            fprintf(stderr,
                    "%s send error: %s\n",
                    __FUNCTION__,
                    strerror(errno));
#endif
        }
        return -1;
    }

    return 0;
}

static int
host_accept(VSLHost* host)
{
    errno = 0;

    int newsock = accept((*host->sockets).one_socket, NULL, NULL);
    if (newsock == -1) {
        if (errno != EBUSY && errno != EAGAIN) {
            fprintf(stderr,
                    "%s failed to accept connection: %s\n",
                    __FUNCTION__,
                    strerror(errno));
        }
        return -1;
    }

#ifndef NDEBUG
    printf("%s new client connection %d\n", __FUNCTION__, newsock);
#endif

    if (socket_blocking(newsock, 0)) {
        fprintf(stderr,
                "%s failed to set client socket non-blocking: %s\n",
                __FUNCTION__,
                strerror(errno));
        close(newsock);
        return -1;
    }

    return newsock;
}

static int
host_newsock(VSLHost* host, int newsock)
{
    int sockidx = 0;

    for (int i = 1; i < host->n_sockets; i++) {
        if (host->sockets[i].one_socket == -1) {
            sockidx = i;
            break;
        }
    }

    if (sockidx == 0) {
        size_t                    newlen = host->n_sockets * 2;
        struct socket_and_frames* newsocks =
            realloc(host->sockets, newlen * sizeof(struct socket_and_frames));

        if (!newsocks) {
            fprintf(stderr,
                    "%s cannot grow sockets list to %zu: %s\n",
                    __FUNCTION__,
                    newlen,
                    strerror(errno));
            errno = ENOMEM;
            return -1;
        }

        for (size_t i = host->n_sockets; i < newlen; i++) {
            newsocks[i].one_socket = -1;
            memset(newsocks[i].frames,
                   0,
                   sizeof(VSLFrame*) * MAX_FRAMES_PER_CLIENT);
        }

        sockidx         = host->n_sockets;
        host->sockets   = newsocks;
        host->n_sockets = newlen;
    }

    host->sockets[sockidx].one_socket = newsock;
    return 0;
}

VSL_API
int
vsl_host_service(VSLHost* host, int sock)
{
    if (!host) {
        errno = EINVAL;
        return -1;
    }

    struct timespec locktimeout;
    clock_gettime(CLOCK_REALTIME, &locktimeout);
    timespec_add_nsec(&locktimeout, LOCK_TIMEOUT);
    int err = pthread_mutex_timedlock(&host->lock, &locktimeout);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return -1;
    }

    int ret = service_client(host, sock);
    pthread_mutex_unlock(&host->lock);
    return ret;
}

VSL_API
int
vsl_host_process(VSLHost* host)
{
    if (!host) {
        errno = EINVAL;
        return -1;
    }

    struct timespec locktimeout;
    clock_gettime(CLOCK_REALTIME, &locktimeout);
    timespec_add_nsec(&locktimeout, LOCK_TIMEOUT);
    int err = pthread_mutex_timedlock(&host->lock, &locktimeout);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return -1;
    }

    int newsock = host_accept(host);
    if (newsock != -1) { host_newsock(host, newsock); }

    for (int i = 1; i < host->n_sockets; i++) {
        if (host->sockets[i].one_socket != -1) {
            if (service_client(host, host->sockets[i].one_socket)) {
                if (errno != ENOMSG) {
#ifndef NDEBUG
                    fprintf(stderr,
                            "%s failed to service client %d: %s\n",
                            __FUNCTION__,
                            i,
                            strerror(errno));
#endif
                    disconnect_client_index(host, i);
                }
            }
        }
    }
    expire_frames(host);
    pthread_mutex_unlock(&host->lock);
    return 0;
}

VSL_API
int
vsl_host_sockets(VSLHost* host,
                 size_t   n_sockets,
                 int*     sockets,
                 size_t*  max_sockets)
{
    size_t open_sockets = 0;

    if (!host) {
        errno = EINVAL;
        return -1;
    }

    int err = pthread_mutex_lock(&host->lock);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return -1;
    }

    for (int i = 0; i < host->n_sockets; i++) {
        if (host->sockets[i].one_socket != -1) { open_sockets++; }
    }

    if (max_sockets) { *max_sockets = open_sockets; }

    if (sockets) {
        int    i = 0;
        size_t j = 0;

        for (; i < host->n_sockets && j < n_sockets; i++) {
            if (host->sockets[i].one_socket != -1) {
                sockets[j++] = host->sockets[i].one_socket;
            }
        }
    }

    pthread_mutex_unlock(&host->lock);

    return 0;
}
