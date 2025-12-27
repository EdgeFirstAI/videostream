// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include "common.h"
#include "frame.h"
#include "videostream.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define INITIAL_BUFF_SIZE 1000
#define SOCKET_ERROR -1

// Timing instrumentation
static inline int64_t
get_timestamp_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

struct vsl_client {
    void*              userptr;
    char*              path;
    SOCKET             sock;
    pthread_mutex_t    lock;
    timer_t            timerid;
    struct itimerspec  trigger;
    struct sockaddr_un sock_addr;
    socklen_t          sock_addrlen;
    float              sock_timeout_secs;
    bool               reconnect;
    bool               is_reconnecting;
};

static float  DEFAULT_SOCK_TO_SECS = 1.0F;
static int    wait_stages_ms[]     = {0, 1, 5, 25, 100, 1000};
static size_t size_of_wait_stages_array =
    sizeof(wait_stages_ms) / sizeof(wait_stages_ms[0]);

static const char*
vsl_frame_strerror(enum vsl_frame_error err)
{
    switch (err) {
    case VSL_FRAME_SUCCESS:
        return "success";
    case VSL_FRAME_ERROR_EXPIRED:
        return "frame expired";
    case VSL_FRAME_ERROR_INVALID_CONTROL:
        return "invalid control";
    case VSL_FRAME_TOO_MANY_FRAMES_LOCKED:
        return "too many frames locked";
    }

    return "unknown error";
}

static int
vsl_frame_errno(enum vsl_frame_error err)
{
    switch (err) {
    case VSL_FRAME_SUCCESS:
        return 0;
    case VSL_FRAME_ERROR_EXPIRED:
        return ESTALE;
    case VSL_FRAME_ERROR_INVALID_CONTROL:
        return EBADMSG;
    case VSL_FRAME_TOO_MANY_FRAMES_LOCKED:
        return ENOLCK;
    }

    return EINVAL;
}

static void
timer_handler(union sigval sv)
{
    VSLClient* client = sv.sival_ptr;

    if (client->is_reconnecting) { return; }

    shutdown(client->sock, SHUT_RDWR);
    close(client->sock);
    client->sock = SOCKET_ERROR;
}

static void
create_timer(VSLClient* client)
{
    timer_t           timerid;
    struct sigevent   sev;
    struct itimerspec trigger;

    memset(&sev, 0, sizeof(struct sigevent));
    memset(&trigger, 0, sizeof(struct itimerspec));

    sev.sigev_notify          = SIGEV_THREAD;
    sev.sigev_notify_function = &timer_handler;
    sev.sigev_value.sival_ptr = client;

    timer_create(CLOCK_REALTIME, &sev, &timerid);

    trigger.it_value.tv_sec = (time_t) client->sock_timeout_secs;

    int   integer_part = (int) client->sock_timeout_secs;
    float decimal_part = client->sock_timeout_secs - integer_part;

    // converting to nanosecs
    decimal_part *= 10e9;

    trigger.it_value.tv_nsec = (uint64_t) decimal_part;

    client->timerid = timerid;
    client->trigger = trigger;

    if (trigger.it_value.tv_nsec == 0 && trigger.it_value.tv_sec == 0) {
        trigger.it_value.tv_sec = DEFAULT_SOCK_TO_SECS;
    }

    timer_settime(timerid, 0, &trigger, NULL);
}

static void
restart_timer(VSLClient* client)
{
    if (client->timerid) {
        timer_settime(client->timerid, 0, &client->trigger, NULL);
    }
}

static void
delete_timer(VSLClient* client)
{
    if (client->timerid) { timer_delete(client->timerid); }
}

static int
get_socket(struct sockaddr_un addr, socklen_t addrlen)
{
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1) {
        fprintf(stderr,
                "%s failed to create socket: %s\n",
                __FUNCTION__,
                strerror(errno));
        errno = ECONNREFUSED;
        return -1;
    }

    // Connect with blocking socket (connect() on non-blocking sockets
    // requires special handling with poll/select)
    if (connect(sock, (struct sockaddr*) &addr, addrlen)) {
#ifndef NDEBUG
        fprintf(stderr,
                "%s failed to connect unix socket on %s: %s\n",
                __FUNCTION__,
                addr.sun_path,
                strerror(errno));
#endif
        close(sock);
        errno = ECONNREFUSED;
        return -1;
    }

    // Set to non-blocking AFTER successful connection
    if (socket_blocking(sock, 0)) {
        fprintf(stderr,
                "%s failed to set socket non-blocking: %s\n",
                __FUNCTION__,
                strerror(errno));
        close(sock);
        errno = ECONNREFUSED;
        return -1;
    }

    return sock;
}

VSL_API void
vsl_client_disconnect(VSLClient* client)
{
    if (!client) {
        errno = EINVAL;
        return;
    }

#ifndef NDEBUG
    printf("%s %p sock %d\n", __FUNCTION__, client, client->sock);
#endif

    client->reconnect = false;
    shutdown(client->sock, SHUT_RDWR);
    close(client->sock);
    client->sock = SOCKET_ERROR;
}

VSL_API VSLClient*
vsl_client_init(const char* path, void* userptr, bool reconnect)
{
    int                sock    = -1;
    struct sockaddr_un addr    = {0};
    socklen_t          addrlen = 0;

    if (sockaddr_from_path(path, &addr, &addrlen)) {
        fprintf(stderr,
                "%s invalid socket path: %s\n",
                __FUNCTION__,
                strerror(errno));
        return NULL;
    }

    VSLClient* client = calloc(1, sizeof(VSLClient));
    if (!client) {
        errno = ENOMEM;
        return NULL;
    }

    uint8_t current_wait_stage = 0;
    client->reconnect          = reconnect;

    while (true) {
        sock = get_socket(addr, addrlen);

        if (sock == -1) {
            if (client->reconnect) {
                usleep(wait_stages_ms[current_wait_stage] * 1000);
            } else {
                free(client);
                return NULL;
            }
        } else {
            break;
        }
    }

    client->userptr           = userptr;
    client->sock              = sock;
    client->path              = strdup(path);
    client->sock_addr         = addr;
    client->sock_addrlen      = addrlen;
    client->sock_timeout_secs = DEFAULT_SOCK_TO_SECS;
    client->is_reconnecting   = false;

    create_timer(client);

    pthread_mutexattr_t thread_attr;
    pthread_mutexattr_init(&thread_attr);
    pthread_mutexattr_settype(&thread_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&client->lock, &thread_attr);

    return client;
}

VSL_API
void
vsl_client_release(VSLClient* client)
{
    if (!client) { return; }

#ifndef NDEBUG
    printf("%s %p\n", __FUNCTION__, client);
#endif

    free(client->path);
    delete_timer(client);
    shutdown(client->sock, SHUT_RDWR);
    close(client->sock);
    pthread_mutex_destroy(&client->lock);
    free(client);
}

VSL_API
void*
vsl_client_userptr(VSLClient* client)
{
    if (!client) {
        errno = EINVAL;
        return NULL;
    }

    return client->userptr;
}

VSL_API
const char*
vsl_client_path(const VSLClient* client)
{
    if (client) { return client->path; }
    return NULL;
}

VSL_API
void
vsl_client_set_timeout(VSLClient* client, float timeout)
{
    if (timeout < 0) {
        client->sock_timeout_secs = DEFAULT_SOCK_TO_SECS;
    } else {
        client->sock_timeout_secs = timeout;
    }

    delete_timer(client);
    create_timer(client);
}

VSL_API
VSLFrame*
vsl_frame_wait(VSLClient* client, int64_t until)
{
    ssize_t                ret   = 0;
    struct vsl_frame_event event = {0};
    struct vsl_aux         aux;
    struct msghdr          msg;
    struct iovec           iov;
    int                    sock;

    if (!client) {
        errno = EINVAL;
        return NULL;
    }

    int err = pthread_mutex_lock(&client->lock);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return NULL;
    }

    while (1) {
        memset(&msg, 0, sizeof(msg));
        memset(&iov, 0, sizeof(iov));
        memset(&aux, 0, sizeof(aux));
        memset(&event, 0, sizeof(event));
        aux.handle      = -1; // Initialize to invalid fd (not 0 which is stdin)
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;
        msg.msg_control = &aux;
        msg.msg_controllen = sizeof(aux);
        iov.iov_base       = &event;
        iov.iov_len        = sizeof(event);

        if (client->sock == SOCKET_ERROR) {
            sock = get_socket(client->sock_addr, client->sock_addrlen);

            if (sock == -1 && !client->reconnect) { return NULL; }

            if (sock >= 0) { client->sock = sock; }
        }

        uint8_t current_wait_stage = 0;
        bool    tried_to_reconnect = false;

        while (true) {

            if (client->sock >= 0) {
                // Restart watchdog timer before each operation
                restart_timer(client);

                // Call recvmsg() directly (non-blocking) to drain queue
                int64_t before_recvmsg = get_timestamp_us();
                errno                  = 0;
                ret                    = recvmsg(client->sock, &msg, 0);
                int64_t after_recvmsg  = get_timestamp_us();
                int64_t duration_us    = after_recvmsg - before_recvmsg;
                if (duration_us > 5000) {
                    fprintf(stderr,
                            "[TIMING][CLIENT] recvmsg took %lld us (%.2f ms)\n",
                            (long long) duration_us,
                            duration_us / 1000.0);
                }
            } else {
#ifndef NDEBUG
                printf("%s client not connected\n", __FUNCTION__);
#endif
                break;
            }

#ifndef NDEBUG
            printf("%s client: %d read: %zd event: %zu error: %s\n",
                   __FUNCTION__,
                   client->sock,
                   ret,
                   sizeof(event),
                   strerror(errno));
#endif

            if (ret == -1) {
                // For non-blocking sockets, EAGAIN/EWOULDBLOCK means no data
                // available
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available, use poll() to wait for next frame
                    struct pollfd pfd;
                    pfd.fd     = client->sock;
                    pfd.events = POLLIN;

                    restart_timer(client);
                    int poll_ret =
                        poll(&pfd, 1, client->sock_timeout_secs * 1000);

                    if (poll_ret == -1) {
                        if (errno == EINTR) {
                            continue; // Interrupted, try again
                        }
                        fprintf(stderr,
                                "%s poll error: %s\n",
                                __FUNCTION__,
                                strerror(errno));
                        if (!client->reconnect) {
                            pthread_mutex_unlock(&client->lock);
                            return NULL;
                        }
                        client->is_reconnecting = true;
                        shutdown(client->sock, SHUT_RDWR);
                        close(client->sock);
                        client->sock = SOCKET_ERROR;
                    } else if (poll_ret == 0) {
                        // Timeout
                        errno = ETIMEDOUT;
                        pthread_mutex_unlock(&client->lock);
                        return NULL;
                    }
                    // poll() succeeded, data is ready, continue loop to call
                    // recvmsg() again
                    continue;
                }

                // Real error (not EAGAIN)
                if (!client->reconnect) {
                    fprintf(stderr,
                            "%s read error: %s\n",
                            __FUNCTION__,
                            strerror(errno));
                    pthread_mutex_unlock(&client->lock);
                    return NULL;
                }

                client->is_reconnecting = true;
                shutdown(client->sock, SHUT_RDWR);
                close(client->sock);
                client->sock = SOCKET_ERROR;
            } else if (ret == 0) {
#ifndef NDEBUG
                printf("%s client %d no message\n", __FUNCTION__, client->sock);
#endif
                client->is_reconnecting = true;
                shutdown(client->sock, SHUT_RDWR);
                close(client->sock);
                int prev_sock = client->sock;
                client->sock  = SOCKET_ERROR;

                if (!client->reconnect) {
                    fprintf(stderr,
                            "%s client %d connection closed: %s\n",
                            __FUNCTION__,
                            prev_sock,
                            strerror(errno));
                    pthread_mutex_unlock(&client->lock);
                    return NULL;
                }

                tried_to_reconnect = true;

                if (current_wait_stage < size_of_wait_stages_array - 1) {
                    current_wait_stage++;
                }

                usleep(wait_stages_ms[current_wait_stage] * 1000);

                // connection was closed, try to get a new socket
                sock = get_socket(client->sock_addr, client->sock_addrlen);

                if (sock >= 0) { client->sock = sock; }
            } else {
#ifndef NDEBUG
                printf("%s client %d got message\n",
                       __FUNCTION__,
                       client->sock);
#endif
                client->is_reconnecting = false;
                break;
            }
        }

        // If the connection was dropped, ignore the first message received
        // as the dma buffer might be invalid, so just restart the loop
        if (tried_to_reconnect) {
#ifndef NDEBUG
            printf("%s client %d ignoring first message after reconnect\n",
                   __FUNCTION__,
                   client->sock);
#endif
            continue;
        }

        current_wait_stage = 0;

        if (event.error) {
            fprintf(stderr,
                    "%s event error: %s\n",
                    __FUNCTION__,
                    vsl_frame_strerror(event.error));
            pthread_mutex_unlock(&client->lock);
            errno = vsl_frame_errno(event.error);
            return NULL;
        }

        restart_timer(client);

#ifndef NDEBUG
        printf("%s client %d event serial: %ld timestamp: %ld expires: %ld\n",
               __FUNCTION__,
               client->sock,
               event.info.serial,
               event.info.timestamp,
               event.info.expires);
#endif

        // non-frame event.
        if (!event.info.serial) {
            if (aux.handle > 2) { close(aux.handle); }
            continue;
        }

        // Ignore expired frame events.
        if (event.info.expires && event.info.expires < vsl_timestamp()) {
            if (aux.handle > 2) { close(aux.handle); }
            continue;
        }

        if (until && until > event.info.timestamp) {
#ifndef NDEBUG
            printf("%s WAIT serial: %lu timestamp: %lu until: %lu\n",
                   __FUNCTION__,
                   event.info.serial,
                   event.info.timestamp,
                   until);
#endif
            if (aux.handle > 2) { close(aux.handle); }
            continue;
        }

        break;
    }

#ifndef NDEBUG
    printf("%s client %d got frame %d %dx%d %c%c%c%c\n",
           __FUNCTION__,
           client->sock,
           aux.handle,
           event.info.width,
           event.info.height,
           event.info.fourcc,
           event.info.fourcc >> 8,
           event.info.fourcc >> 16,
           event.info.fourcc >> 24);
#endif

    // Debug: check if aux.handle is valid
    // After recvmsg, msg_controllen is updated to indicate how much was
    // received
    if (aux.handle <= 2) {
        fprintf(stderr,
                "%s: WARNING: aux.handle=%d (should be > 2), "
                "msg_controllen=%zu (expected %zu)\n",
                __FUNCTION__,
                aux.handle,
                msg.msg_controllen,
                sizeof(aux));
    }

    // If we received fd 0, something closed stdin. Reject this frame.
    if (aux.handle == 0) {
        fprintf(stderr,
                "%s: ERROR: received fd 0 - stdin was closed somewhere!\n",
                __FUNCTION__);
        pthread_mutex_unlock(&client->lock);
        errno = EBADF;
        return NULL;
    }

    VSLFrame* frame = calloc(1, sizeof(VSLFrame));
    if (!frame) {
        pthread_mutex_unlock(&client->lock);
        if (aux.handle > 2) { close(aux.handle); }
        return NULL;
    }

    frame->client    = client;
    frame->handle    = aux.handle;
    frame->allocator = VSL_FRAME_ALLOCATOR_EXTERNAL;
    memcpy(&frame->info, &event.info, sizeof(struct vsl_frame_info));

    pthread_mutex_unlock(&client->lock);

#ifndef NDEBUG
    printf("%s client %d returning frame %p parent %p %dx%d\n",
           __FUNCTION__,
           client->sock,
           frame,
           frame->_parent,
           vsl_frame_width(frame),
           vsl_frame_height(frame));
#endif

    return frame;
}

VSL_API
int
vsl_frame_trylock(VSLFrame* frame)
{
    ssize_t                  ret;
    struct vsl_frame_control control = {0};
    struct vsl_frame_event   event   = {0};

    if (!frame) {
        errno = EINVAL;
        return -1;
    }

    VSLClient* client = frame->client;

    if (!client) {
        errno = EINVAL;
        return -1;
    }

    int err = pthread_mutex_lock(&client->lock);
    if (err) {
        fprintf(stderr,
                "%s pthread_mutex_lock failed: %s\n",
                __FUNCTION__,
                strerror(err));
        errno = err;
        return -1;
    }

    memset(&control, 0, sizeof(control));
    control.message = VSL_FRAME_TRYLOCK;
    control.serial  = vsl_frame_serial(frame);

    uint8_t current_wait_stage = 0;

    while (true) {
        if (client->sock >= 0) {
            ret = send(client->sock, &control, sizeof(control), 0);
        } else {
            ret = 0;
        }

        if (ret == -1) {
            if (!client->reconnect) {
                fprintf(stderr,
                        "%s read error: %s\n",
                        __FUNCTION__,
                        strerror(errno));

                pthread_mutex_unlock(&client->lock);
                return -1;
            }

            shutdown(client->sock, SHUT_RDWR);
            close(client->sock);
            client->sock = SOCKET_ERROR;
            break;
        } else if (ret == 0) {
            client->is_reconnecting = true;
            shutdown(client->sock, SHUT_RDWR);
            close(client->sock);
            client->sock = SOCKET_ERROR;

            if (!client->reconnect) {
                fprintf(stderr,
                        "%s connection closed: %s\n",
                        __FUNCTION__,
                        strerror(errno));
                pthread_mutex_unlock(&client->lock);
                return -1;
            }

            if (current_wait_stage < size_of_wait_stages_array - 1) {
                current_wait_stage++;
            }

            usleep(wait_stages_ms[current_wait_stage] * 1000);

            // connection was closed, try to get a new socket
            int sock = get_socket(client->sock_addr, client->sock_addrlen);

            if (sock >= 0) { client->sock = sock; }
        } else {
            client->is_reconnecting = false;
            break;
        }
    }

    current_wait_stage = 0;

    do {
        memset(&event, 0, sizeof(event));

        if (client->sock >= 0) {
            ret = recv(client->sock, &event, sizeof(event), 0);
        } else {
            ret = 0;
        }

        if (ret == -1) {
            if (!client->reconnect) {
                fprintf(stderr,
                        "%s read error: %s\n",
                        __FUNCTION__,
                        strerror(errno));
                pthread_mutex_unlock(&client->lock);
                return -1;
            }

            shutdown(client->sock, SHUT_RDWR);
            close(client->sock);
            client->sock = SOCKET_ERROR;

            break;

        } else if (ret == 0) {
            client->is_reconnecting = true;
            shutdown(client->sock, SHUT_RDWR);
            close(client->sock);
            client->sock = SOCKET_ERROR;

            if (!client->reconnect) {
                fprintf(stderr,
                        "%s connection closed: %s\n",
                        __FUNCTION__,
                        strerror(errno));
                pthread_mutex_unlock(&client->lock);
                return -1;
            }

            if (current_wait_stage < size_of_wait_stages_array - 1) {
                current_wait_stage++;
            }

            usleep(wait_stages_ms[current_wait_stage] * 1000);
            // connection was closed, try to get a new socket
            int sock = get_socket(client->sock_addr, client->sock_addrlen);
            if (sock >= 0) { client->sock = sock; }
        }
    } while (event.info.serial); // non-zero serial indicates a frame event.

    client->is_reconnecting = false;

    switch (event.error) {
    case VSL_FRAME_ERROR_EXPIRED:
        pthread_mutex_unlock(&client->lock);
#ifndef NDEBUG
        fprintf(stderr, "%s frame %ld expired\n", __FUNCTION__, control.serial);
#endif
        errno = EEXIST;
        return -1;
    case VSL_FRAME_ERROR_INVALID_CONTROL:
        pthread_mutex_unlock(&client->lock);
#ifndef NDEBUG
        fprintf(stderr,
                "%s invalid control message %d\n",
                __FUNCTION__,
                control.message);
#endif
        errno = EINVAL;
        return -1;

    case VSL_FRAME_TOO_MANY_FRAMES_LOCKED:
        pthread_mutex_unlock(&client->lock);
#ifndef NDEBUG
        fprintf(stderr,
                "%s too many frames locked by this client\n",
                __FUNCTION__);
#endif
        errno = EMFILE;
        return -1;
    case VSL_FRAME_SUCCESS:
        break;
    }

    pthread_mutex_unlock(&client->lock);

    return 0;
}

VSL_API
int
vsl_frame_unlock(VSLFrame* frame)
{
    ssize_t                  ret;
    struct vsl_frame_control control = {0};
    struct vsl_frame_event   event   = {0};

    if (!frame) {
        errno = EINVAL;
        return -1;
    }

    vsl_frame_munmap(frame);

    VSLClient* client = frame->client;
    if (!client) { return 0; }

    int err = pthread_mutex_lock(&client->lock);
    if (err) {
        fprintf(stderr,
                "%s %p pthread_mutex_lock %p failed: %s\n",
                __FUNCTION__,
                frame,
                client,
                strerror(err));
        errno = err;
        return -1;
    }

    memset(&control, 0, sizeof(control));
    control.message = VSL_FRAME_UNLOCK;
    control.serial  = vsl_frame_serial(frame);

    if (client->sock >= 0) {
        ret = send(client->sock, &control, sizeof(control), 0);
    } else {
        // If client was disconnected, no frame is locked, so there is no need
        // to send any message
        fprintf(stderr,
                "%s socket disconnected, no frame to unlock\n",
                __FUNCTION__);
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    if (ret == -1) {
        fprintf(stderr,
                "%s failed to send: %s\n",
                __FUNCTION__,
                strerror(errno));

        shutdown(client->sock, SHUT_RDWR);
        close(client->sock);
        client->sock = SOCKET_ERROR;

        pthread_mutex_unlock(&client->lock);
        return -1;
    } else if (ret == 0) {
        fprintf(stderr,
                "%s connection closed: %s\n",
                __FUNCTION__,
                strerror(errno));

        shutdown(client->sock, SHUT_RDWR);
        close(client->sock);
        client->sock = SOCKET_ERROR;

        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    do {
        if (client->sock >= 0) {
            // Use poll() to wait for data since socket is non-blocking
            struct pollfd pfd;
            pfd.fd       = client->sock;
            pfd.events   = POLLIN;
            pfd.revents  = 0;
            int poll_ret = poll(&pfd, 1, 1000); // 1 second timeout
            if (poll_ret == -1) {
                fprintf(stderr,
                        "%s poll error: %s\n",
                        __FUNCTION__,
                        strerror(errno));
                shutdown(client->sock, SHUT_RDWR);
                close(client->sock);
                client->sock = SOCKET_ERROR;
                pthread_mutex_unlock(&client->lock);
                return -1;
            } else if (poll_ret == 0) {
                // Timeout - protocol state is now indeterminate, close socket
                fprintf(stderr,
                        "%s timeout waiting for unlock response\n",
                        __FUNCTION__);
                shutdown(client->sock, SHUT_RDWR);
                close(client->sock);
                client->sock = SOCKET_ERROR;
                pthread_mutex_unlock(&client->lock);
                errno = ETIMEDOUT;
                return -1;
            }
            ret = recv(client->sock, &event, sizeof(event), 0);
        } else {
            // If client was disconnected, no frame is locked, so there is no
            // need to receive any message
            fprintf(stderr,
                    "%s socket disconnected, no response to wait for\n",
                    __FUNCTION__);
            pthread_mutex_unlock(&client->lock);
            return -1;
        }

        if (ret == -1) {
            // Handle EAGAIN/EWOULDBLOCK - shouldn't happen after poll() but be
            // safe
            if (errno == EAGAIN || errno == EWOULDBLOCK) { continue; }

            fprintf(stderr,
                    "%s read error: %s\n",
                    __FUNCTION__,
                    strerror(errno));

            shutdown(client->sock, SHUT_RDWR);
            close(client->sock);
            client->sock = SOCKET_ERROR;

            pthread_mutex_unlock(&client->lock);
            return -1;
        } else if (ret == 0) {
            fprintf(stderr,
                    "%s connection closed: %s\n",
                    __FUNCTION__,
                    strerror(errno));

            shutdown(client->sock, SHUT_RDWR);
            close(client->sock);
            client->sock = SOCKET_ERROR;

            pthread_mutex_unlock(&client->lock);
            return -1;
        }
    } while (event.info.serial); // non-zero serial indicates frame event.

    switch (event.error) {
    case VSL_FRAME_ERROR_EXPIRED:
        pthread_mutex_unlock(&client->lock);
        fprintf(stderr, "%s frame %ld expired\n", __FUNCTION__, control.serial);
        errno = EEXIST;
        return -1;
    case VSL_FRAME_ERROR_INVALID_CONTROL:
        pthread_mutex_unlock(&client->lock);
        fprintf(stderr,
                "%s invalid control message %d\n",
                __FUNCTION__,
                control.message);
        errno = EINVAL;
        return -1;
    case VSL_FRAME_TOO_MANY_FRAMES_LOCKED:
        pthread_mutex_unlock(&client->lock);
        fprintf(stderr, "%s too many frames locked\n", __FUNCTION__);
        errno = ENOLCK;
        return -1;
    default:
        break;
    }

    pthread_mutex_unlock(&client->lock);

    return 0;
}
