// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VIDEOSTREAM_H
#define VIDEOSTREAM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * VideoStream Library Version
 * Format: "MAJOR.MINOR.PATCH"
 * This is the single source of truth - updated by cargo-release
 */
#define VSL_VERSION "1.4.0-rc0"

#define VSL_VERSION_ENCODE(major, minor, revision) \
    (((major) * 1000000) + ((minor) * 1000) + (revision))
#define VSL_VERSION_DECODE_MAJOR(version) ((version) / 1000000)
#define VSL_VERSION_DECODE_MINOR(version) (((version) % 1000000) / 1000)
#define VSL_VERSION_DECODE_REVISION(version) ((version) % 1000)

#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#define VSL_GNUC_VERSION \
    VSL_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__)
#define VSL_GNUC_VERSION VSL_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, 0)
#endif

#if defined(VSL_GNUC_VERSION)
#define VSL_GNUC_VERSION_CHECK(major, minor, patch) \
    (VSL_GNUC_VERSION >= VSL_VERSION_ENCODE(major, minor, patch))
#else
#define VSL_GNUC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__CC_ARM) && defined(__ARMCOMPILER_VERSION)
#define VSL_ARM_VERSION                                           \
    VSL_VERSION_ENCODE(__ARMCOMPILER_VERSION / 1000000,           \
                       (__ARMCOMPILER_VERSION % 1000000) / 10000, \
                       (__ARMCOMPILER_VERSION % 10000) / 100)
#elif defined(__CC_ARM) && defined(__ARMCC_VERSION)
#define VSL_ARM_VERSION                                     \
    VSL_VERSION_ENCODE(__ARMCC_VERSION / 1000000,           \
                       (__ARMCC_VERSION % 1000000) / 10000, \
                       (__ARMCC_VERSION % 10000) / 100)
#endif

#if defined(VSL_ARM_VERSION)
#define VSL_ARM_VERSION_CHECK(major, minor, patch) \
    (VSL_ARM_VERSION >= VSL_VERSION_ENCODE(major, minor, patch))
#else
#define VSL_ARM_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__IAR_SYSTEMS_ICC__)
#if __VER__ > 1000
#define VSL_IAR_VERSION                           \
    VSL_VERSION_ENCODE((__VER__ / 1000000),       \
                       ((__VER__ / 1000) % 1000), \
                       (__VER__ % 1000))
#else
#define VSL_IAR_VERSION VSL_VERSION_ENCODE(VER / 100, __VER__ % 100, 0)
#endif
#endif

#if defined(VSL_IAR_VERSION)
#define VSL_IAR_VERSION_CHECK(major, minor, patch) \
    (VSL_IAR_VERSION >= VSL_VERSION_ENCODE(major, minor, patch))
#else
#define VSL_IAR_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(VSL_GNUC_VERSION) && !defined(__clang) && !defined(VSL_ARM_VERSION)
#define VSL_GCC_VERSION VSL_GNUC_VERSION
#endif

#if defined(VSL_GCC_VERSION)
#define VSL_GCC_VERSION_CHECK(major, minor, patch) \
    (VSL_GCC_VERSION >= VSL_VERSION_ENCODE(major, minor, patch))
#else
#define VSL_GCC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__cplusplus) && (__cplusplus >= 201402L)
#define VSL_DEPRECATED(since) [[deprecated("Since " #since)]]
#define VSL_DEPRECATED_FOR(since, replacement) \
    [[deprecated("Since " #since "; use " #replacement)]]
#elif _MSC_VER >= 1400
#define VSL_DEPRECATED(since) __declspec(deprecated("Since " #since))
#define VSL_DEPRECATED_FOR(since, replacement) \
    __declspec(deprecated("Since " #since "; use " #replacement))
#elif _MSC_VER >= 1310
#define VSL_DEPRECATED(since) _declspec(deprecated)
#define VSL_DEPRECATED_FOR(since, replacement) __declspec(deprecated)
#elif VSL_IAR_VERSION_CHECK(8, 0, 0)
#define VSL_DEPRECATED(since) _Pragma("deprecated")
#define VSL_DEPRECATED_FOR(since, replacement) _Pragma("deprecated")
#elif defined(_GHS_MULTI)
#define VSL_DEPRECATED(since)
#define VSL_DEPRECATED_FOR(since, replacement)
#else
#define VSL_DEPRECATED(since) __attribute__((__deprecated__("Since " #since)))
#define VSL_DEPRECATED_FOR(since, replacement) \
    __attribute__((__deprecated__("Since " #since "; use " #replacement)))
#endif

#if VSL_GCC_VERSION_CHECK(4, 3, 0)
#define VSL_UNAVAILABLE(available_since) \
    __attribute__((__warning__("Not available until " #available_since)))
#else
#define VSL_UNAVAILABLE(available_since)
#endif

#if defined(__cplusplus) && (__cplusplus >= 201703L)
#define VSL_WARN_UNUSED_RESULT [[nodiscard]]
#elif defined(_Check_return_) /* SAL */
#define VSL_WARN_UNUSED_RESULT _Check_return_
#else
#define VSL_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#define VSL_PRIVATE
#define VSL_PUBLIC __declspec(dllexport)
#define VSL_IMPORT __declspec(dllimport)
#else
#define VSL_PRIVATE __attribute__((__visibility__("hidden")))
#define VSL_PUBLIC __attribute__((__visibility__("default")))
#define VSL_IMPORT extern
#endif

#if !defined(__cplusplus) &&                                         \
    ((defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
     defined(_Static_assert))
#define VSL_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
#elif (defined(__cplusplus) && (__cplusplus >= 201703L)) || (_MSC_VER >= 1600)
#define VSL_STATIC_ASSERT(expr, message) static_assert(expr, message)
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
#define VSL_STATIC_ASSERT(expr, message) static_assert(expr)
#else
#define VSL_STATIC_ASSERT(expr, message)
#endif

#ifdef VSL_API_STATIC
#define VSL_API
#else
#ifdef VSL_API_EXPORT
#define VSL_API VSL_PUBLIC
#else
#define VSL_API VSL_IMPORT
#endif
#endif

#define VSL_VERSION_1_0 VSL_VERSION_ENCODE(1, 0, 0)
#define VSL_VERSION_1_1 VSL_VERSION_ENCODE(1, 1, 0)
#define VSL_VERSION_1_2 VSL_VERSION_ENCODE(1, 2, 0)
#define VSL_VERSION_1_3 VSL_VERSION_ENCODE(1, 3, 0)
#define VSL_VERSION_1_4 VSL_VERSION_ENCODE(1, 4, 0)

#ifndef VSL_TARGET_VERSION
#define VSL_TARGET_VERSION VSL_VERSION_1_4
#endif

#if VSL_TARGET_VERSION < VSL_VERSION_ENCODE(1, 0, 0)
#define VSL_AVAILABLE_SINCE_1_0 VSL_UNAVAILABLE(1.0)
#define VSL_DEPRECATED_SINCE_1_0
#define VSL_DEPRECATED_SINCE_1_0_FOR(replacement)
#else
#define VSL_AVAILABLE_SINCE_1_0
#define VSL_DEPRECATED_SINCE_1_0 VSL_DEPRECATED(1.0)
#define VSL_DEPRECATED_SINCE_1_0_FOR(replacement) \
    VSL_DEPRECATED_FOR(1.0, replacement)
#endif

#if VSL_TARGET_VERSION < VSL_VERSION_ENCODE(1, 1, 0)
#define VSL_AVAILABLE_SINCE_1_1 VSL_UNAVAILABLE(1.1)
#define VSL_DEPRECATED_SINCE_1_1
#define VSL_DEPRECATED_SINCE_1_1_FOR(replacement)
#else
#define VSL_AVAILABLE_SINCE_1_1
#define VSL_DEPRECATED_SINCE_1_1 VSL_DEPRECATED(1.1)
#define VSL_DEPRECATED_SINCE_1_1_FOR(replacement) \
    VSL_DEPRECATED_FOR(1.1, replacement)
#endif

#if VSL_TARGET_VERSION < VSL_VERSION_ENCODE(1, 2, 0)
#define VSL_AVAILABLE_SINCE_1_2 VSL_UNAVAILABLE(1.2)
#define VSL_DEPRECATED_SINCE_1_2
#define VSL_DEPRECATED_SINCE_1_2_FOR(replacement)
#else
#define VSL_AVAILABLE_SINCE_1_2
#define VSL_DEPRECATED_SINCE_1_2 VSL_DEPRECATED(1.2)
#define VSL_DEPRECATED_SINCE_1_2_FOR(replacement) \
    VSL_DEPRECATED_FOR(1.2, replacement)
#endif

#if VSL_TARGET_VERSION < VSL_VERSION_ENCODE(1, 3, 0)
#define VSL_AVAILABLE_SINCE_1_3 VSL_UNAVAILABLE(1.3)
#define VSL_DEPRECATED_SINCE_1_3
#define VSL_DEPRECATED_SINCE_1_3_FOR(replacement)
#else
#define VSL_AVAILABLE_SINCE_1_3
#define VSL_DEPRECATED_SINCE_1_3 VSL_DEPRECATED(1.3)
#define VSL_DEPRECATED_SINCE_1_3_FOR(replacement) \
    VSL_DEPRECATED_FOR(1.3, replacement)
#endif

#if VSL_TARGET_VERSION < VSL_VERSION_ENCODE(1, 4, 0)
#define VSL_AVAILABLE_SINCE_1_4 VSL_UNAVAILABLE(1.4)
#define VSL_DEPRECATED_SINCE_1_4
#define VSL_DEPRECATED_SINCE_1_4_FOR(replacement)
#else
#define VSL_AVAILABLE_SINCE_1_4
#define VSL_DEPRECATED_SINCE_1_4 VSL_DEPRECATED(1.4)
#define VSL_DEPRECATED_SINCE_1_4_FOR(replacement) \
    VSL_DEPRECATED_FOR(1.4, replacement)
#endif

#define VSL_FOURCC(a, b, c, d)                                         \
    ((uint32_t) (a) | ((uint32_t) (b) << 8) | ((uint32_t) (c) << 16) | \
     ((uint32_t) (d) << 24))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The VSLHost object manages a connection point at the user-defined path and
 * allows frames to be registered for client use.
 */
typedef struct vsl_host VSLHost;

/**
 * The VSLClient object manages a single connection to a VSLHost.
 */
typedef struct vsl_client VSLClient;

/**
 * The VSLFrame object represents a single video frame from either the host
 * or client perspective.  Certain API are only available to the host or client.
 */
typedef struct vsl_frame VSLFrame;

/**
 * The VSLEncoder object represents encoder instance.
 *
 */
typedef struct vsl_encoder VSLEncoder;

/**
 * The VSLEncoder object represents encoder instance.
 *
 */
typedef struct vsl_decoder VSLDecoder;

/**
 * The VSLRect structure represents a rectangle region of a frame and is used to
 * define cropping regions for sub-frames.
 */
typedef struct vsl_rect {
    /**
     * The left-most pixel offset for the rectangle.
     */
    int x;
    /**
     * The top-most pixel offset for the rectangle.
     */
    int y;
    /**
     * The width in pixels of the rectangle.  The end position is x+width.
     */
    int width;
    /**
     * The height in pixels of the rectangle.  The end position is y+height.
     */
    int height;
} VSLRect;

// TODO: Define profiles that are usable for different codecs and encoders,
// they could mean different things for different codecs and encoders but
// overall they should provide noticeable difference between output size to
// quality ratio and in some cases performance (depending on codec and encoder),
// they can also mean a specific thing for specific encoder and format combo,
// ie. for HEVC still image encoding MSP profile is used.
typedef enum vsl_encode_profile {
    VSL_ENCODE_PROFILE_AUTO, // Encoder default, auto settings
    VSL_ENCODE_PROFILE_5000_KBPS,
    VSL_ENCODE_PROFILE_25000_KBPS,
    VSL_ENCODE_PROFILE_50000_KBPS,
    VSL_ENCODE_PROFILE_100000_KBPS,
} VSLEncoderProfile;

typedef enum {
    VSL_DEC_H264,
    VSL_DEC_HEVC,
} VSLDecoderCodec;

/**
 * Function pointer definition which will be called as part of
 * @ref vsl_frame_unregister.  This is typically used to free resources
 * associated with the frame on either client or host side.
 */
typedef void (*vsl_frame_cleanup)(VSLFrame* frame);

/**
 * Returns the VideoStream Library version.
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
const char*
vsl_version();

/**
 *
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_timestamp();

/**
 * Creates a host on the requested path.  If the path is unavailable because
 * of permissions or already exists then NULL is returned and errno is set.
 *
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
VSLHost*
vsl_host_init(const char* path);

/**
 * Releases the host, disconnecting all clients and releasing any allocated
 * memory.
 *
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_host_release(VSLHost* host);

/**
 * Returns the bound path of the host.
 *
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
const char*
vsl_host_path(const VSLHost* host);

/**
 * Polls the list of available connections in our pool.  If @param wait is >0
 * then poll will timeout after @param wait milliseconds.  Note frames are only
 * expired by the @ref vsl_host_process function so the @param wait parameter
 * should be some value no greater than the desired expiration time.
 *
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_poll(VSLHost* host, int64_t wait);

/**
 * Services a single client socket.  Note this does not accept new sockets for
 * that you must call @ref vsl_host_process().  The main advantage over calling
 * this function is to see if individual client servicing resulted in an error.
 *
 * @since 1.0
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_service(VSLHost* host, int sock);

/**
 * Process the host tasks by first expiring old frames and then servicing the
 * first available connection in our pool.  This function should be called in a
 * loop, generally blocked by @ref vsl_host_poll.
 *
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_process(VSLHost* host);

/**
 * Request a copy of the sockets managed by the host.  There will always be at
 * least one socket which is the connection socket which accepts new
 * connections.  Up to n_sockets socket descriptors will be copied into the
 * sockets buffer, if n_sockets is fewer than the number of available sockets
 * errno will be set to ENOBUFS. The n_socket parameter, if provided, will be
 * populated with a value of n_clients+1 which can be used to query required
 * space for the sockets buffer.  It is suggested to provide a buffer which is
 * larger than max_sockets to avoid race conditions where the number of sockets
 * changes between calls to this function.
 *
 * Note that the array of sockets should be refreshed often as once the function
 * returns they may be stale.  The API is implemented in such as way as to allow
 * thread-safe operations where one thread may-be using the vsl sockets to send
 * messages while another is polling for a read.
 *
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_sockets(VSLHost* host,
                 size_t   n_sockets,
                 int*     sockets,
                 size_t*  max_sockets);

/**
 * Registers the frame with the host and publishes it to subscribers.
 *
 * @note A frame posted to this function transfers ownership to the host and
 * should not have @ref vsl_frame_release called on it.  This will be managed
 * by the host on frame expiry.
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_host_post(VSLHost*  host,
              VSLFrame* frame,
              int64_t   expires,
              int64_t   duration,
              int64_t   pts,
              int64_t   dts);

/**
 * Drops the frame from the host.  This is meant to be called from the frame
 * but can also be used to remove the host association of the frame and return
 * ownership to the caller.
 *
 * @since 1.3
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_host_drop(VSLHost* host, VSLFrame* frame);

/**
 * Creates a client and connects to the host at the provided path.  If the
 * connection cannot be made NULL is returned and errno is set.
 *
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
VSLClient*
vsl_client_init(const char* path, void* userptr, bool reconnect);

/**
 * Releases the client, disconnecting from the host and releasing allocated
 * memory.
 *
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_client_release(VSLClient* client);

/**
 * Disconnects from the VSLHost and stops all reconnection attempts.  This
 * should be called as part of closing down a VSL client session.  It is
 * thread-safe unlike vsl_client_release which disposes of the client object.
 *
 * @memberof VSLClient
 * @since 1.1
 */
VSL_AVAILABLE_SINCE_1_1
VSL_API
void
vsl_client_disconnect(VSLClient* client);

/**
 * Returns the optional userptr associated with this client connection.
 *
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void*
vsl_client_userptr(VSLClient* client);

/**
 * Returns the path on which the client has connected to the host.
 *
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
const char*
vsl_client_path(const VSLClient* client);

/**
 * Sets the socket timeout for this client.
 *
 * @memberof VSLClient
 */

VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_client_set_timeout(VSLClient* client, float timeout);

/**
 * Creates and posts the video frame along with optional user pointer to any
 * arbitrary data.  Typically it would be used for holding a reference to
 * the host's view of the frame handle.
 *
 * @deprecated The vsl_frame_register function is deprecated in favour of using
 * the @ref vsl_frame_init(), @ref vsl_frame_alloc() or @ref vsl_frame_attach(),
 * and @ref vsl_host_post() functions which separate frame creation from posting
 * to the host for publishing to subscribers.
 *
 * @note A frame created through this function is owned by the host and should
 * not have @ref vsl_frame_release called on it.  This will be managed by the
 * host on frame expiry.
 *
 * @memberof VSLFrame
 */
VSL_DEPRECATED_SINCE_1_3
VSL_AVAILABLE_SINCE_1_0
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
                   void*             userptr);

/**
 * Initializes a VSLFrame without underlying frame buffer.  To create the
 * backing memory either call @ref vsl_frame_alloc() or to attach to an existing
 * bufer use @ref vsl_frame_attach().
 *
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
VSLFrame*
vsl_frame_init(uint32_t          width,
               uint32_t          height,
               uint32_t          stride,
               uint32_t          fourcc,
               void*             userptr,
               vsl_frame_cleanup cleanup);

/**
 * Allocates the underlying memory for the frame.  This function will prefer to
 * allocate using dmabuf and fallback to shared memory if dmabuf is not
 * available, unless the frame has a path defined in which case shared memory is
 * assumed.  If the path begins with /dev then it assumed to point to a
 * dmabuf-heap device.  If path is NULL then the allocator will first attempt to
 * create a dmabuf then fallback to shared memory.
 *
 * Allocations will be based on a buffer large enough to hold height*stride
 * bytes.  If using a compressed fourcc such as JPEG the actual data will be
 * smaller, this size can be captured when calling @ref vsl_frame_copy() as the
 * function returns the number of bytes copied into the target frame.  There is
 * currently no method to capture the actual compressed size when receiving an
 * already compressed frame.  This limitation is because the size varies from
 * frame to frame while the underlying buffer is of a fixed size.  When the
 * actual encoded size is important, the @ref vsl_frame_copy() should be called
 * directly or the reported size communicated to the client through a separate
 * channel.
 *
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_alloc(VSLFrame* frame, const char* path);

/**
 * Frees the allocated buffer for this frame.  Does not release the frame itself
 * for that use @ref vsl_frame_release().
 *
 * @param frame
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_frame_unalloc(VSLFrame* frame);

/**
 * Attach the provided file descriptor to the VSLFrame.  If size is not provided
 * it is assumed to be stride*height bytes.  If offset is provided then size
 * *MUST* be provided, the offset is in bytes to the start of the frame.
 *
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_attach(VSLFrame* frame, int fd, size_t size, size_t offset);

/**
 * Returns the path to the underlying VSLFrame buffer.  Note it will not always
 * be available, such as when the frame was externally created.  When no path is
 * available NULL is returned.
 *
 * @note This function is not thread-safe and you must use the string
 * immediately.
 *
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
const char*
vsl_frame_path(const VSLFrame* frame);

/**
 * Unregisters the frame, removing it from the host pool.
 *
 * @deprecated This function is deprecated in favour of calling
 * @ref vsl_frame_release() which will handle the required cleanup.
 *
 * @memberof VSLFrame
 */
VSL_DEPRECATED_SINCE_1_3_FOR(vsl_frame_release)
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_unregister(VSLFrame* frame);

/**
 * Copy the source frame into the target frame, with optional source crop. The
 * copy handles format conversion, rescaling to fit the target frame.  Resize
 * happens after the crop, if required.
 *
 * Copy can happen between any frames, regardless of whether they are parented
 * or not or have differing parents.  The copy happens through the underlying
 * buffers and will attempt to use available hardware accelerators.
 *
 * The function will attempt to lock target and source.  Since lock is a no-op
 * when not a client frame it is safe even for free-standing frames.  Copying to
 * or from a posted frame is safe but is likely to cause visual corruption such
 * as tearing.
 *
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_copy(VSLFrame* target, VSLFrame* source, const VSLRect* crop);

/**
 * Returns the user pointer associated with this frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void*
vsl_frame_userptr(VSLFrame* frame);

/**
 * Associate userptr with this frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_set_userptr(VSLFrame* frame, void* userptr);

/**
 * Waits for a frame to arrive and returns a new frame object.  Frames who's
 * timestamp is less than @param until will be ignored.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
VSLFrame*
vsl_frame_wait(VSLClient* client, int64_t until);

/**
 * Releases the frame, performing required cleanup.  If the frame was mapped it
 * will be unmapped.  If the frame was posted to a host it will be removed, if
 * this is a client frame it will be unlocked.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_release(VSLFrame* frame);

/**
 * Attempts to lock the video frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_trylock(VSLFrame* frame);

/**
 * Attempts to unlock the video frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_unlock(VSLFrame* frame);

/**
 * Returns the serial frame count of the video frame.
 *
 * Note this frame serial tracks the count of frames registered on the host and
 * does not necessarily equal the actual frame number from the camera.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_serial(const VSLFrame* frame);

/**
 * Returns the timestamp for this frame in nanoseconds.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_timestamp(const VSLFrame* frame);

/**
 * Returns the duration for this frame in nanoseconds.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_duration(const VSLFrame* frame);

/**
 * Returns the presentation timestamp for this frame in nanoseconds.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_pts(const VSLFrame* frame);

/**
 * Returns the decode timestamp for this frame in nanoseconds.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_dts(const VSLFrame* frame);

/**
 * Returns the epiration time for this frame in milliseconds.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_expires(const VSLFrame* frame);

/**
 * Returns the FOURCC code for the video frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
uint32_t
vsl_frame_fourcc(const VSLFrame* frame);

/**
 * Returns the width in pixels of the video frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_width(const VSLFrame* frame);

/**
 * Returns the height in pixels of the video frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_height(const VSLFrame* frame);

/**
 * Returns the stride in bytes of the video frame, to go from one row to the
 * next.
 *
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_stride(const VSLFrame* frame);

/**
 * Returns the size in bytes of the video frame.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_size(const VSLFrame* frame);

/**
 * Returns the file descriptor for this frame or -1 if none is associated.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_handle(const VSLFrame* frame);

/**
 * Returns the physical address of the frame.  If the frame does not support
 * DMA then MMAP_FAILED is returned.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
intptr_t
vsl_frame_paddr(const VSLFrame* frame);

/**
 * Maps the frame into the process' memory space, optionally also sets the
 * size of the frame if @param size is non-NULL.  Ensure the frame is
 * unmapped when no longer needed using @ref nn_frame_munmap().
 *
 * Note that a frame must be locked for the duration of the mapping.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void*
vsl_frame_mmap(VSLFrame* frame, size_t* size);

/**
 * Maps the frame into the process' memory space, optionally also sets the
 * size of the frame if @param size is non-NULL.
 *
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_munmap(VSLFrame* frame);

/**
 * Cache synchronization session control for when using DMA-backed buffers.
 * This happens automatically on mmap/munmap but the API is also available for
 * cases where the frame is updated in-place during a mapping.
 *
 * @param frame the frame object to synchronize
 * @param enable whether the sync session is being enabled or disabled
 * @param mode the synchronization mode controls READ, WRITE, or both.
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_sync(const VSLFrame* frame, int enable, int mode);

/**
 * Returns a fourcc integer code from the string.  If the fourcc code is invalid
 * or unsupported then 0 is returned.
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint32_t
vsl_fourcc_from_string(const char* fourcc);

/**
 * @brief Creates VSLEncoder instance
 *
 * @param profile VSLEncoderProfile determining encode quality
 * @param outputFourcc fourcc code defining the codec
 * @param fps output stream fps
 * @return VSLEncoder* new encoder instance
 *
 * Every encoder instance must be released using vsl_encoder_release
 *
 * For Hantro VC8000e encoder initialization is performed when vsl_encode_frame
 * is called for a first time
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
VSLEncoder*
vsl_encoder_create(VSLEncoderProfile profile, uint32_t outputFourcc, int fps);

/**
 * @brief Destroys VSLEncoder instance
 *
 * @param encoder VSLEncoder* instance to destroy
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_encoder_release(VSLEncoder* encoder);

/**
 * @brief Encode frame
 * @param encoder VSLEncoder instance
 * @param source VSLFrame source
 * @param destination VSLFrame destination
 * @param cropRegion (optional) VSLRect that defines the crop region, NULL when
 * destination and source sizes match
 * @param keyframe (optional) VSL sets this to 1 if the encoded frame is a
 * keyframe, otherwise 0. User can set to NULL to ignore param.
 * @retval 0 on success
 * @retval -1 on falure (check errno for details)
 *
 * For Hantro VC8000e encoder initialization is performed when this function is
 * called for a first time For Hantro VC8000e encoder source width, height and
 * fourcc; destination width, height and fourcc; cropRegion parameters must
 * match for all function calls throughout the lifetime of the encoder instance
 */

VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_encode_frame(VSLEncoder*    encoder,
                 VSLFrame*      source,
                 VSLFrame*      destination,
                 const VSLRect* cropRegion,
                 int*           keyframe);

VSL_AVAILABLE_SINCE_1_3
VSL_API
VSLFrame*
vsl_encoder_new_output_frame(const VSLEncoder* encoder,
                             int               width,
                             int               height,
                             int64_t           duration,
                             int64_t           pts,
                             int64_t           dts);

typedef struct vsl_camera_buffer vsl_camera_buffer;

typedef struct vsl_camera vsl_camera;

/**
 * Opens the camera device specified by the @param filename and allocates
 * device memory. If the device was not found or could not be recognized
 *
 * Return NULL if the device was not found or could not be recognized.
 * Otherwise returns a vsl_camera context which can be used in other vsl_camera
 * functions.
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
vsl_camera*
vsl_camera_open_device(const char* filename);

/**
 * Initialized the camera device in @param ctx for streaming
 * and allocate camera buffers.
 *
 * Then requests the camera to stream at the requested @param width
 * and @param height using the requested @param fourcc code.
 *
 * If @param width, @param height, or @param fourcc are 0, the respective value
 * use the default provided by the driver
 *
 * The @param width, @param height, @param fourcc parameters
 * will be set to the actual width and height and fourcc that
 * the camera driver sets the device to.
 *
 * Returns -1 if an error is encountered when initializing the camera to stream,
 * otherwise returns 0
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_init_device(vsl_camera* ctx,
                       int*        width,
                       int*        height,
                       int*        buf_count,
                       uint32_t*   fourcc);

/**
 * Requests the camera in @param ctx to mirror the image leftside right
 *
 * Returns -1 if a mirror was requested but the camera driver refused
 * the request, otherwise 0.
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_mirror(const vsl_camera* ctx, bool mirror);

/**
 * Requests the camera in @param ctx to mirror the image upside down
 *
 * Returns -1 if a mirror was requested but the camera driver refused
 * the request, otherwise 0.
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_mirror_v(const vsl_camera* ctx, bool mirror);

/**
 * Starts the camera stream.
 *
 * Must be called after @ref vsl_camera_init_device
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_start_capturing(vsl_camera* ctx);

/**
 * Attempts to read a frame from the camera.
 *
 * Must be called after @ref vsl_camera_start_capturing.
 *
 * Ensure to call @ref vsl_camera_release_buffer after the buffer is done being
 * used and allow the buffer to be reused for frame capture.
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
vsl_camera_buffer*
vsl_camera_get_data(vsl_camera* ctx);

/**
 * Enqueues a buffer to be reused for frame capture.
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_release_buffer(vsl_camera* ctx, const vsl_camera_buffer* buffer);

/**
 * Stops the camera stream.
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_stop_capturing(const vsl_camera* ctx);

/**
 * Uninitializes the camera buffers and frees the buffer memory
 *
 * Ensure that the device is not streaming. If
 * @ref vsl_camera_start_capturing was called, ensure that
 * @ref vsl_camera_stop_capturing is called before this function
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_camera_uninit_device(vsl_camera* ctx);

/**
 * Closes the camera device and frees the device memory
 *
 * Ensure that the device is not streaming. If
 * @ref vsl_camera_start_capturing was called, ensure that
 * @ref vsl_camera_stop_capturing is called before this function
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_camera_close_device(vsl_camera* ctx);

/**
 * Checks if dma buffers are supported on the camera
 *
 * Ensure that this is called after
 * @ref vsl_camera_init_device
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_is_dmabuf_supported(const vsl_camera* ctx);

/**
 * Returns the number of queued buffers for the camera.
 * @ref vsl_camera_get_data will timeout if there are 0 queued buffers.
 *
 * The user can send buffers back to the buffer queue using
 * @ref vsl_camera_release_buffer
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_get_queued_buf_count(const vsl_camera* ctx);

/**
 * Returns the mmap memory of the camera buffer
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void*
vsl_camera_buffer_mmap(vsl_camera_buffer* buffer);

/**
 * Returns the dmabuf file descriptor of the camera buffer
 *
 * If the device does not support dmabuf, returns -1
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_buffer_dma_fd(const vsl_camera_buffer* buffer);

/**
 * Returns the phys addr of the camera buffer
 *
 * If the device does not support physical address, returns 0
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint64_t
vsl_camera_buffer_phys_addr(const vsl_camera_buffer* buffer);

/**
 * Returns the length of the camera buffer in bytes
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint32_t
vsl_camera_buffer_length(const vsl_camera_buffer* buffer);

/**
 * Returns the fourcc code of the camera buffer
 *
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint32_t
vsl_camera_buffer_fourcc(const vsl_camera_buffer* buffer);

/**
 * Reads the timestamp of the camera buffer into @param seconds and @param
 * nanoseconds.  The seconds are relative to the monotonic time when the frame
 * was captured, nanoseconds are the sub-seconds in nanoseconds.
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_camera_buffer_timestamp(const vsl_camera_buffer* buffer,
                            int64_t*                 seconds,
                            int64_t*                 nanoseconds);

/**
 * Lists the supported single planar formats of
 * the camera into @param codes as fourcc codes
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_enum_fmts(const vsl_camera* ctx, uint32_t* codes, int size);

/**
 * Lists the supported multi planar formats of
 * the camera into @param codes as fourcc codes
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_enum_mplane_fmts(const vsl_camera* ctx, uint32_t* codes, int size);

VSL_AVAILABLE_SINCE_1_4
VSL_API
VSLDecoder*
vsl_decoder_create(uint32_t outputFourcc, int fps);

typedef enum {
    VSL_DEC_SUCCESS   = 0x0,
    VSL_DEC_ERR       = 0x1,
    VSL_DEC_INIT_INFO = 0x2,
    VSL_DEC_FRAME_DEC = 0x4,
} VSLDecoderRetCode;

VSL_AVAILABLE_SINCE_1_4
VSL_API
VSLDecoderRetCode
vsl_decode_frame(VSLDecoder*  decoder,
                 const void*  data,
                 unsigned int data_length,
                 size_t*      bytes_used,
                 VSLFrame**   output_frame);

VSL_AVAILABLE_SINCE_1_4
VSL_API
int
vsl_decoder_width(const VSLDecoder*  decoder);

VSL_AVAILABLE_SINCE_1_4
VSL_API
int
vsl_decoder_height(const VSLDecoder* decoder);

VSL_AVAILABLE_SINCE_1_4
VSL_API
VSLRect
vsl_decoder_crop(const VSLDecoder* decoder);

VSL_AVAILABLE_SINCE_1_4
VSL_API
int
vsl_decoder_release(VSLDecoder* decoder);

#ifdef __cplusplus
}
#endif

#endif /* VIDEOSTREAM_H */
