// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#ifndef VSL_COMMON_H
#define VSL_COMMON_H

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#ifndef _WIN32
#define SOCKET int
#endif

/**
 * Align a value to the specified alignment boundary.
 *
 * @param val Value to align (integer or pointer cast to unsigned long)
 * @param align Alignment boundary (must be power of 2)
 * @return Aligned value
 */
#define VSL_ALIGN(val, align) \
    ((((unsigned long) (val)) + ((align) - 1)) / (align) * (align))

// =============================================================================
// Safe String Functions (C11 Annex K style)
// =============================================================================
//
// These functions follow the conventions of strcpy_s/strncpy_s:
// - Return 0 on success, error code on failure
// - Always null-terminate destination on success
// - Set destination to empty string on error (security)
// - Validate all parameters

/**
 * Safely copy a string with bounds checking (strcpy_s equivalent).
 *
 * Copies src to dest, ensuring null-termination and bounds safety.
 * On error, dest is set to an empty string for security.
 *
 * @param dest    Destination buffer
 * @param destsz  Size of destination buffer in bytes
 * @param src     Source string to copy
 * @return 0 on success, EINVAL if dest/src is NULL or destsz is 0,
 *         ERANGE if source string doesn't fit in destination
 */
static inline int
vsl_strcpy_s(char* dest, size_t destsz, const char* src)
{
    if (dest == NULL || destsz == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return EINVAL;
    }

    size_t src_len = strlen(src);
    if (src_len >= destsz) {
        dest[0] = '\0';
        return ERANGE;
    }

    memcpy(dest, src, src_len + 1);
    return 0;
}

/**
 * Safely copy up to n characters with bounds checking (strncpy_s equivalent).
 *
 * Copies at most count characters from src to dest, ensuring null-termination.
 * On error, dest is set to an empty string for security.
 *
 * @param dest    Destination buffer
 * @param destsz  Size of destination buffer in bytes
 * @param src     Source string to copy
 * @param count   Maximum number of characters to copy (excluding null)
 * @return 0 on success, EINVAL if dest/src is NULL or destsz is 0,
 *         ERANGE if the copy would overflow the destination
 */
static inline int
vsl_strncpy_s(char* dest, size_t destsz, const char* src, size_t count)
{
    if (dest == NULL || destsz == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return EINVAL;
    }

    // Determine how many characters to copy
    size_t src_len  = strlen(src);
    size_t copy_len = (src_len < count) ? src_len : count;

    // Check if it fits (need room for null terminator)
    if (copy_len >= destsz) {
        dest[0] = '\0';
        return ERANGE;
    }

    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    return 0;
}

/**
 * Get current monotonic timestamp in microseconds.
 * Suitable for timing measurements and performance instrumentation.
 *
 * @return Current timestamp in microseconds
 */
int64_t
vsl_timestamp_us(void);

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