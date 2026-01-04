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
#define VSL_VERSION "2.1.0"

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
#define VSL_VERSION_2_0 VSL_VERSION_ENCODE(2, 0, 0)

#ifndef VSL_TARGET_VERSION
#define VSL_TARGET_VERSION VSL_VERSION_2_0
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

#if VSL_TARGET_VERSION < VSL_VERSION_ENCODE(2, 0, 0)
#define VSL_AVAILABLE_SINCE_2_0 VSL_UNAVAILABLE(2.0)
#define VSL_DEPRECATED_SINCE_2_0
#define VSL_DEPRECATED_SINCE_2_0_FOR(replacement)
#else
#define VSL_AVAILABLE_SINCE_2_0
#define VSL_DEPRECATED_SINCE_2_0 VSL_DEPRECATED(2.0)
#define VSL_DEPRECATED_SINCE_2_0_FOR(replacement) \
    VSL_DEPRECATED_FOR(2.0, replacement)
#endif

#define VSL_FOURCC(a, b, c, d)                                         \
    ((uint32_t) (a) | ((uint32_t) (b) << 8) | ((uint32_t) (c) << 16) | \
     ((uint32_t) (d) << 24))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct VSLHost
 * @brief The VSLHost object manages a connection point at the user-defined path
 * and allows frames to be registered for client use.
 */
typedef struct vsl_host VSLHost;

/**
 * @struct VSLClient
 * @brief The VSLClient object manages a single connection to a VSLHost.
 */
typedef struct vsl_client VSLClient;

/**
 * @struct VSLFrame
 * @brief The VSLFrame object represents a single video frame from either the
 * host or client perspective.  Certain API are only available to the host or
 * client.
 */
typedef struct vsl_frame VSLFrame;

/**
 * @struct VSLEncoder
 * @brief The VSLEncoder object represents encoder instance.
 *
 */
typedef struct vsl_encoder VSLEncoder;

/**
 * @struct VSLDecoder
 * @brief The VSLDecoder object represents decoder instance.
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

/**
 * Encoder profile defining target bitrate for video encoding.
 *
 * These profiles provide a convenient way to specify encoding quality/bitrate
 * tradeoffs. Higher bitrates produce better quality at the cost of larger file
 * sizes and potentially higher CPU/power consumption.
 *
 * @note The actual quality-to-bitrate ratio depends on the codec (H.264 vs
 * H.265), encoder implementation, and content complexity.
 */
typedef enum vsl_encode_profile {
    /**
     * Automatic bitrate selection (encoder default).
     *
     * Platform and version dependent. Testing shows approximately 10000 kbps
     * on i.MX8M Plus. Use this for general-purpose encoding when specific
     * bitrate control is not required.
     */
    VSL_ENCODE_PROFILE_AUTO,

    /**
     * 5 Mbps target bitrate.
     *
     * Suitable for moderate quality 1080p video or high quality 720p.
     */
    VSL_ENCODE_PROFILE_5000_KBPS,

    /**
     * 25 Mbps target bitrate.
     *
     * Suitable for high quality 1080p video or moderate quality 4K.
     */
    VSL_ENCODE_PROFILE_25000_KBPS,

    /**
     * 50 Mbps target bitrate.
     *
     * Suitable for very high quality 1080p or high quality 4K video.
     */
    VSL_ENCODE_PROFILE_50000_KBPS,

    /**
     * 100 Mbps target bitrate.
     *
     * Suitable for maximum quality 4K video or when preserving fine details
     * is critical. May stress storage and network bandwidth.
     */
    VSL_ENCODE_PROFILE_100000_KBPS,
} VSLEncoderProfile;

/**
 * Video codec type for hardware decoder.
 *
 * Specifies which video compression standard to use for decoding.
 * Both codecs are supported via Hantro VPU hardware acceleration on i.MX8.
 */
typedef enum {
    /**
     * H.264/AVC (Advanced Video Coding) codec.
     *
     * Widely supported standard (ISO/IEC 14496-10, ITU-T H.264) with good
     * compression and compatibility. Recommended for maximum device
     * compatibility.
     */
    VSL_DEC_H264,

    /**
     * H.265/HEVC (High Efficiency Video Coding) codec.
     *
     * Next-generation standard (ISO/IEC 23008-2, ITU-T H.265) providing
     * approximately 50% better compression than H.264 at equivalent quality.
     * Recommended when bandwidth/storage are constrained and decoder support
     * is confirmed.
     */
    VSL_DEC_HEVC,
} VSLDecoderCodec;

/**
 * Codec backend selection for encoder/decoder.
 *
 * Allows selection between V4L2 kernel driver and Hantro user-space
 * library (libcodec.so) backends. Use with vsl_decoder_create_ex() and
 * vsl_encoder_create_ex() for explicit backend control.
 *
 * The VSL_CODEC_BACKEND environment variable can override the AUTO selection:
 * - "hantro" - Force Hantro backend even if V4L2 available
 * - "v4l2"   - Force V4L2 backend (fail if unavailable)
 * - "auto"   - Auto-detect (default)
 *
 * @since 2.0
 */
typedef enum {
    /**
     * Auto-detect best available backend.
     *
     * Selection priority:
     * 1. Check VSL_CODEC_BACKEND environment variable
     * 2. Prefer V4L2 if device available and has M2M capability
     * 3. Fall back to Hantro if V4L2 unavailable
     * 4. Fail if no backend available
     */
    VSL_CODEC_BACKEND_AUTO = 0,

    /**
     * Force Hantro/libcodec.so backend.
     *
     * Uses the proprietary VPU wrapper library. May be needed for:
     * - Systems without V4L2 codec driver
     * - Testing/debugging Hantro-specific behavior
     * - Compatibility with older configurations
     */
    VSL_CODEC_BACKEND_HANTRO = 1,

    /**
     * Force V4L2 kernel driver backend.
     *
     * Uses the vsi_v4l2 mem2mem driver. Provides:
     * - 37-56x faster decode performance
     * - Stable encoder (vs crashing libcodec.so)
     * - Standard Linux API
     */
    VSL_CODEC_BACKEND_V4L2 = 2,
} VSLCodecBackend;

/**
 * Function pointer definition which will be called as part of
 * @ref vsl_frame_unregister.  This is typically used to free resources
 * associated with the frame on either client or host side.
 */
typedef void (*vsl_frame_cleanup)(VSLFrame* frame);

/**
 * Returns the VideoStream Library version string.
 *
 * @return Version string in "MAJOR.MINOR.PATCH" format (e.g., "1.5.4")
 * @since 1.0
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
const char*
vsl_version();

/**
 * Returns the current timestamp in nanoseconds.
 *
 * Uses monotonic clock (CLOCK_MONOTONIC) for consistent timing across the
 * system. This timestamp is used for frame timing and synchronization.
 *
 * @return Current time in nanoseconds since an unspecified starting point
 * @since 1.0
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_timestamp();

/**
 * Creates a host on the requested path for inter-process frame sharing.
 *
 * The host manages a UNIX domain socket at the specified path and accepts
 * connections from clients. Frames posted to the host are broadcast to all
 * connected clients.
 *
 * @param path UNIX socket path (filesystem or abstract). If path starts with
 *             '/' it creates a filesystem socket, otherwise an abstract socket.
 * @return Pointer to VSLHost object on success, NULL on failure (sets errno)
 * @since 1.0
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
 * Closes the UNIX socket, disconnects all clients, and frees all resources
 * associated with the host. Any posted frames are released.
 *
 * @param host The host to release
 * @since 1.0
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_host_release(VSLHost* host);

/**
 * Returns the bound path of the host.
 *
 * @param host The host instance
 * @return UNIX socket path string (owned by host, do not free)
 * @since 1.0
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
const char*
vsl_host_path(const VSLHost* host);

/**
 * Polls the list of available connections for activity.
 *
 * Waits for socket activity (new connections or client messages) using poll().
 * Should be called in a loop before vsl_host_process(). Note frames are only
 * expired by vsl_host_process(), so the wait parameter should be no greater
 * than the desired frame expiration time to ensure timely cleanup.
 *
 * @param host The host instance
 * @param wait Timeout in milliseconds. If >0, poll waits up to this duration.
 *             If 0, returns immediately. If <0, waits indefinitely.
 * @return Number of sockets with activity, 0 on timeout, -1 on error (sets
 * errno)
 * @since 1.0
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_poll(VSLHost* host, int64_t wait);

/**
 * Services a single client socket.
 *
 * Processes messages from a specific client socket. Does not accept new
 * connections - use vsl_host_process() for that. Useful when you need to
 * track errors for individual clients.
 *
 * @param host The host instance
 * @param sock The client socket file descriptor to service
 * @return 0 on success, -1 on error (sets errno, EPIPE if client disconnected)
 * @since 1.0
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_service(VSLHost* host, int sock);

/**
 * Process host tasks: expire old frames and service one client connection.
 *
 * First expires frames past their lifetime, then services the first available
 * connection (accepting new clients or processing client messages). Should be
 * called in a loop, typically after vsl_host_poll() indicates activity.
 *
 * @param host The host instance
 * @return 0 on success, -1 on error (sets errno, ETIMEDOUT if no activity)
 * @since 1.0
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_host_process(VSLHost* host);

/**
 * Request a copy of the sockets managed by the host.
 *
 * Returns socket file descriptors for the host's listening socket and all
 * connected client sockets. The first socket is always the listening socket.
 * The array should be refreshed frequently as sockets may become stale.
 *
 * Thread-safe: allows one thread to use sockets for messaging while another
 * polls for reads.
 *
 * @param host The host instance
 * @param n_sockets Size of the sockets array buffer
 * @param sockets Buffer to receive socket file descriptors
 * @param max_sockets If provided, populated with n_clients+1 (total sockets)
 * @return 0 on success, -1 on error (sets errno to ENOBUFS if buffer too small)
 * @since 1.0
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
 * Transfers ownership of the frame to the host. The frame is broadcast to all
 * connected clients and will be automatically released when it expires. Do not
 * call vsl_frame_release() on frames posted to the host.
 *
 * @param host The host instance
 * @param frame Frame to post (ownership transferred to host)
 * @param expires Expiration time in nanoseconds (absolute, from
 * vsl_timestamp())
 * @param duration Frame duration in nanoseconds (-1 if unknown)
 * @param pts Presentation timestamp in nanoseconds (-1 if unknown)
 * @param dts Decode timestamp in nanoseconds (-1 if unknown)
 * @return 0 on success, -1 on error (sets errno)
 * @since 1.3
 * @memberof VSLHost
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
 * Drops the frame from the host.
 *
 * Removes the host association of the frame and returns ownership to the
 * caller. Can be used to cancel a previously posted frame before it expires.
 *
 * @param host The host instance
 * @param frame Frame to drop from host
 * @return 0 on success, -1 on error (sets errno)
 * @since 1.3
 * @memberof VSLHost
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_host_drop(VSLHost* host, VSLFrame* frame);

/**
 * Creates a client and connects to the host at the provided path.
 *
 * Establishes a connection to a VSLHost via UNIX domain socket. The client
 * can receive frames broadcast by the host.
 *
 * @param path UNIX socket path matching the host's path
 * @param userptr Optional user data pointer (retrievable via
 * vsl_client_userptr)
 * @param reconnect If true, automatically reconnect if connection is lost
 * @return Pointer to VSLClient object on success, NULL on failure (sets errno)
 * @since 1.0
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
 * Closes the socket connection, frees all resources, and invalidates any
 * pending frames. Not thread-safe - use vsl_client_disconnect() for
 * thread-safe disconnection before calling this.
 *
 * @param client The client to release
 * @since 1.0
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_client_release(VSLClient* client);

/**
 * Disconnects from the VSLHost and stops all reconnection attempts.
 *
 * Thread-safe disconnect operation. Should be called before
 * vsl_client_release() when shutting down a client session from a different
 * thread.
 *
 * @param client The client to disconnect
 * @since 1.1
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_1
VSL_API
void
vsl_client_disconnect(VSLClient* client);

/**
 * Returns the optional userptr associated with this client connection.
 *
 * @param client The client instance
 * @return User pointer provided to vsl_client_init(), or NULL if none
 * @since 1.0
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void*
vsl_client_userptr(VSLClient* client);

/**
 * Returns the path on which the client has connected to the host.
 *
 * @param client The client instance
 * @return UNIX socket path string (owned by client, do not free)
 * @since 1.0
 * @memberof VSLClient
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
const char*
vsl_client_path(const VSLClient* client);

/**
 * Sets the socket timeout for this client.
 *
 * Configures how long socket operations wait before timing out. Affects
 * recv() calls when waiting for frames from the host.
 *
 * @param client The client instance
 * @param timeout Timeout in seconds (e.g., 1.0 for 1 second)
 * @since 1.0
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
 * Initializes a VSLFrame without underlying frame buffer.
 *
 * Creates a frame descriptor with specified dimensions and format. To allocate
 * backing memory, call vsl_frame_alloc(). To attach existing memory (e.g.,
 * DmaBuf from camera), call vsl_frame_attach().
 *
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param stride Row stride in bytes (0 to auto-calculate from width)
 * @param fourcc Pixel format as FOURCC code (e.g., VSL_FOURCC('N','V','1','2'))
 * @param userptr Optional user data pointer
 * @param cleanup Optional cleanup callback invoked on vsl_frame_release()
 * @return Pointer to VSLFrame object, or NULL on failure
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
 * Allocates the underlying memory for the frame.
 *
 * Prefers DmaBuf allocation for zero-copy, falling back to POSIX shared memory
 * if DmaBuf unavailable. If path is provided, it determines allocation type:
 * - NULL: Try DmaBuf first, fallback to shared memory
 * - Starts with "/dev": Use DmaBuf heap device at this path
 * - Other paths: Use shared memory at this path
 *
 * Allocates height*stride bytes. For compressed formats (JPEG, H.264), the
 * actual data size may be smaller. Use vsl_frame_copy() return value to get
 * actual compressed size.
 *
 * @param frame Frame to allocate memory for
 * @param path Optional allocation path (NULL for auto, /dev/... for DmaBuf
 * heap)
 * @return 0 on success, -1 on failure (sets errno)
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_alloc(VSLFrame* frame, const char* path);

/**
 * Frees the allocated buffer for this frame.
 *
 * Releases the underlying memory (DmaBuf or shared memory) but does not
 * destroy the frame object. Use vsl_frame_release() to destroy the frame.
 *
 * @param frame Frame whose buffer should be freed
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_frame_unalloc(VSLFrame* frame);

/**
 * Attach the provided file descriptor to the VSLFrame.
 *
 * Associates an existing buffer (typically DmaBuf from camera or hardware
 * accelerator) with the frame. The frame does not take ownership of the FD.
 *
 * @param frame Frame to attach buffer to
 * @param fd File descriptor (typically DmaBuf) to attach
 * @param size Buffer size in bytes (0 to use stride*height)
 * @param offset Byte offset to frame data start (must provide size if offset>0)
 * @return 0 on success, -1 on failure (sets errno)
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_attach(VSLFrame* frame, int fd, size_t size, size_t offset);

/**
 * Returns the path to the underlying VSLFrame buffer.
 *
 * Returns the filesystem path for shared memory buffers or DmaBuf heap devices.
 * Not available for externally created DmaBufs (e.g., from camera driver).
 *
 * @warning Not thread-safe. Use the returned string immediately.
 *
 * @param frame The frame instance
 * @return Buffer path string (owned by frame), or NULL if unavailable
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
 * @deprecated Use vsl_frame_release() instead, which handles cleanup properly.
 *
 * @param frame Frame to unregister
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_DEPRECATED_SINCE_1_3_FOR(vsl_frame_release)
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_unregister(VSLFrame* frame);

/**
 * Copy the source frame into the target frame, with optional source crop.
 *
 * Handles format conversion, rescaling, and cropping using hardware
 * acceleration when available (G2D on i.MX8). Both frames can be host or client
 * frames. Automatically locks frames during copy (safe for free-standing frames
 * too).
 *
 * Copy sequence: 1) Crop source, 2) Convert format, 3) Scale to target size.
 *
 * @warning Copying to/from a posted frame may cause visual tearing.
 *
 * @param target Destination frame (receives copied data)
 * @param source Source frame to copy from
 * @param crop Optional crop region in source coordinates (NULL for full frame)
 * @return Number of bytes copied on success, -1 on failure (sets errno)
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
 * @param frame The frame instance
 * @return User pointer provided to vsl_frame_init(), or NULL if none
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void*
vsl_frame_userptr(VSLFrame* frame);

/**
 * Associate userptr with this frame.
 *
 * Sets or updates the user data pointer for this frame.
 *
 * @param frame The frame instance
 * @param userptr User data pointer to associate with frame
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_set_userptr(VSLFrame* frame, void* userptr);

/**
 * Waits for a frame to arrive and returns a new frame object.
 *
 * Blocks until the host broadcasts a new frame. Frames with timestamp less
 * than 'until' are ignored (useful for skipping old frames after a pause).
 *
 * Caller must lock the frame (vsl_frame_trylock) before accessing data,
 * then unlock and release when done.
 *
 * @param client The client instance
 * @param until Minimum timestamp in nanoseconds (0 to accept next frame)
 * @return Pointer to VSLFrame object, or NULL on error (sets errno)
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
VSLFrame*
vsl_frame_wait(VSLClient* client, int64_t until);

/**
 * Releases the frame, performing required cleanup.
 *
 * Unmaps memory if mapped, unlocks if locked. If frame was posted to a host,
 * removes it. If client frame, decrements reference count. Invokes cleanup
 * callback if registered.
 *
 * @param frame Frame to release
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_release(VSLFrame* frame);

/**
 * Attempts to lock the video frame.
 *
 * Locks the frame for exclusive access (prevents host from releasing it).
 * Must be called before accessing frame data from a client. Always succeeds
 * for host-owned frames.
 *
 * @param frame Frame to lock
 * @return 0 on success, -1 on failure (frame expired or already unlocked)
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_trylock(VSLFrame* frame);

/**
 * Attempts to unlock the video frame.
 *
 * Releases the lock acquired by vsl_frame_trylock(), allowing the host to
 * release the frame when it expires.
 *
 * @param frame Frame to unlock
 * @return 0 on success, -1 on failure (sets errno)
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_unlock(VSLFrame* frame);

/**
 * Returns the serial frame count of the video frame.
 *
 * Frame serial is a monotonically increasing counter assigned by the host
 * when frames are registered. Does not necessarily equal camera frame number.
 *
 * @param frame The frame instance
 * @return Frame serial number (starts at 1)
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_serial(const VSLFrame* frame);

/**
 * Returns the timestamp for this frame in nanoseconds.
 *
 * Timestamp from vsl_timestamp() when frame was registered with host.
 * Uses monotonic clock for consistent timing.
 *
 * @param frame The frame instance
 * @return Frame timestamp in nanoseconds
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_timestamp(const VSLFrame* frame);

/**
 * Returns the duration for this frame in nanoseconds.
 *
 * Frame duration indicates how long this frame should be displayed.
 * May be -1 if unknown or not applicable.
 *
 * @param frame The frame instance
 * @return Frame duration in nanoseconds, or -1 if unknown
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_duration(const VSLFrame* frame);

/**
 * Returns the presentation timestamp for this frame in nanoseconds.
 *
 * PTS indicates when this frame should be presented/displayed in a stream.
 * May be -1 if unknown or not applicable.
 *
 * @param frame The frame instance
 * @return Presentation timestamp in nanoseconds, or -1 if unknown
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_pts(const VSLFrame* frame);

/**
 * Returns the decode timestamp for this frame in nanoseconds.
 *
 * DTS indicates when this frame should be decoded in a stream (important for
 * B-frames in video codecs). May be -1 if unknown or not applicable.
 *
 * @param frame The frame instance
 * @return Decode timestamp in nanoseconds, or -1 if unknown
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_dts(const VSLFrame* frame);

/**
 * Returns the expiration time for this frame in nanoseconds.
 *
 * Absolute timestamp (from vsl_timestamp()) when this frame will be expired
 * by the host. Clients should lock frames before this time.
 *
 * @param frame The frame instance
 * @return Expiration timestamp in nanoseconds
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int64_t
vsl_frame_expires(const VSLFrame* frame);

/**
 * Returns the FOURCC code for the video frame.
 *
 * FOURCC identifies the pixel format (e.g., NV12, YUY2, JPEG, H264).
 * Use VSL_FOURCC() macro to create fourcc codes.
 *
 * @param frame The frame instance
 * @return FOURCC code as uint32_t
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
uint32_t
vsl_frame_fourcc(const VSLFrame* frame);

/**
 * Returns the width in pixels of the video frame.
 *
 * @param frame The frame instance
 * @return Frame width in pixels
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_width(const VSLFrame* frame);

/**
 * Returns the height in pixels of the video frame.
 *
 * @param frame The frame instance
 * @return Frame height in pixels
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_height(const VSLFrame* frame);

/**
 * Returns the stride in bytes of the video frame.
 *
 * Stride is the number of bytes from the start of one row to the next.
 * May be larger than width*bytes_per_pixel due to alignment requirements.
 *
 * @param frame The frame instance
 * @return Row stride in bytes
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_stride(const VSLFrame* frame);

/**
 * Returns the size in bytes of the video frame buffer.
 *
 * For uncompressed formats, this is stride*height. For compressed formats
 * (JPEG, H.264), this is the maximum buffer size, not the actual data size.
 *
 * @param frame The frame instance
 * @return Buffer size in bytes
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_size(const VSLFrame* frame);

/**
 * Returns the file descriptor for this frame.
 *
 * Returns the DmaBuf or shared memory file descriptor used for zero-copy
 * sharing. Returns -1 if no file descriptor is associated (e.g., CPU-only
 * memory).
 *
 * @param frame The frame instance
 * @return File descriptor, or -1 if none
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
int
vsl_frame_handle(const VSLFrame* frame);

/**
 * Returns the physical address of the frame.
 *
 * Physical address is available for DMA-capable buffers on platforms where
 * the kernel provides physical address translation (some i.MX platforms).
 * Note: This function caches the physical address internally on first call.
 *
 * @param frame The frame instance
 * @return Physical address, or MMAP_FAILED ((intptr_t)-1) if DMA not supported
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
intptr_t
vsl_frame_paddr(VSLFrame* frame);

/**
 * Maps the frame into the process memory space.
 *
 * Creates a memory mapping for CPU access to frame data. Frame must be locked
 * (vsl_frame_trylock) for the duration of the mapping. Call vsl_frame_munmap()
 * when done.
 *
 * @param frame The frame instance
 * @param size Optional pointer to receive mapped size in bytes (may be NULL)
 * @return Pointer to mapped memory, or NULL on failure
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void*
vsl_frame_mmap(VSLFrame* frame, size_t* size);

/**
 * Unmaps the frame from the process memory space.
 *
 * Releases the memory mapping created by vsl_frame_mmap(). Should be called
 * when done accessing frame data.
 *
 * @param frame The frame instance to unmap
 * @since 1.0
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_0
VSL_API
void
vsl_frame_munmap(VSLFrame* frame);

/**
 * Cache synchronization session control for DMA-backed buffers.
 *
 * Controls CPU cache coherency for DMA buffers. Automatically called by
 * mmap/munmap, but can be used manually for in-place frame updates.
 *
 * Call with enable=1 before accessing, enable=0 after modifying.
 * Mode: DMA_BUF_SYNC_READ (CPU reads), DMA_BUF_SYNC_WRITE (CPU writes),
 *       or DMA_BUF_SYNC_RW (both).
 *
 * @param frame The frame object to synchronize
 * @param enable 1 to start sync session, 0 to end it
 * @param mode Sync mode: DMA_BUF_SYNC_READ, DMA_BUF_SYNC_WRITE, or
 * DMA_BUF_SYNC_RW
 * @return 0 on success, -1 on failure (sets errno)
 * @since 1.3
 * @memberof VSLFrame
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_frame_sync(const VSLFrame* frame, int enable, int mode);

/**
 * Returns a fourcc integer code from the string.
 *
 * Converts a 4-character string to FOURCC code. Example: "NV12" ->
 * VSL_FOURCC('N','V','1','2').
 *
 * @param fourcc String containing exactly 4 characters (e.g., "NV12", "YUY2")
 * @return FOURCC code as uint32_t, or 0 if invalid/unsupported
 * @since 1.3
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
 * @brief Creates VSLEncoder instance with explicit backend selection
 *
 * Extended version of vsl_encoder_create() that allows selecting a specific
 * codec backend. Use this when you need to force V4L2 or Hantro backend.
 *
 * @param profile VSLEncoderProfile determining encode quality
 * @param outputFourcc fourcc code defining the codec (H264 or HEVC)
 * @param fps output stream fps
 * @param backend Which backend to use (VSL_CODEC_BACKEND_AUTO, _V4L2, _HANTRO)
 * @return VSLEncoder* new encoder instance, or NULL if backend unavailable
 *
 * @since 2.0
 */
VSL_AVAILABLE_SINCE_2_0
VSL_API
VSLEncoder*
vsl_encoder_create_ex(VSLEncoderProfile profile,
                      uint32_t          outputFourcc,
                      int               fps,
                      VSLCodecBackend   backend);

/**
 * @brief Destroys VSLEncoder instance
 *
 * Frees all resources associated with the encoder, including hardware
 * resources. Do not use the encoder after calling this function.
 *
 * @param encoder VSLEncoder instance to destroy
 * @since 1.3
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_encoder_release(VSLEncoder* encoder);

/**
 * @brief Encode frame
 *
 * Encodes the source frame into the destination frame using hardware
 * acceleration (Hantro VPU on i.MX8). First call initializes the encoder with
 * the given parameters. Subsequent calls must use identical source/destination
 * dimensions, formats, and crop region.
 *
 * @param encoder VSLEncoder instance
 * @param source Source frame (raw video data)
 * @param destination Pre-allocated destination frame (receives encoded data)
 * @param cropRegion Optional crop region in source coordinates (NULL for no
 * crop)
 * @param keyframe Optional output: set to 1 if encoded frame is IDR/keyframe,
 *                 0 otherwise. Pass NULL to ignore.
 * @retval 0 on success
 * @retval -1 on failure (check errno for details)
 * @since 1.3
 */

VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_encode_frame(VSLEncoder*    encoder,
                 VSLFrame*      source,
                 VSLFrame*      destination,
                 const VSLRect* cropRegion,
                 int*           keyframe);

/**
 * @brief Creates a new output frame for encoder
 *
 * Allocates a frame suitable for receiving encoded output from
 * vsl_encode_frame(). The frame uses encoder-specific memory (e.g., Hantro VPU
 * EWL memory on i.MX8) for efficient hardware encoding.
 *
 * @param encoder VSLEncoder instance
 * @param width Encoded frame width in pixels (should match encoder source)
 * @param height Encoded frame height in pixels (should match encoder source)
 * @param duration Frame duration in nanoseconds (passed through to output)
 * @param pts Presentation timestamp in nanoseconds (passed through to output)
 * @param dts Decode timestamp in nanoseconds (passed through to output)
 * @return Pointer to VSLFrame for encoded output, or NULL on failure
 * @since 1.3
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
VSLFrame*
vsl_encoder_new_output_frame(const VSLEncoder* encoder,
                             int               width,
                             int               height,
                             int64_t           duration,
                             int64_t           pts,
                             int64_t           dts);

/**
 * @struct vsl_camera_buffer
 * @brief Opaque structure representing a V4L2 camera buffer.
 */
typedef struct vsl_camera_buffer vsl_camera_buffer;

/**
 * @struct VSLCamera
 * @brief Opaque structure representing a V4L2 camera device.
 */
typedef struct vsl_camera vsl_camera;
typedef struct vsl_camera VSLCamera;

/**
 * Opens the camera device specified by filename and allocates device memory.
 *
 * Opens a V4L2 video capture device (e.g., /dev/video0) and prepares it for
 * streaming. The device is not yet configured - call vsl_camera_init_device()
 * next.
 *
 * @param filename V4L2 device path (e.g., "/dev/video0")
 * @return Pointer to vsl_camera context on success, NULL on failure
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
vsl_camera*
vsl_camera_open_device(const char* filename);

/**
 * Initializes the camera device for streaming and allocates camera buffers.
 *
 * Negotiates format with the V4L2 driver. On entry, width/height/fourcc contain
 * desired values (0 for driver default). On success, they're updated with
 * actual negotiated values. Allocates buf_count buffers (updated with actual
 * count).
 *
 * Must be called after vsl_camera_open_device() and before
 * vsl_camera_start_capturing().
 *
 * @param ctx Camera context from vsl_camera_open_device()
 * @param width Pointer to desired/actual width in pixels (0 for default)
 * @param height Pointer to desired/actual height in pixels (0 for default)
 * @param buf_count Pointer to desired/actual buffer count (0 for default)
 * @param fourcc Pointer to desired/actual fourcc format (0 for default)
 * @return 0 on success, -1 on error
 * @since 1.3
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
 * Requests the camera to mirror the image left-to-right.
 *
 * Uses V4L2_CID_HFLIP control to flip the image horizontally.
 * Not all cameras support this feature.
 *
 * @param ctx Camera context
 * @param mirror true to enable horizontal flip, false to disable
 * @return 0 on success, -1 if driver refused the request
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_mirror(const vsl_camera* ctx, bool mirror);

/**
 * Requests the camera to mirror the image top-to-bottom.
 *
 * Uses V4L2_CID_VFLIP control to flip the image vertically.
 * Not all cameras support this feature.
 *
 * @param ctx Camera context
 * @param mirror true to enable vertical flip, false to disable
 * @return 0 on success, -1 if driver refused the request
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_mirror_v(const vsl_camera* ctx, bool mirror);

/**
 * Starts the camera stream.
 *
 * Begins V4L2 streaming (VIDIOC_STREAMON). Frames can now be captured with
 * vsl_camera_get_data(). Must be called after vsl_camera_init_device().
 *
 * @param ctx Camera context
 * @return 0 on success, -1 on error
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_start_capturing(vsl_camera* ctx);

/**
 * Attempts to read a frame from the camera.
 *
 * Dequeues a filled buffer from the camera driver (VIDIOC_DQBUF). Blocks until
 * a frame is available. Must be called after vsl_camera_start_capturing().
 *
 * After processing, call vsl_camera_release_buffer() to return the buffer to
 * the driver's queue for reuse.
 *
 * @param ctx Camera context
 * @return Pointer to camera buffer, or NULL on timeout/error
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
vsl_camera_buffer*
vsl_camera_get_data(vsl_camera* ctx);

/**
 * Enqueues a buffer to be reused for frame capture.
 *
 * Returns the buffer to the camera driver's queue (VIDIOC_QBUF) so it can be
 * filled with new frame data. Must be called after processing each buffer from
 * vsl_camera_get_data().
 *
 * @param ctx Camera context
 * @param buffer Buffer to release (from vsl_camera_get_data)
 * @return 0 on success, -1 on error
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_release_buffer(vsl_camera* ctx, const vsl_camera_buffer* buffer);

/**
 * Stops the camera stream.
 *
 * Stops V4L2 streaming (VIDIOC_STREAMOFF). No more frames will be captured.
 * Call before vsl_camera_uninit_device() and vsl_camera_close_device().
 *
 * @param ctx Camera context
 * @return 0 on success, -1 on error
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_stop_capturing(const vsl_camera* ctx);

/**
 * Uninitializes the camera buffers and frees the buffer memory.
 *
 * Releases all allocated camera buffers. Ensure the device is not streaming
 * (call vsl_camera_stop_capturing() first).
 *
 * @param ctx Camera context
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_camera_uninit_device(vsl_camera* ctx);

/**
 * Closes the camera device and frees the device memory.
 *
 * Closes the V4L2 device file descriptor and releases all resources.
 * Ensure the device is not streaming (call vsl_camera_stop_capturing() and
 * vsl_camera_uninit_device() first).
 *
 * @param ctx Camera context to close
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_camera_close_device(vsl_camera* ctx);

/**
 * Checks if DmaBuf export is supported on the camera.
 *
 * DmaBuf support allows zero-copy frame sharing with hardware accelerators
 * (VPU, NPU, GPU). Requires V4L2 driver support for VIDIOC_EXPBUF.
 *
 * Must be called after vsl_camera_init_device().
 *
 * @param ctx Camera context
 * @return 1 if DmaBuf supported, 0 if not supported
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_is_dmabuf_supported(const vsl_camera* ctx);

/**
 * Returns the number of buffers queued in the camera driver.
 *
 * Queued buffers are available for the driver to fill with new frames.
 * If count reaches 0, vsl_camera_get_data() will block/timeout waiting for
 * buffers to be released via vsl_camera_release_buffer().
 *
 * @param ctx Camera context
 * @return Number of queued buffers
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_get_queued_buf_count(const vsl_camera* ctx);

/**
 * Returns the mmap memory pointer of the camera buffer.
 *
 * Provides CPU access to the camera buffer's memory. The buffer is already
 * mapped by the camera driver.
 *
 * @param buffer Camera buffer from vsl_camera_get_data()
 * @return Pointer to mapped memory
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void*
vsl_camera_buffer_mmap(vsl_camera_buffer* buffer);

/**
 * Returns the DmaBuf file descriptor of the camera buffer.
 *
 * Returns the DmaBuf FD for zero-copy sharing with hardware accelerators.
 * Only available if vsl_camera_is_dmabuf_supported() returns true.
 *
 * @param buffer Camera buffer from vsl_camera_get_data()
 * @return DmaBuf file descriptor, or -1 if DmaBuf not supported
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_buffer_dma_fd(const vsl_camera_buffer* buffer);

/**
 * Returns the physical address of the camera buffer.
 *
 * Physical address is available on some platforms (certain i.MX drivers) for
 * DMA operations. Not commonly used - prefer DmaBuf FD for portability.
 *
 * @param buffer Camera buffer from vsl_camera_get_data()
 * @return Physical address, or 0 if not supported
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint64_t
vsl_camera_buffer_phys_addr(const vsl_camera_buffer* buffer);

/**
 * Returns the length of the camera buffer in bytes.
 *
 * Buffer size as reported by the V4L2 driver. For multi-planar formats,
 * this is the total size across all planes.
 *
 * @param buffer Camera buffer from vsl_camera_get_data()
 * @return Buffer length in bytes
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint32_t
vsl_camera_buffer_length(const vsl_camera_buffer* buffer);

/**
 * Returns the fourcc code of the camera buffer.
 *
 * Pixel format as negotiated with the driver during vsl_camera_init_device().
 *
 * @param buffer Camera buffer from vsl_camera_get_data()
 * @return FOURCC code
 * @since 1.3
 * @memberof VSLCamera
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
uint32_t
vsl_camera_buffer_fourcc(const vsl_camera_buffer* buffer);

/**
 * Reads the timestamp of the camera buffer.
 *
 * Retrieves the capture timestamp from the V4L2 buffer. Time is relative to
 * CLOCK_MONOTONIC when the frame was captured by the camera driver.
 *
 * @param buffer Camera buffer from vsl_camera_get_data()
 * @param seconds Output pointer for timestamp seconds
 * @param nanoseconds Output pointer for sub-second nanoseconds
 * @since 1.3
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
void
vsl_camera_buffer_timestamp(const vsl_camera_buffer* buffer,
                            int64_t*                 seconds,
                            int64_t*                 nanoseconds);

/**
 * Lists the supported single-planar formats of the camera.
 *
 * Queries V4L2 device for available single-planar pixel formats. Call before
 * vsl_camera_init_device() to determine what formats to request.
 *
 * @param ctx Camera context from vsl_camera_open_device()
 * @param codes Array to receive fourcc codes
 * @param size Size of codes array
 * @return Number of formats written to codes array, or -1 on error
 * @since 1.3
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_enum_fmts(const vsl_camera* ctx, uint32_t* codes, int size);

/**
 * Lists the supported multi-planar formats of the camera.
 *
 * Queries V4L2 device for available multi-planar pixel formats (e.g., NV12,
 * NV21 with separate Y and UV planes). Call before vsl_camera_init_device().
 *
 * @param ctx Camera context from vsl_camera_open_device()
 * @param codes Array to receive fourcc codes
 * @param size Size of codes array
 * @return Number of formats written to codes array, or -1 on error
 * @since 1.3
 */
VSL_AVAILABLE_SINCE_1_3
VSL_API
int
vsl_camera_enum_mplane_fmts(const vsl_camera* ctx, uint32_t* codes, int size);

/**
 * @brief Creates VSLDecoder instance
 *
 * Creates a hardware video decoder for H.264/H.265 using the best available
 * backend (V4L2 preferred, Hantro fallback). The decoder is initialized on the
 * first call to vsl_decode_frame().
 *
 * @param codec Codec type: VSL_DEC_H264 or VSL_DEC_HEVC
 * @param fps Expected frame rate (used for buffer management)
 * @return Pointer to VSLDecoder instance, or NULL on failure
 * @since 1.4
 */
VSL_AVAILABLE_SINCE_1_4
VSL_API
VSLDecoder*
vsl_decoder_create(VSLDecoderCodec codec, int fps);

/**
 * @brief Creates VSLDecoder instance with explicit backend selection
 *
 * Creates a hardware video decoder with explicit backend selection. Use this
 * when you need to force a specific backend instead of auto-detection.
 *
 * @param codec Codec fourcc: VSL_FOURCC('H','2','6','4') or
 *              VSL_FOURCC('H','E','V','C')
 * @param fps Expected frame rate (used for buffer management)
 * @param backend Backend to use (AUTO, HANTRO, or V4L2)
 * @return Pointer to VSLDecoder instance, or NULL on failure
 * @since 2.0
 */
VSL_AVAILABLE_SINCE_2_0
VSL_API
VSLDecoder*
vsl_decoder_create_ex(uint32_t codec, int fps, VSLCodecBackend backend);

typedef enum {
    VSL_DEC_SUCCESS   = 0x0,
    VSL_DEC_ERR       = 0x1,
    VSL_DEC_INIT_INFO = 0x2,
    VSL_DEC_FRAME_DEC = 0x4,
} VSLDecoderRetCode;

/**
 * @brief Decode compressed video frame
 *
 * Decodes H.264/H.265 data into a raw frame using hardware acceleration.
 * First call initializes the decoder. May require multiple calls to decode
 * one frame (returns VSL_DEC_INIT_INFO or VSL_DEC_FRAME_DEC).
 *
 * @param decoder VSLDecoder instance from vsl_decoder_create()
 * @param data Pointer to compressed video data
 * @param data_length Length of compressed data in bytes
 * @param bytes_used Output: number of bytes consumed from data
 * @param output_frame Output: decoded frame (NULL if frame not yet complete)
 * @return VSL_DEC_SUCCESS (frame decoded), VSL_DEC_INIT_INFO (need more calls),
 *         VSL_DEC_FRAME_DEC (frame in progress), or VSL_DEC_ERR (error)
 * @since 1.4
 */
VSL_AVAILABLE_SINCE_1_4
VSL_API
VSLDecoderRetCode
vsl_decode_frame(VSLDecoder*  decoder,
                 const void*  data,
                 unsigned int data_length,
                 size_t*      bytes_used,
                 VSLFrame**   output_frame);

/**
 * @brief Returns the decoded frame width
 *
 * Returns the width of decoded frames as determined from the stream headers.
 * Only valid after decoder initialization (after first vsl_decode_frame()).
 *
 * @param decoder VSLDecoder instance
 * @return Frame width in pixels
 * @since 1.4
 */
VSL_AVAILABLE_SINCE_1_4
VSL_API
int
vsl_decoder_width(const VSLDecoder* decoder);

/**
 * @brief Returns the decoded frame height
 *
 * Returns the height of decoded frames as determined from the stream headers.
 * Only valid after decoder initialization (after first vsl_decode_frame()).
 *
 * @param decoder VSLDecoder instance
 * @return Frame height in pixels
 * @since 1.4
 */
VSL_AVAILABLE_SINCE_1_4
VSL_API
int
vsl_decoder_height(const VSLDecoder* decoder);

/**
 * @brief Returns the decoder crop rectangle
 *
 * Returns the active video area within decoded frames, as specified in stream
 * headers. Some encoded streams have padding that should be cropped.
 *
 * @param decoder VSLDecoder instance
 * @return VSLRect with crop region (x, y, width, height)
 * @since 1.4
 */
VSL_AVAILABLE_SINCE_1_4
VSL_API
VSLRect
vsl_decoder_crop(const VSLDecoder* decoder);

/**
 * @brief Destroys VSLDecoder instance
 *
 * Frees all resources associated with the decoder, including hardware
 * resources. Do not use the decoder after calling this function.
 *
 * @param decoder VSLDecoder instance to destroy
 * @return 0 on success, -1 on error
 * @since 1.4
 */
VSL_AVAILABLE_SINCE_1_4
VSL_API
int
vsl_decoder_release(VSLDecoder* decoder);

#ifdef __cplusplus
}
#endif

#endif /* VIDEOSTREAM_H */
