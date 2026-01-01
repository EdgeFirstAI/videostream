// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "dma-buf.h"
#include "dma-heap.h"
#include "frame.h"
#include "libg2d.h"
#include "videostream.h"

#define make_fourcc(a, b, c, d)                                        \
    ((uint32_t) (a) | ((uint32_t) (b) << 8) | ((uint32_t) (c) << 16) | \
     ((uint32_t) (d) << 24))

int
frame_stride(uint32_t fourcc, int width)
{
    switch (fourcc) {
    case make_fourcc('R', 'G', 'B', 'A'):
        return width * 4;
    case make_fourcc('R', 'G', 'B', 'X'):
        return width * 4;
    case make_fourcc('R', 'G', 'B', '3'):
        return width * 3;
    case make_fourcc('B', 'G', 'R', 'A'):
        return width * 4;
    case make_fourcc('B', 'G', 'R', 'X'):
        return width * 4;
    case make_fourcc('B', 'G', 'R', '3'):
        return width * 3;
    case make_fourcc('Y', 'U', 'Y', 'V'):
        return width * 2;
    case make_fourcc('Y', 'U', 'Y', '2'):
        return width * 2;
    case make_fourcc('Y', 'V', 'Y', 'U'):
        return width * 2;
    case make_fourcc('U', 'Y', 'V', 'Y'):
        return width * 2;
    case make_fourcc('V', 'Y', 'U', 'Y'):
        return width * 2;
    case make_fourcc('N', 'V', '1', '2'):
        return width + (width >> 1);
    case make_fourcc('I', '4', '2', '0'):
        return width + (width >> 1);
    case make_fourcc('Y', 'V', '1', '2'):
        return width + (width >> 1);
    case make_fourcc('N', 'V', '2', '1'):
        return width + (width >> 1);
    case make_fourcc('N', 'V', '1', '6'):
        return width + (width >> 1);
    case make_fourcc('N', 'V', '6', '1'):
        return width + (width >> 1);
    default:
        return 0;
    }
}

VSL_API
uint32_t
vsl_fourcc_from_string(const char* fourcc)
{
    if (!fourcc || strlen(fourcc) != 4) { return 0; }
    return make_fourcc(fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
}

VSL_API
int64_t
vsl_frame_serial(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.serial;
}

VSL_API
int64_t
vsl_frame_timestamp(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.timestamp;
}

VSL_API
int64_t
vsl_frame_duration(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.duration;
}

VSL_API
int64_t
vsl_frame_pts(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.pts;
}

VSL_API
int64_t
vsl_frame_dts(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.dts;
}

VSL_API
int64_t
vsl_frame_expires(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.expires;
}

VSL_API
uint32_t
vsl_frame_fourcc(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.fourcc;
}

VSL_API
int
vsl_frame_width(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.width;
}

VSL_API
int
vsl_frame_height(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.height;
}

VSL_API
int
vsl_frame_stride(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.stride;
}

VSL_API
int
vsl_frame_size(const VSLFrame* frame)
{
    if (!frame) { return 0; }
    return frame->info.size;
}

VSL_API
intptr_t
vsl_frame_paddr(const VSLFrame* frame)
{
    if (!frame) { return -1; }
    if (!frame->info.paddr) {
        struct vsl_frame_info* info = (struct vsl_frame_info*) &frame->info;
        struct dma_buf_phys    dma_phys;

        if (ioctl(frame->handle, DMA_BUF_IOCTL_PHYS, &dma_phys)) {
            fprintf(stderr,
                    "%s ioctl error: %s\n",
                    __FUNCTION__,
                    strerror(errno));
            return -1;
        }

#ifndef NDEBUG
        printf("%s queried paddr %lx\n", __FUNCTION__, dma_phys.phys);
#endif
        info->paddr = dma_phys.phys;
        return info->paddr;
    }

#ifndef NDEBUG
    printf("%s cached paddr %lx\n", __FUNCTION__, frame->info.paddr);
#endif

    return frame->info.paddr;
}

VSL_API
void*
vsl_frame_mmap(VSLFrame* frame, size_t* size)
{
    if (!frame) { return NULL; }
    if (frame->map) {
        if (size) { *size = frame->mapsize; }
        return frame->map;
    }

    vsl_frame_sync(frame, 1, O_RDWR);

#ifndef NDEBUG
    printf("%s fd: %d size: %zu offset: %zd\n",
           __FUNCTION__,
           frame->handle,
           frame->info.size,
           frame->info.offset);
#endif

    void* map = mmap(NULL,
                     frame->info.size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     frame->handle,
                     frame->info.offset);
    if (map == MAP_FAILED) {
        fprintf(stderr,
                "%s: mmap failed: %s (frame=%p, fd=%d, size=%zu, offset=%zd, "
                "allocator=%d)\n",
                __FUNCTION__,
                strerror(errno),
                (void*) frame,
                frame->handle,
                frame->info.size,
                frame->info.offset,
                frame->allocator);
        return NULL;
    }

    frame->map = map;
    // FIXME: should we calculate the frame size to only map it?  Need to
    // confirm how this works with the offset!
    frame->mapsize = frame->info.size;
    if (size) { *size = frame->mapsize; }

    return map;
}

VSL_API
void
vsl_frame_munmap(VSLFrame* frame)
{
    if (frame && frame->map) {
        munmap(frame->map, frame->mapsize);
        frame->map     = NULL;
        frame->mapsize = 0;
        vsl_frame_sync(frame, 0, O_RDWR);
    }
}

VSL_API
void*
vsl_frame_userptr(VSLFrame* frame)
{
    if (frame) { return frame->userptr; }
    return NULL;
}

VSL_API
void
vsl_frame_set_userptr(VSLFrame* frame, void* userptr)
{
    if (frame) { frame->userptr = userptr; }
}

VSL_API
int
vsl_frame_handle(const VSLFrame* frame)
{
    if (!frame) {
        errno = EINVAL;
        return -1;
    }

    return frame->handle;
}

VSL_API
const char*
vsl_frame_path(const VSLFrame* frame)
{
    if (!frame) {
        errno = EINVAL;
        return NULL;
    }

    return frame->path;
}

VSL_API
VSLFrame*
vsl_frame_init(uint32_t          width,
               uint32_t          height,
               uint32_t          stride,
               uint32_t          fourcc,
               void*             userptr,
               vsl_frame_cleanup cleanup)
{
#ifndef NDEBUG
    printf("vsl_frame_init %dx%d (s=%d) %c%c%c%c\n",
           width,
           height,
           stride,
           fourcc,
           fourcc >> 8,
           fourcc >> 16,
           fourcc >> 24);
#endif

    if (!width || !height || !fourcc) {
        errno = EINVAL;
        return NULL;
    }

    VSLFrame* frame = calloc(1, sizeof(VSLFrame));
    if (!frame) { return NULL; }

    frame->info.width  = width;
    frame->info.height = height;
    frame->info.fourcc = fourcc;
    frame->info.stride = stride ? (int) stride : frame_stride(fourcc, width);

    if (!frame->info.stride) {
        free(frame);
        errno = ENOTSUP;
        return NULL;
    }

    frame->userptr = userptr;
    frame->cleanup = cleanup;

    frame->handle    = -1;
    frame->allocator = VSL_FRAME_ALLOCATOR_EXTERNAL;

#ifndef NDEBUG
    printf("%s created frame %p with parent %p\n",
           __FUNCTION__,
           frame,
           frame->_parent);
#endif

    return frame;
}

VSL_API
void
vsl_frame_release(VSLFrame* frame)
{
    errno = 0;

    if (!frame) {
        errno = EINVAL;
        return;
    }

#ifndef NDEBUG
    printf("%s %p\n", __FUNCTION__, frame);
#endif

    if (frame->_parent) {
        fprintf(stderr,
                "%s frame %p has deprecated parent %p\n",
                __FUNCTION__,
                frame,
                frame->_parent);
    }

    vsl_frame_munmap(frame);

    if (frame->host) { vsl_host_drop(frame->host, frame); }
    if (frame->client) { vsl_frame_unlock(frame); }

    vsl_frame_unalloc(frame);
    if (frame->cleanup) { frame->cleanup(frame); }

    free(frame);
}

static int
frame_alloc_shm(VSLFrame* frame)
{
    frame->info.offset = 0;
    frame->info.size   = frame_stride(frame->info.fourcc, frame->info.width) *
                       frame->info.height;

#ifndef NDEBUG
    printf("%s path: %s size: %d\n",
           __FUNCTION__,
           frame->path,
           frame->info.size);
#endif

    if (frame->info.size == 0) {
        errno = ENOTSUP;
        return -1;
    }

    int fd = shm_open(frame->path, O_RDWR | O_CREAT, 0660);
    if (fd == -1) {
        fprintf(stderr,
                "%s: shm_open failed: %s\n",
                __FUNCTION__,
                strerror(errno));
        return -1;
    }

    if (ftruncate(fd, frame->info.size)) {
        fprintf(stderr,
                "%s: ftruncate failed: %s\n",
                __FUNCTION__,
                strerror(errno));
        shm_unlink(frame->path);
        return -1;
    }

    frame->handle    = fd;
    frame->allocator = VSL_FRAME_ALLOCATOR_SHM;

    return 0;
}

static void
frame_unalloc_shm(VSLFrame* frame)
{
    if (frame->handle > 2) { close(frame->handle); }
    frame->handle = -1;
    shm_unlink(frame->path);
    frame->allocator = VSL_FRAME_ALLOCATOR_EXTERNAL;
}

static int
frame_alloc_dma(VSLFrame* frame)
{
    frame->info.offset = 0;

    // If size is already set (e.g., by V4L2 decoder for driver alignment),
    // respect it. Otherwise calculate from dimensions.
    if (frame->info.size == 0) {
        frame->info.size = frame_stride(frame->info.fourcc, frame->info.width) *
                           frame->info.height;
    }

#ifndef NDEBUG
    printf("%s path: %s size: %zu\n",
           __FUNCTION__,
           frame->path ? frame->path : "NULL",
           frame->info.size);
#endif

    if (frame->info.size == 0) {
        errno = ENOTSUP;
        return -1;
    }

    if (!frame->path) {
        errno = ENOENT;
        return -1;
    }

    int fd = open(frame->path, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
#ifndef NDEBUG
        fprintf(stderr,
                "%s failed to open %s: %s\n",
                __FUNCTION__,
                frame->path,
                strerror(errno));
#endif
        return -1;
    }

    struct dma_heap_allocation_data heap_data = {
        .len      = frame->info.size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };

    if (ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &heap_data)) {
#ifndef DEBUG
        fprintf(stderr,
                "%s: dma heap alloc failed: %s\n",
                __FUNCTION__,
                strerror(errno));
#endif
        close(fd);
        return -1;
    }

    close(fd);

    frame->handle    = heap_data.fd;
    frame->allocator = VSL_FRAME_ALLOCATOR_DMAHEAP;

    return 0;
}

static void
frame_unalloc_dma(VSLFrame* frame)
{
    if (frame->handle > 2) { close(frame->handle); }
    frame->handle    = -1;
    frame->allocator = VSL_FRAME_ALLOCATOR_EXTERNAL;
}

VSL_API
int
vsl_frame_sync(const VSLFrame* frame, int enable, int mode)
{
    struct dma_buf_sync sync = {0};

    sync.flags |= enable ? DMA_BUF_SYNC_START : DMA_BUF_SYNC_END;

    // O_RDONLY is 0, so bitwise AND doesn't work. Use equality checks instead.
    // Set READ flag if mode is not write-only (i.e., RDONLY or RDWR)
    // Set WRITE flag if mode is not read-only (i.e., WRONLY or RDWR)
    if (mode != O_WRONLY) { sync.flags |= DMA_BUF_SYNC_READ; }
    if (mode != O_RDONLY) { sync.flags |= DMA_BUF_SYNC_WRITE; }

#ifndef NDEBUG
    printf("%s (%d %d) %d %s %s\n",
           __FUNCTION__,
           enable,
           mode,
           sync.flags & DMA_BUF_SYNC_RW,
           sync.flags & DMA_BUF_SYNC_END ? "end" : "start",
           sync.flags & DMA_BUF_SYNC_RW == DMA_BUF_SYNC_RW ? "rw"
           : sync.flags & DMA_BUF_SYNC_WRITE               ? "w"
                                                           : "r");
#endif

    if (!frame || frame->handle == -1) {
        errno = EINVAL;
        return -1;
    }

    // Sync is only supported and required with dma buffers.
    if (frame->allocator != VSL_FRAME_ALLOCATOR_DMAHEAP) { return 0; }

    return ioctl(frame->handle, DMA_BUF_IOCTL_SYNC, &sync);
}

VSL_API
int
vsl_frame_alloc(VSLFrame* frame, const char* path)
{
    if (!frame) {
        errno = EINVAL;
        return -1;
    }

    vsl_frame_unalloc(frame);

    // If path is provided and is not /dev then we're creating a shared memory
    // buffer.
    if (path && strncmp(path, "/dev", 4)) {
        frame->path = strdup(path);
        return frame_alloc_shm(frame);
    }

    // If path was not provided look for possible dmabuf heap, otherwise
    // fallback to shared memory.
    if (!path) {
        if (access("/dev/dma_heap/linux,cma", R_OK | W_OK) == 0) {
            frame->path = strdup("/dev/dma_heap/linux,cma");
        } else if (access("/dev/dma_heap/system", R_OK | W_OK) == 0) {
            frame->path = strdup("/dev/dma_heap/system");
        } else {
            frame->path = calloc(1, 128);
            snprintf(frame->path,
                     128,
                     "/VSL_%ld_%ld",
                     (long) getpid(),
                     (long) syscall(SYS_gettid));

            return frame_alloc_shm(frame);
        }
    }

    return frame_alloc_dma(frame);
}

VSL_API
void
vsl_frame_unalloc(VSLFrame* frame)
{
    if (!frame) { return; }

#ifndef NDEBUG
    printf("%s handle: %d, allocator: %d\n",
           __FUNCTION__,
           frame->handle,
           frame->allocator);
#endif

    vsl_frame_munmap(frame);

    switch (frame->allocator) {
    case VSL_FRAME_ALLOCATOR_SHM:
        frame_unalloc_shm(frame);
        break;
    case VSL_FRAME_ALLOCATOR_DMAHEAP:
        frame_unalloc_dma(frame);
        break;
    case VSL_FRAME_ALLOCATOR_EXTERNAL:
        /* Owned externally.
           NOTE: When using vslsink externally allocated frame is provided with
           duplicated fd which must be closed to avoid leak.
           However, if there's a cleanup callback, the owner (e.g., VPU decoder)
           manages the fd and we should NOT close it here. The cleanup callback
           can access the handle before we clear it. */
        if (frame->cleanup) {
            // Owner has cleanup callback - they manage the fd
            // Don't close, don't clear - cleanup callback may need the handle
            return;
        }
        // No cleanup callback - this is a dup'd fd that we should close
        if (frame->handle >= 0) {
            close(frame->handle);
            frame->handle = -1;
        }
        return;
    }

    if (frame->path) {
        free(frame->path);
        frame->path = NULL;
    }

    frame->info.size   = 0;
    frame->info.offset = 0;
}

VSL_API
int
vsl_frame_attach(VSLFrame* frame, int fd, size_t size, size_t offset)
{
    if (!frame) {
        errno = EINVAL;
        return -1;
    }

    // Reject fd <= 0 as these are invalid or reserved (stdin)
    if (fd <= 0) {
        fprintf(stderr, "%s: invalid fd %d (must be > 0)\n", __FUNCTION__, fd);
        errno = EBADF;
        return -1;
    }

    vsl_frame_unalloc(frame);

    // Verify we received a valid file descriptor, otherwise return.
    // On failure errno will report the invalid file descriptor.
    int mode = fcntl(fd, F_GETFL);
    if (mode == -1) { return -1; }

#ifndef NDEBUG
    printf("%s: RD: %d WR: %d RW: %d\n",
           __FUNCTION__,
           mode & O_RDONLY,
           mode & O_WRONLY,
           mode & O_RDWR);
#endif

    if (!size) {
        size = frame_stride(frame->info.fourcc, frame->info.width) *
               frame->info.height;

#ifndef NDEBUG
        printf("%s size: %d stride: %d height: %d fourcc: %c%c%c%c\n",
               __FUNCTION__,
               size,
               frame->info.stride,
               frame->info.height,
               frame->info.fourcc,
               frame->info.fourcc >> 8,
               frame->info.fourcc >> 16,
               frame->info.fourcc >> 24);
#endif

        if (size == 0) {
            errno = ENOTSUP;
            return -1;
        }
    }

    frame->handle      = dup(fd);
    frame->info.offset = offset;
    frame->info.size   = size;

    frame->allocator = VSL_FRAME_ALLOCATOR_EXTERNAL;

    if (frame->handle == -1) { return -1; }

    // Detect if dup returned a stdio fd (shouldn't happen unless stdio was
    // closed)
    if (frame->handle >= 0 && frame->handle <= 2) {
        fprintf(stderr,
                "%s: ERROR: dup(%d) returned stdio fd %d - fd 0/1/2 was "
                "closed!\n",
                __FUNCTION__,
                fd,
                frame->handle);
        close(frame->handle);
        frame->handle = -1;
        errno         = EBADF;
        return -1;
    }
    return 0;
}

VSL_API
int
vsl_frame_copy(VSLFrame* target, VSLFrame* source, const VSLRect* crop)
{
    (void) target;
    (void) source;
    (void) crop;

    errno = ENOTSUP;
    return -1;
}
