// SPDX-License-Identifier: Apache-2.0
// Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include <gst/allocators/allocators.h>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include "videostream.h"
#include "vslsink.h"

#define NANOS_PER_MILLI 1000000
#define DEFAULT_LIFESPAN 100
#define DEFAULT_POOL_SIZE 8
#define POLL_TIMEOUT_MS 1000
#define DMA_HEAP_PATH "/dev/dma_heap/linux,cma"
#define DMA_HEAP_PATH_ALT "/dev/dma_heap/system"

GST_DEBUG_CATEGORY_STATIC(vsl_sink_debug_category);
#define GST_CAT_DEFAULT vsl_sink_debug_category

// Structure to hold pool entry reference for cleanup callback
typedef struct {
    VslSink*         vslsink; // Back-reference to sink for pool access
    DmaBufPoolEntry* entry;   // Pool entry to release
} DmaBufPoolRef;

// ============================================================================
// VslDmaBufBufferPool - Custom GstBufferPool that allocates from dma_heap
// ============================================================================
//
// This pool provides dmabuf-backed buffers to upstream elements that cannot
// allocate dmabuf themselves (e.g., videotestsrc). By proposing this pool
// in propose_allocation, upstream sources write directly into our dmabuf
// memory, enabling zero-copy sharing with IPC clients.
//
// Flow:
// 1. Upstream acquires buffer from this pool (gets mmap'd dmabuf pointer)
// 2. Upstream writes frame data into our dmabuf
// 3. Buffer flows to vslsink render()
// 4. We extract dmabuf fd and share with IPC clients - zero copy!

#define VSL_TYPE_DMABUF_BUFFER_POOL (vsl_dmabuf_buffer_pool_get_type())
#define VSL_DMABUF_BUFFER_POOL(obj)                          \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),                       \
                                VSL_TYPE_DMABUF_BUFFER_POOL, \
                                VslDmaBufBufferPool))
#define VSL_IS_DMABUF_BUFFER_POOL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), VSL_TYPE_DMABUF_BUFFER_POOL))

typedef struct _VslDmaBufBufferPool {
    GstVideoBufferPool parent;

    VslSink*      vslsink;      // Back-reference to owning sink
    GstAllocator* dmabuf_alloc; // DmaBuf allocator for wrapping fds
    GstVideoInfo  video_info;   // Video format info
    gboolean      add_videometa;

    // Pool of pre-allocated dmabuf entries
    DmaBufPoolEntry* entries;
    gsize            entry_count;
    gsize            next_entry;
    GMutex           entry_lock;
    gboolean         entries_allocated;
} VslDmaBufBufferPool;

typedef struct _VslDmaBufBufferPoolClass {
    GstVideoBufferPoolClass parent_class;
} VslDmaBufBufferPoolClass;

G_DEFINE_TYPE(VslDmaBufBufferPool,
              vsl_dmabuf_buffer_pool,
              GST_TYPE_VIDEO_BUFFER_POOL)

// Quark for storing entry reference on buffers
static GQuark vsl_pool_entry_quark;

static void
vsl_dmabuf_buffer_pool_init(VslDmaBufBufferPool* pool)
{
    pool->vslsink           = NULL;
    pool->dmabuf_alloc      = NULL;
    pool->entries           = NULL;
    pool->entry_count       = 0;
    pool->next_entry        = 0;
    pool->entries_allocated = FALSE;
    pool->add_videometa     = FALSE;
    g_mutex_init(&pool->entry_lock);
}

static void
vsl_dmabuf_buffer_pool_finalize(GObject* object)
{
    VslDmaBufBufferPool* pool = VSL_DMABUF_BUFFER_POOL(object);

    GST_DEBUG_OBJECT(pool, "finalizing dmabuf buffer pool");

    // Free allocated dmabuf entries
    if (pool->entries) {
        for (gsize i = 0; i < pool->entry_count; i++) {
            DmaBufPoolEntry* entry = &pool->entries[i];
            if (entry->map_ptr && entry->map_ptr != MAP_FAILED) {
                munmap(entry->map_ptr, entry->map_size);
            }
            if (entry->dmabuf_fd >= 0) { close(entry->dmabuf_fd); }
        }
        g_free(pool->entries);
        pool->entries = NULL;
    }

    if (pool->dmabuf_alloc) {
        gst_object_unref(pool->dmabuf_alloc);
        pool->dmabuf_alloc = NULL;
    }

    g_mutex_clear(&pool->entry_lock);

    G_OBJECT_CLASS(vsl_dmabuf_buffer_pool_parent_class)->finalize(object);
}

// Allocate a dmabuf entry from dma_heap
static gboolean
vsl_dmabuf_pool_alloc_entry(DmaBufPoolEntry* entry, gsize size)
{
    int heap_fd = open(DMA_HEAP_PATH, O_RDWR);
    if (heap_fd < 0) {
        heap_fd = open(DMA_HEAP_PATH_ALT, O_RDWR);
        if (heap_fd < 0) { return FALSE; }
    }

    struct dma_heap_allocation_data alloc = {
        .len      = size,
        .fd_flags = O_CLOEXEC | O_RDWR,
    };

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
        close(heap_fd);
        return FALSE;
    }
    close(heap_fd);

    void* map_ptr =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
    if (map_ptr == MAP_FAILED) {
        close(alloc.fd);
        return FALSE;
    }

    entry->dmabuf_fd = alloc.fd;
    entry->map_ptr   = map_ptr;
    entry->map_size  = size;
    entry->in_use    = FALSE;

    return TRUE;
}

// Configure the pool with video info
static gboolean
vsl_dmabuf_buffer_pool_set_config(GstBufferPool* bpool, GstStructure* config)
{
    VslDmaBufBufferPool* pool = VSL_DMABUF_BUFFER_POOL(bpool);
    GstCaps*             caps;
    guint                size, min_buffers, max_buffers;

    if (!gst_buffer_pool_config_get_params(config,
                                           &caps,
                                           &size,
                                           &min_buffers,
                                           &max_buffers)) {
        GST_WARNING_OBJECT(pool, "invalid config");
        return FALSE;
    }

    if (!caps) {
        GST_WARNING_OBJECT(pool, "no caps in config");
        return FALSE;
    }

    if (!gst_video_info_from_caps(&pool->video_info, caps)) {
        GST_WARNING_OBJECT(pool, "failed to parse video info from caps");
        return FALSE;
    }

    // Check if we should add video meta
    pool->add_videometa =
        gst_buffer_pool_config_has_option(config,
                                          GST_BUFFER_POOL_OPTION_VIDEO_META);

    GST_DEBUG_OBJECT(pool,
                     "configured: %dx%d, size=%u, min=%u, max=%u, videometa=%d",
                     GST_VIDEO_INFO_WIDTH(&pool->video_info),
                     GST_VIDEO_INFO_HEIGHT(&pool->video_info),
                     size,
                     min_buffers,
                     max_buffers,
                     pool->add_videometa);

    // Chain up to parent
    return GST_BUFFER_POOL_CLASS(vsl_dmabuf_buffer_pool_parent_class)
        ->set_config(bpool, config);
}

// Start the pool - allocate dmabuf entries
static gboolean
vsl_dmabuf_buffer_pool_start(GstBufferPool* bpool)
{
    VslDmaBufBufferPool* pool = VSL_DMABUF_BUFFER_POOL(bpool);
    GstStructure*        config;
    guint                size, min_buffers, max_buffers;

    config = gst_buffer_pool_get_config(bpool);
    gst_buffer_pool_config_get_params(config,
                                      NULL,
                                      &size,
                                      &min_buffers,
                                      &max_buffers);
    gst_structure_free(config);

    // Use max_buffers if set, otherwise min_buffers, with sensible default
    gsize count = max_buffers > 0
                      ? max_buffers
                      : (min_buffers > 0 ? min_buffers : DEFAULT_POOL_SIZE);

    g_mutex_lock(&pool->entry_lock);

    if (!pool->entries_allocated) {
        pool->entry_count = count;
        pool->entries     = g_new0(DmaBufPoolEntry, count);

        GST_INFO_OBJECT(pool,
                        "allocating %zu dmabuf entries x %u bytes from "
                        "dma_heap",
                        count,
                        size);

        for (gsize i = 0; i < count; i++) {
            if (!vsl_dmabuf_pool_alloc_entry(&pool->entries[i], size)) {
                GST_ERROR_OBJECT(pool,
                                 "failed to allocate dmabuf entry %zu: %s",
                                 i,
                                 strerror(errno));
                // Clean up partial allocations
                for (gsize j = 0; j < i; j++) {
                    if (pool->entries[j].map_ptr) {
                        munmap(pool->entries[j].map_ptr,
                               pool->entries[j].map_size);
                    }
                    if (pool->entries[j].dmabuf_fd >= 0) {
                        close(pool->entries[j].dmabuf_fd);
                    }
                }
                g_free(pool->entries);
                pool->entries     = NULL;
                pool->entry_count = 0;
                g_mutex_unlock(&pool->entry_lock);
                return FALSE;
            }
            GST_DEBUG_OBJECT(pool,
                             "allocated dmabuf entry %zu: fd=%d",
                             i,
                             pool->entries[i].dmabuf_fd);
        }

        pool->next_entry        = 0;
        pool->entries_allocated = TRUE;
    }

    g_mutex_unlock(&pool->entry_lock);

    // Chain up
    return GST_BUFFER_POOL_CLASS(vsl_dmabuf_buffer_pool_parent_class)
        ->start(bpool);
}

// Acquire a dmabuf entry (round-robin)
static DmaBufPoolEntry*
vsl_dmabuf_pool_acquire_entry(VslDmaBufBufferPool* pool)
{
    DmaBufPoolEntry* entry = NULL;

    g_mutex_lock(&pool->entry_lock);

    if (!pool->entries_allocated || pool->entry_count == 0) {
        g_mutex_unlock(&pool->entry_lock);
        return NULL;
    }

    // Round-robin through entries, looking for one not in use
    for (gsize attempts = 0; attempts < pool->entry_count; attempts++) {
        gsize idx = (pool->next_entry + attempts) % pool->entry_count;
        if (!pool->entries[idx].in_use) {
            entry            = &pool->entries[idx];
            entry->in_use    = TRUE;
            pool->next_entry = (idx + 1) % pool->entry_count;
            GST_LOG_OBJECT(pool,
                           "acquired entry %zu: fd=%d",
                           idx,
                           entry->dmabuf_fd);
            break;
        }
    }

    g_mutex_unlock(&pool->entry_lock);

    if (!entry) {
        GST_WARNING_OBJECT(pool,
                           "all %zu dmabuf entries in use",
                           pool->entry_count);
    }

    return entry;
}

// Release entry back to pool
static void
vsl_dmabuf_pool_release_entry(VslDmaBufBufferPool* pool, DmaBufPoolEntry* entry)
{
    g_mutex_lock(&pool->entry_lock);
    if (entry) {
        entry->in_use = FALSE;
        GST_LOG_OBJECT(pool, "released entry: fd=%d", entry->dmabuf_fd);
    }
    g_mutex_unlock(&pool->entry_lock);
}

// Callback when buffer is released - return entry to pool
static void
vsl_dmabuf_buffer_released(gpointer data)
{
    DmaBufPoolEntry* entry = data;
    // The entry's in_use flag will be cleared by reset_buffer
    (void) entry;
}

// Allocate a buffer from the pool
static GstFlowReturn
vsl_dmabuf_buffer_pool_alloc_buffer(GstBufferPool*              bpool,
                                    GstBuffer**                 buffer,
                                    GstBufferPoolAcquireParams* params)
{
    VslDmaBufBufferPool* pool = VSL_DMABUF_BUFFER_POOL(bpool);
    (void) params;

    // Acquire a dmabuf entry
    DmaBufPoolEntry* entry = vsl_dmabuf_pool_acquire_entry(pool);
    if (!entry) {
        GST_ERROR_OBJECT(pool, "failed to acquire dmabuf entry");
        return GST_FLOW_ERROR;
    }

    // dup() the fd - GStreamer will close its copy, we keep the original
    int fd_copy = dup(entry->dmabuf_fd);
    if (fd_copy < 0) {
        GST_ERROR_OBJECT(pool,
                         "failed to dup fd %d: %s",
                         entry->dmabuf_fd,
                         strerror(errno));
        vsl_dmabuf_pool_release_entry(pool, entry);
        return GST_FLOW_ERROR;
    }

    // Wrap fd as GstMemory using dmabuf allocator
    GstMemory* mem = gst_dmabuf_allocator_alloc(pool->dmabuf_alloc,
                                                fd_copy,
                                                entry->map_size);
    if (!mem) {
        GST_ERROR_OBJECT(pool, "failed to wrap fd as GstMemory");
        close(fd_copy);
        vsl_dmabuf_pool_release_entry(pool, entry);
        return GST_FLOW_ERROR;
    }

    // Create buffer with this memory
    *buffer = gst_buffer_new();
    gst_buffer_append_memory(*buffer, mem);

    // Add video meta for proper stride/plane handling
    if (pool->add_videometa) {
        gst_buffer_add_video_meta_full(*buffer,
                                       GST_VIDEO_FRAME_FLAG_NONE,
                                       GST_VIDEO_INFO_FORMAT(&pool->video_info),
                                       GST_VIDEO_INFO_WIDTH(&pool->video_info),
                                       GST_VIDEO_INFO_HEIGHT(&pool->video_info),
                                       GST_VIDEO_INFO_N_PLANES(
                                           &pool->video_info),
                                       pool->video_info.offset,
                                       pool->video_info.stride);
    }

    // Store entry pointer on buffer for release tracking
    gst_mini_object_set_qdata(GST_MINI_OBJECT(*buffer),
                              vsl_pool_entry_quark,
                              entry,
                              vsl_dmabuf_buffer_released);

    GST_LOG_OBJECT(pool,
                   "allocated buffer %p with dmabuf fd=%d (dup'd as %d)",
                   *buffer,
                   entry->dmabuf_fd,
                   fd_copy);

    return GST_FLOW_OK;
}

// Reset buffer for reuse - release entry back to pool
static void
vsl_dmabuf_buffer_pool_reset_buffer(GstBufferPool* bpool, GstBuffer* buffer)
{
    VslDmaBufBufferPool* pool = VSL_DMABUF_BUFFER_POOL(bpool);

    // Get the entry stored on this buffer and release it
    DmaBufPoolEntry* entry = gst_mini_object_get_qdata(GST_MINI_OBJECT(buffer),
                                                       vsl_pool_entry_quark);
    if (entry) { vsl_dmabuf_pool_release_entry(pool, entry); }

    // Chain up
    GST_BUFFER_POOL_CLASS(vsl_dmabuf_buffer_pool_parent_class)
        ->reset_buffer(bpool, buffer);
}

static void
vsl_dmabuf_buffer_pool_class_init(VslDmaBufBufferPoolClass* klass)
{
    GObjectClass*       gobject_class = G_OBJECT_CLASS(klass);
    GstBufferPoolClass* pool_class    = GST_BUFFER_POOL_CLASS(klass);

    gobject_class->finalize = vsl_dmabuf_buffer_pool_finalize;

    pool_class->set_config   = vsl_dmabuf_buffer_pool_set_config;
    pool_class->start        = vsl_dmabuf_buffer_pool_start;
    pool_class->alloc_buffer = vsl_dmabuf_buffer_pool_alloc_buffer;
    pool_class->reset_buffer = vsl_dmabuf_buffer_pool_reset_buffer;

    // Initialize quark for entry tracking
    vsl_pool_entry_quark = g_quark_from_static_string("vsl-pool-entry");
}

// Create a new VslDmaBufBufferPool
static GstBufferPool*
vsl_dmabuf_buffer_pool_new(VslSink* vslsink)
{
    VslDmaBufBufferPool* pool = g_object_new(VSL_TYPE_DMABUF_BUFFER_POOL, NULL);

    pool->vslsink      = vslsink;
    pool->dmabuf_alloc = gst_dmabuf_allocator_new();

    if (!pool->dmabuf_alloc) {
        GST_WARNING_OBJECT(pool, "failed to create dmabuf allocator");
        gst_object_unref(pool);
        return NULL;
    }

    GST_DEBUG_OBJECT(pool, "created dmabuf buffer pool for vslsink");

    return GST_BUFFER_POOL(pool);
}

// ============================================================================
// End of VslDmaBufBufferPool
// ============================================================================

#define vsl_sink_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE(
    VslSink,
    vsl_sink,
    GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT(vsl_sink_debug_category,
                            "vslsink",
                            0,
                            "debug category for vslsink element"));

#define VIDEO_SINK_CAPS                                                      \
    GST_VIDEO_CAPS_MAKE("{ NV12, YV12, I420, YUY2, YUYV, UYVY, RGBA, RGBx, " \
                        "RGB, BGRA, BGRx, BGR }")

typedef enum {
    PROP_0,
    PROP_PATH,
    PROP_LIFESPAN,
    PROP_POOL_SIZE,
    N_PROPERTIES
} VslSinkProperty;

static GParamSpec* properties[N_PROPERTIES] = {
    NULL,
};

static void
vsl_task(void* data)
{
    VslSink* vslsink = VSL_SINK(data);

    if (!vslsink->host) {
        GST_WARNING_OBJECT(vslsink, "vsl host unavailable");
        return;
    }

    if (!vslsink->sockets) {
        GST_ERROR_OBJECT(vslsink, "sockets buffer unavailable");
        return;
    }

    if (vsl_host_poll(vslsink->host, POLL_TIMEOUT_MS)) {
        size_t max_sockets;
        if (vsl_host_sockets(vslsink->host,
                             vslsink->n_sockets,
                             vslsink->sockets,
                             &max_sockets)) {
            if (errno == ENOBUFS) {
                vslsink->n_sockets = max_sockets * 2;
                vslsink->sockets =
                    realloc(vslsink->sockets, vslsink->n_sockets);
                return;
            } else {
                GST_ERROR_OBJECT(vslsink,
                                 "failed to query "
                                 "sockets: %s\n",
                                 strerror(errno));
                return;
            }
        }

        for (int i = 1; i < max_sockets; i++) {
            int err = vsl_host_service(vslsink->host, vslsink->sockets[i]);
            if (!err || errno == EPIPE || errno == ECONNRESET ||
                errno == ENOTSOCK || errno == EBADF) {
                continue;
            }
            GST_WARNING_OBJECT(vslsink,
                               "client %d error - %s",
                               vslsink->sockets[i],
                               strerror(errno));
        }
    }
}

static void
vsl_sink_init(VslSink* vslsink)
{
    vslsink->n_sockets = 32;
    vslsink->sockets   = calloc(vslsink->n_sockets, sizeof(int));
    vslsink->lifespan  = DEFAULT_LIFESPAN;
    vslsink->pool_size = DEFAULT_POOL_SIZE;
    g_rec_mutex_init(&vslsink->mutex);
    vslsink->task = gst_task_new(vsl_task, vslsink, NULL);
    gst_task_set_lock(vslsink->task, &vslsink->mutex);

    // Initialize pool structure (actual buffers allocated on first frame)
    g_mutex_init(&vslsink->dmabuf_pool.lock);
    vslsink->dmabuf_pool.entries     = NULL;
    vslsink->dmabuf_pool.count       = 0;
    vslsink->dmabuf_pool.next_idx    = 0;
    vslsink->dmabuf_pool.initialized = FALSE;
}

static void
set_property(GObject*      object,
             guint         property_id,
             const GValue* value,
             GParamSpec*   pspec)
{
    VslSink* vslsink = VSL_SINK(object);

    switch ((VslSinkProperty) property_id) {
    case PROP_PATH:
        if (vslsink->path) {
            GST_WARNING_OBJECT(vslsink,
                               "cannot adjust path once set (currently: %s)",
                               vslsink->path);
            break;
        }

        vslsink->path = g_value_dup_string(value);
        break;
    case PROP_LIFESPAN:
        vslsink->lifespan = g_value_get_int64(value);
        break;
    case PROP_POOL_SIZE:
        if (vslsink->dmabuf_pool.initialized) {
            GST_WARNING_OBJECT(vslsink,
                               "cannot change pool-size after pool is "
                               "initialized");
            break;
        }
        vslsink->pool_size = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
get_property(GObject*    object,
             guint       property_id,
             GValue*     value,
             GParamSpec* pspec)
{
    VslSink* vslsink = VSL_SINK(object);

    switch ((VslSinkProperty) property_id) {
    case PROP_PATH:
        g_value_set_string(value, vslsink->path);
        break;
    case PROP_LIFESPAN:
        g_value_set_int64(value, vslsink->lifespan);
        break;
    case PROP_POOL_SIZE:
        g_value_set_uint(value, vslsink->pool_size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

// Free all DMA buffer pool entries
static void
dmabuf_pool_free(VslSink* vslsink)
{
    DmaBufPool* pool = &vslsink->dmabuf_pool;

    g_mutex_lock(&pool->lock);

    if (pool->entries) {
        for (size_t i = 0; i < pool->count; i++) {
            DmaBufPoolEntry* entry = &pool->entries[i];
            if (entry->map_ptr && entry->map_ptr != MAP_FAILED) {
                munmap(entry->map_ptr, entry->map_size);
            }
            if (entry->dmabuf_fd >= 0) { close(entry->dmabuf_fd); }
        }
        g_free(pool->entries);
        pool->entries = NULL;
    }
    pool->count       = 0;
    pool->next_idx    = 0;
    pool->initialized = FALSE;

    g_mutex_unlock(&pool->lock);

    GST_DEBUG_OBJECT(vslsink, "freed DMA buffer pool");
}

static void
dispose(GObject* object)
{
    VslSink* vslsink = VSL_SINK(object);
    GST_LOG_OBJECT(vslsink, "dispose");

    if (vslsink->host) {
        gst_task_stop(vslsink->task);
        gst_task_join(vslsink->task);
        vsl_host_release(vslsink->host);
        vslsink->host = NULL;
    }

    // Free pool before releasing host (frames may still reference pool)
    dmabuf_pool_free(vslsink);

    gst_object_unref(vslsink->task);

    if (vslsink->sockets) { free(vslsink->sockets); }

    if (vslsink->path) { free(vslsink->path); }

    g_rec_mutex_clear(&vslsink->mutex);
    g_mutex_clear(&vslsink->dmabuf_pool.lock);

    G_OBJECT_CLASS(vsl_sink_parent_class)->dispose(object);
}

static void
finalize(GObject* object)
{
    VslSink* vslsink = VSL_SINK(object);
    GST_LOG_OBJECT(vslsink, "finalize");
    G_OBJECT_CLASS(vsl_sink_parent_class)->finalize(object);
}

static gboolean
propose_allocation(GstBaseSink* sink, GstQuery* query)
{
    VslSink* vslsink = VSL_SINK(sink);
    GstCaps* caps;
    gboolean need_pool;

    gst_query_parse_allocation(query, &caps, &need_pool);

    if (!caps) {
        GST_DEBUG_OBJECT(vslsink,
                         "no caps in allocation query, skipping pool proposal");
        return TRUE;
    }

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        GST_WARNING_OBJECT(vslsink, "failed to parse video info from caps");
        return TRUE;
    }

    GST_DEBUG_OBJECT(vslsink,
                     "proposing dmabuf pool for %dx%d %s frames",
                     GST_VIDEO_INFO_WIDTH(&info),
                     GST_VIDEO_INFO_HEIGHT(&info),
                     gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)));

    // Create our custom dmabuf buffer pool
    // This allows sources that can't allocate dmabuf (e.g., videotestsrc)
    // to write directly into our dmabuf memory for zero-copy IPC sharing
    GstBufferPool* pool = vsl_dmabuf_buffer_pool_new(vslsink);
    if (pool) {
        // Configure the pool
        GstStructure* config = gst_buffer_pool_get_config(pool);
        gst_buffer_pool_config_set_params(config,
                                          caps,
                                          info.size,
                                          4,                   // min buffers
                                          vslsink->pool_size); // max buffers
        gst_buffer_pool_config_add_option(config,
                                          GST_BUFFER_POOL_OPTION_VIDEO_META);

        if (!gst_buffer_pool_set_config(pool, config)) {
            GST_WARNING_OBJECT(vslsink, "failed to configure dmabuf pool");
            gst_object_unref(pool);
            pool = NULL;
        }
    }

    if (pool) {
        // Add our pool to the allocation query
        // Upstream sources will use this pool to get dmabuf-backed buffers
        gst_query_add_allocation_pool(query,
                                      pool,
                                      info.size,
                                      4,                   // min
                                      vslsink->pool_size); // max
        gst_object_unref(pool);

        GST_INFO_OBJECT(vslsink,
                        "proposed dmabuf buffer pool: %zu x %zu bytes",
                        vslsink->pool_size,
                        (gsize) info.size);
    } else {
        GST_WARNING_OBJECT(vslsink,
                           "dmabuf pool creation failed, "
                           "upstream will use default allocator");
    }

    // Add video meta option so upstream knows we support it
    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

    return TRUE;
}

static gboolean
start(GstBaseSink* sink)
{
    VslSink* vslsink = VSL_SINK(sink);

    GstClock* clock = gst_element_get_clock(GST_ELEMENT(sink));
    if (clock) { vslsink->last_frame = gst_clock_get_time(clock); }

    if (!vslsink->path) {
        gchar* name;
        g_object_get(vslsink, "name", &name, NULL);
        vslsink->path =
            g_strdup_printf("/tmp/%s.%ld", name, syscall(SYS_gettid));
        g_free(name);
    }

    GST_INFO_OBJECT(vslsink, "creating vsl host on %s", vslsink->path);

    vslsink->host = vsl_host_init(vslsink->path);
    if (!vslsink->host) {
        GST_ERROR_OBJECT(vslsink,
                         "failed to initialize vsl host: %s",
                         strerror(errno));
        return FALSE;
    }

    gst_task_start(vslsink->task);

    return TRUE;
}

static gboolean
stop(GstBaseSink* sink)
{
    VslSink* vslsink = VSL_SINK(sink);
    gst_task_stop(vslsink->task);
    return TRUE;
}

// Allocate a single DMA buffer entry
static gboolean
dmabuf_pool_alloc_entry(DmaBufPoolEntry* entry, size_t size)
{
    int heap_fd = open(DMA_HEAP_PATH, O_RDWR);
    if (heap_fd < 0) {
        heap_fd = open(DMA_HEAP_PATH_ALT, O_RDWR);
        if (heap_fd < 0) { return FALSE; }
    }

    struct dma_heap_allocation_data alloc = {
        .len      = size,
        .fd_flags = O_CLOEXEC | O_RDWR,
    };

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
        close(heap_fd);
        return FALSE;
    }
    close(heap_fd);

    void* map_ptr =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
    if (map_ptr == MAP_FAILED) {
        close(alloc.fd);
        return FALSE;
    }

    entry->dmabuf_fd = alloc.fd;
    entry->map_ptr   = map_ptr;
    entry->map_size  = size;
    entry->in_use    = false;

    return TRUE;
}

// Initialize the DMA buffer pool with pre-allocated buffers
static gboolean
dmabuf_pool_init(VslSink* vslsink, size_t buffer_size)
{
    DmaBufPool* pool = &vslsink->dmabuf_pool;

    g_mutex_lock(&pool->lock);

    if (pool->initialized) {
        // Already initialized - check if size matches
        if (pool->count > 0 && pool->entries[0].map_size == buffer_size) {
            g_mutex_unlock(&pool->lock);
            return TRUE;
        }
        // Size changed - need to reinitialize (shouldn't happen normally)
        GST_WARNING_OBJECT(vslsink,
                           "buffer size changed from %zu to %zu, "
                           "reinitializing pool",
                           pool->entries[0].map_size,
                           buffer_size);
        g_mutex_unlock(&pool->lock);
        dmabuf_pool_free(vslsink);
        g_mutex_lock(&pool->lock);
    }

    pool->count   = vslsink->pool_size;
    pool->entries = g_new0(DmaBufPoolEntry, pool->count);

    GST_INFO_OBJECT(vslsink,
                    "allocating DMA buffer pool: %zu entries x %zu bytes",
                    pool->count,
                    buffer_size);

    for (size_t i = 0; i < pool->count; i++) {
        if (!dmabuf_pool_alloc_entry(&pool->entries[i], buffer_size)) {
            GST_ERROR_OBJECT(vslsink,
                             "failed to allocate pool entry %zu: %s",
                             i,
                             strerror(errno));
            // Clean up partially allocated pool
            for (size_t j = 0; j < i; j++) {
                if (pool->entries[j].map_ptr) {
                    munmap(pool->entries[j].map_ptr, pool->entries[j].map_size);
                }
                if (pool->entries[j].dmabuf_fd >= 0) {
                    close(pool->entries[j].dmabuf_fd);
                }
            }
            g_free(pool->entries);
            pool->entries = NULL;
            pool->count   = 0;
            g_mutex_unlock(&pool->lock);
            return FALSE;
        }
        GST_DEBUG_OBJECT(vslsink,
                         "allocated pool entry %zu: fd=%d",
                         i,
                         pool->entries[i].dmabuf_fd);
    }

    pool->next_idx    = 0;
    pool->initialized = TRUE;

    g_mutex_unlock(&pool->lock);
    return TRUE;
}

// Acquire a buffer from the pool (round-robin with fallback)
static DmaBufPoolEntry*
dmabuf_pool_acquire(VslSink* vslsink)
{
    DmaBufPool* pool = &vslsink->dmabuf_pool;

    g_mutex_lock(&pool->lock);

    if (!pool->initialized || pool->count == 0) {
        g_mutex_unlock(&pool->lock);
        return NULL;
    }

    // Try round-robin starting from next_idx
    for (size_t attempts = 0; attempts < pool->count; attempts++) {
        size_t           idx   = (pool->next_idx + attempts) % pool->count;
        DmaBufPoolEntry* entry = &pool->entries[idx];

        if (!entry->in_use) {
            entry->in_use  = true;
            pool->next_idx = (idx + 1) % pool->count;
            GST_LOG_OBJECT(vslsink,
                           "acquired pool entry %zu: fd=%d",
                           idx,
                           entry->dmabuf_fd);
            g_mutex_unlock(&pool->lock);
            return entry;
        }
    }

    // All buffers in use - this shouldn't happen if pool is sized correctly
    GST_WARNING_OBJECT(vslsink,
                       "all %zu pool buffers in use, frames may be leaking",
                       pool->count);
    g_mutex_unlock(&pool->lock);
    return NULL;
}

// Release a buffer back to the pool
static void
dmabuf_pool_release(VslSink* vslsink, DmaBufPoolEntry* entry)
{
    DmaBufPool* pool = &vslsink->dmabuf_pool;

    g_mutex_lock(&pool->lock);

    if (entry) {
        entry->in_use = false;
        GST_LOG_OBJECT(vslsink, "released pool entry: fd=%d", entry->dmabuf_fd);
    }

    g_mutex_unlock(&pool->lock);
}

// Cleanup callback for pooled DMA buffers - returns buffer to pool
static void
dmabuf_pool_cleanup(VSLFrame* frame)
{
    DmaBufPoolRef* ref = vsl_frame_userptr(frame);
    if (ref) {
        GST_TRACE("pool cleanup: frame=%p serial=%ld fd=%d",
                  frame,
                  vsl_frame_serial(frame),
                  ref->entry ? ref->entry->dmabuf_fd : -1);
        dmabuf_pool_release(ref->vslsink, ref->entry);
        g_free(ref);
    }

    // Close the dup'd fd created by vsl_frame_attach
    // (vsl_frame_unalloc skips this when cleanup callback exists)
    int handle = vsl_frame_handle(frame);
    if (handle >= 0) { close(handle); }
}

// Copy data into a pooled buffer
static int
dmabuf_pool_copy(VslSink*        vslsink,
                 size_t          size,
                 const void*     src_data,
                 DmaBufPoolRef** out_ref)
{
    // Initialize pool on first use (now we know the frame size)
    if (!vslsink->dmabuf_pool.initialized) {
        if (!dmabuf_pool_init(vslsink, size)) {
            GST_ERROR_OBJECT(vslsink,
                             "failed to initialize DMA buffer pool. "
                             "Ensure %s or %s exists",
                             DMA_HEAP_PATH,
                             DMA_HEAP_PATH_ALT);
            return -1;
        }
    }

    DmaBufPoolEntry* entry = dmabuf_pool_acquire(vslsink);
    if (!entry) {
        GST_ERROR_OBJECT(vslsink,
                         "no available buffers in pool (size=%zu). "
                         "Consider increasing pool-size property",
                         vslsink->pool_size);
        errno = ENOMEM;
        return -1;
    }

    // Verify size matches
    if (entry->map_size < size) {
        GST_ERROR_OBJECT(vslsink,
                         "buffer size mismatch: need %zu, have %zu",
                         size,
                         entry->map_size);
        dmabuf_pool_release(vslsink, entry);
        errno = EINVAL;
        return -1;
    }

    // Sync for CPU write and copy data
    struct dma_buf_sync sync = {.flags =
                                    DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE};
    ioctl(entry->dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);

    memcpy(entry->map_ptr, src_data, size);

    sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE;
    ioctl(entry->dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);

    // Create reference for cleanup callback
    DmaBufPoolRef* ref = g_new0(DmaBufPoolRef, 1);
    ref->vslsink       = vslsink;
    ref->entry         = entry;

    *out_ref = ref;
    return entry->dmabuf_fd;
}

static void
frame_cleanup(VSLFrame* frame)
{
    GstMemory* memory = vsl_frame_userptr(frame);
    GST_TRACE("%p serial:%ld timestamp:%ld expires:%ld now:%ld",
              frame,
              vsl_frame_serial(frame),
              vsl_frame_timestamp(frame),
              vsl_frame_expires(frame),
              vsl_timestamp());
    gst_memory_unref(memory);

    // Close the dup'd fd created by vsl_frame_attach
    // (vsl_frame_unalloc skips this when cleanup callback exists)
    int handle = vsl_frame_handle(frame);
    if (handle >= 0) { close(handle); }
}

static GstFlowReturn
show_frame(GstVideoSink* sink, GstBuffer* buffer)
{
    VslSink*       vslsink = VSL_SINK(sink);
    int            width, height;
    gchar*         framerate;
    const char*    format;
    uint32_t       fourcc;
    GstMemory*     memory;
    GstCaps*       caps;
    GstStructure*  structure;
    GstVideoFormat videoformat;
    gboolean       is_dmabuf;
    int            fd;
    size_t         offset, size;
    DmaBufPoolRef* pool_ref     = NULL;
    void*          cleanup_func = frame_cleanup;
    void*          cleanup_data = NULL;

    vslsink->frame_number++;
    GST_TRACE_OBJECT(vslsink, "frame_number:%ld", vslsink->frame_number);

    memory    = gst_buffer_get_all_memory(buffer);
    is_dmabuf = gst_is_dmabuf_memory(memory);

    if (is_dmabuf) {
        // Zero-copy path: use DMA-BUF directly
        fd = gst_dmabuf_memory_get_fd(memory);
        gst_memory_get_sizes(memory, &offset, &size);
        cleanup_func = frame_cleanup;
        cleanup_data = memory;
        GST_LOG_OBJECT(vslsink, "using zero-copy dmabuf fd:%d", fd);
    } else {
        // Fallback path: copy system memory to pooled DMA-BUF
        GstMapInfo map;
        if (!gst_memory_map(memory, &map, GST_MAP_READ)) {
            GST_ERROR_OBJECT(vslsink, "failed to map system memory");
            gst_memory_unref(memory);
            return GST_FLOW_ERROR;
        }

        fd = dmabuf_pool_copy(vslsink, map.size, map.data, &pool_ref);
        gst_memory_unmap(memory, &map);
        gst_memory_unref(memory);

        if (fd < 0) {
            GST_ERROR_OBJECT(vslsink,
                             "failed to acquire pooled dmabuf: %s. "
                             "Ensure %s or %s exists and pool-size is adequate",
                             strerror(errno),
                             DMA_HEAP_PATH,
                             DMA_HEAP_PATH_ALT);
            return GST_FLOW_ERROR;
        }

        offset       = 0;
        size         = pool_ref->entry->map_size;
        cleanup_func = dmabuf_pool_cleanup;
        cleanup_data = pool_ref;
        GST_LOG_OBJECT(vslsink,
                       "copied system memory to pooled dmabuf fd:%d size:%zu",
                       fd,
                       size);
    }

    caps      = gst_pad_get_current_caps(GST_VIDEO_SINK_PAD(sink));
    structure = gst_caps_get_structure(caps, 0);

    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    framerate =
        gst_value_serialize(gst_structure_get_value(structure, "framerate"));

    format      = gst_structure_get_string(structure, "format");
    videoformat = gst_video_format_from_string(format);
    fourcc      = gst_video_format_to_fourcc(videoformat);

    if (!fourcc) {
        switch (videoformat) {
        case GST_VIDEO_FORMAT_RGB:
            fourcc = GST_MAKE_FOURCC('R', 'G', 'B', '3');
            break;
        case GST_VIDEO_FORMAT_BGR:
            fourcc = GST_MAKE_FOURCC('B', 'G', 'R', '3');
            break;
        case GST_VIDEO_FORMAT_RGBA:
            fourcc = GST_MAKE_FOURCC('R', 'G', 'B', 'A');
            break;
        case GST_VIDEO_FORMAT_BGRA:
            fourcc = GST_MAKE_FOURCC('B', 'G', 'R', 'A');
            break;
        default:
            GST_WARNING_OBJECT(vslsink,
                               "format %s has no fourcc code - leaving empty",
                               format);
            break;
        }
    }

    GstClockTime now   = 0;
    GstClock*    clock = gst_element_get_clock(GST_ELEMENT(sink));
    if (clock) { now = gst_clock_get_time(clock); }

    GST_LOG_OBJECT(vslsink,
                   "dmabuf fd:%d size:%zu offset:%zu %dx%d framerate=%s %s "
                   "fourcc:%c%c%c%c frame:%ld " GST_STIME_FORMAT,
                   fd,
                   size,
                   offset,
                   width,
                   height,
                   framerate,
                   format,
                   (char) fourcc,
                   (char) (fourcc >> 8),
                   (char) (fourcc >> 16),
                   (char) (fourcc >> 24),
                   vslsink->frame_number,
                   GST_STIME_ARGS(GST_CLOCK_DIFF(vslsink->last_frame, now)));
    vslsink->last_frame = now;

    g_free(framerate);

    while (1) {
        if (vsl_host_process(vslsink->host) == 0) {
            break;
        } else {
            if (errno != ETIMEDOUT) {
                GST_ERROR_OBJECT(vslsink,
                                 "vsl host processing error: %s",
                                 strerror(errno));
                // Clean up based on which path we took
                if (is_dmabuf) {
                    gst_memory_unref(memory);
                } else if (pool_ref) {
                    dmabuf_pool_release(vslsink, pool_ref->entry);
                    g_free(pool_ref);
                }
                return GST_FLOW_ERROR;
            }
        }
    }

    int64_t duration = GST_BUFFER_DURATION(buffer);
    int64_t pts      = GST_BUFFER_PTS(buffer);
    int64_t dts      = GST_BUFFER_DTS(buffer);
    int64_t serial   = GST_BUFFER_OFFSET(buffer);

    GST_LOG_OBJECT(vslsink,
                   "FRAME:%ld PTS:%ld DTS:%ld DURATION:%lu\n",
                   serial,
                   pts,
                   dts,
                   duration);

    gst_task_pause(vslsink->task);
    // g_rec_mutex_lock(&vslsink->mutex);
    VSLFrame* frame = vsl_frame_register(vslsink->host,
                                         serial,
                                         fd,
                                         width,
                                         height,
                                         fourcc,
                                         size,
                                         offset,
                                         vsl_timestamp() + vslsink->lifespan *
                                                               NANOS_PER_MILLI,
                                         duration,
                                         pts,
                                         dts,
                                         cleanup_func,
                                         cleanup_data);
    // g_rec_mutex_unlock(&vslsink->mutex);
    gst_task_start(vslsink->task);

    if (!frame) {
        // Clean up based on which path we took
        if (is_dmabuf) {
            gst_memory_unref(memory);
        } else if (pool_ref) {
            dmabuf_pool_release(vslsink, pool_ref->entry);
            g_free(pool_ref);
        }
        GST_ERROR_OBJECT(vslsink,
                         "vsl frame register error: %s",
                         strerror(errno));
        return GST_FLOW_ERROR;
    }

    GST_TRACE_OBJECT(vslsink, "frame %ld broadcast", vsl_frame_serial(frame));

    return GST_FLOW_OK;
}

static void
vsl_sink_class_init(VslSinkClass* klass)
{
    GObjectClass*      gobject_class    = G_OBJECT_CLASS(klass);
    GstBaseSinkClass*  base_sink_class  = GST_BASE_SINK_CLASS(klass);
    GstVideoSinkClass* video_sink_class = GST_VIDEO_SINK_CLASS(klass);

    gst_element_class_add_pad_template(
        GST_ELEMENT_CLASS(klass),
        gst_pad_template_new("sink",
                             GST_PAD_SINK,
                             GST_PAD_ALWAYS,
                             gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "VideoStream Sink",
                                          "Sink/Video",
                                          "DMA-enabled cross-process GStreamer "
                                          "pipeline",
                                          "Au-Zone Technologies "
                                          "<info@au-zone.com>");

    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;
    gobject_class->dispose      = dispose;
    gobject_class->finalize     = finalize;

    base_sink_class->propose_allocation = propose_allocation;
    base_sink_class->start              = start;
    base_sink_class->stop               = stop;

    video_sink_class->show_frame = show_frame;

    properties[PROP_PATH] =
        g_param_spec_string("path",
                            "Path",
                            "Path to the VideoStream socket",
                            NULL,
                            G_PARAM_READWRITE);

    properties[PROP_LIFESPAN] =
        g_param_spec_int64("lifespan",
                           "lifespan",
                           "The lifespan of unlocked frames in milliseconds",
                           0,
                           INT64_MAX,
                           DEFAULT_LIFESPAN,
                           G_PARAM_READWRITE);

    properties[PROP_POOL_SIZE] =
        g_param_spec_uint("pool-size",
                          "Pool Size",
                          "Number of pre-allocated DMA buffers for system "
                          "memory "
                          "copy (only used when upstream doesn't provide "
                          "dmabuf)",
                          1,
                          64,
                          DEFAULT_POOL_SIZE,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, properties);
}
