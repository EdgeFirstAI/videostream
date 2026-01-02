# V4L2 Codec Implementation Design

**Document Version**: 1.1
**Date**: 2025-01-01
**Author**: Sebastien Taylor
**Status**: Validated - Benchmarks Complete

---

## Executive Summary

This document describes the design for adding V4L2-based encoder and decoder support to the VideoStream library as an alternative to the current Hantro user-space codec (libcodec.so). The V4L2 path uses the Linux kernel's V4L2 mem2mem interface to access the same Hantro VPU hardware but through a more portable and potentially faster path.

### Motivation

- **Performance**: Benchmarks confirm V4L2 decoder is **37-56x faster** than libcodec.so across all resolutions
- **Stability**: libcodec.so encoder crashes with SIGSEGV; V4L2 encoder is stable
- **Portability**: V4L2 is a standard Linux API, reducing dependency on proprietary binaries
- **Maintainability**: Kernel drivers are actively maintained by NXP and the community
- **Zero-copy**: V4L2 DMABUF support enables efficient buffer sharing

---

## Hardware Overview

### i.MX 8M Plus VPU Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        i.MX 8M Plus SoC                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐          ┌─────────────────┐              │
│  │   Hantro G1     │          │   Hantro G2     │              │
│  │   (Decoder)     │          │   (Decoder)     │              │
│  │                 │          │                 │              │
│  │  - H.264        │          │  - HEVC/H.265   │              │
│  │  - VP8          │          │  - VP9          │              │
│  │  - MPEG-2/4     │          │                 │              │
│  │  - VC-1         │          │                 │              │
│  └────────┬────────┘          └────────┬────────┘              │
│           │                            │                        │
│           └────────────┬───────────────┘                        │
│                        │                                        │
│                        ▼                                        │
│           ┌────────────────────────┐                           │
│           │     vsi_v4l2dec        │ ◄── /dev/video1           │
│           │   (Kernel Driver)      │                           │
│           └────────────────────────┘                           │
│                                                                 │
│  ┌─────────────────────────────────┐                           │
│  │      Hantro VC8000E             │                           │
│  │        (Encoder)                │                           │
│  │                                 │                           │
│  │  - H.264 up to 1080p60          │                           │
│  │  - HEVC up to 1080p60           │                           │
│  │  - JPEG                         │                           │
│  └────────────────┬────────────────┘                           │
│                   │                                             │
│                   ▼                                             │
│           ┌────────────────────────┐                           │
│           │     vsi_v4l2enc        │ ◄── /dev/video0           │
│           │   (Kernel Driver)      │                           │
│           └────────────────────────┘                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### V4L2 Device Nodes

| Device | Driver | Function | Max Resolution |
|--------|--------|----------|----------------|
| `/dev/video0` | vsi_v4l2enc | H.264/HEVC Encoder | 1920x1080 @ 60fps |
| `/dev/video1` | vsi_v4l2dec | H.264/HEVC/VP9 Decoder | 4096x2160 @ 60fps |

---

## Current Architecture (libcodec.so)

```
┌──────────────────────────────────────────────────────────────────┐
│                      User Space                                   │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────┐    ┌─────────────────┐    ┌─────────────────┐  │
│  │ VideoStream │    │  vpu_wrapper    │    │  libcodec.so    │  │
│  │   Library   │───►│  hantro.c       │───►│  (Binary Blob)  │  │
│  └─────────────┘    └─────────────────┘    └─────────────────┘  │
│                                                    │              │
│                                                    ▼              │
│                                            ┌─────────────────┐   │
│                                            │  DWL (Device    │   │
│                                            │  Wrapper Layer) │   │
│                                            └────────┬────────┘   │
│                                                     │             │
├─────────────────────────────────────────────────────┼─────────────┤
│                      Kernel Space                   │             │
├─────────────────────────────────────────────────────┼─────────────┤
│                                                     ▼             │
│                                            ┌─────────────────┐   │
│                                            │  /dev/hantrodec │   │
│                                            │  /dev/hantroenc │   │
│                                            └─────────────────┘   │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘

Issues:
- libcodec.so is a proprietary binary blob
- 34-151ms decode latency (vs 0.8-4ms with V4L2)
- Encoder crashes with SIGSEGV
- Limited debugging capability
- Version compatibility issues
```

---

## Proposed Architecture (V4L2)

```
┌──────────────────────────────────────────────────────────────────┐
│                      User Space                                   │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────┐    ┌─────────────────┐                          │
│  │ VideoStream │    │  encoder_v4l2.c │                          │
│  │   Library   │───►│  decoder_v4l2.c │                          │
│  └─────────────┘    └────────┬────────┘                          │
│                              │                                    │
│                              │ V4L2 ioctl() + DMABUF              │
│                              │                                    │
├──────────────────────────────┼────────────────────────────────────┤
│                      Kernel Space                                 │
├──────────────────────────────┼────────────────────────────────────┤
│                              ▼                                    │
│                     ┌─────────────────┐                          │
│                     │   vsi_v4l2      │                          │
│                     │  (Kernel Driver)│                          │
│                     └────────┬────────┘                          │
│                              │                                    │
│                              ▼                                    │
│                     ┌─────────────────┐                          │
│                     │  Hantro VPU HW  │                          │
│                     └─────────────────┘                          │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘

Benefits:
- Standard Linux API (portable)
- Kernel-managed interrupt handling
- DMABUF for zero-copy buffer sharing
- Active driver maintenance
- 37-56x faster decode across all resolutions
- Stable encoder (no crashes)
```

---

## V4L2 mem2mem Concept

V4L2 mem2mem (memory-to-memory) is a framework for hardware codecs that process data from one buffer to another:

```
            ┌─────────────────────────────────────────┐
            │           V4L2 mem2mem Device           │
            │                                         │
 Encoded    │  ┌─────────┐         ┌─────────────┐   │   Decoded
  Data  ───►│  │ OUTPUT  │ ──────► │   CAPTURE   │   │──► Frames
            │  │ Queue   │  (HW)   │    Queue    │   │
            │  └─────────┘         └─────────────┘   │
            │                                         │
            └─────────────────────────────────────────┘

OUTPUT Queue: Source data (compressed for decoder, raw for encoder)
CAPTURE Queue: Destination data (raw for decoder, compressed for encoder)
```

### Buffer Flow

```
1. Application queues buffer to OUTPUT  (VIDIOC_QBUF)
2. Hardware processes data
3. Application dequeues from CAPTURE    (VIDIOC_DQBUF)
4. Application returns buffer to OUTPUT (VIDIOC_QBUF)
```

---

## Decoder Implementation Design

### File: `lib/decoder_v4l2.c`

#### Data Structures

```c
typedef struct {
    int fd;                          // V4L2 device fd

    // OUTPUT queue (compressed data)
    struct {
        int count;
        size_t size;
        void *mmap[MAX_OUTPUT_BUFFERS];
        int queued[MAX_OUTPUT_BUFFERS];
    } output;

    // CAPTURE queue (decoded frames)
    struct {
        int count;
        int dmabuf_fds[MAX_CAPTURE_BUFFERS];
        imx_physical_address_t phys[MAX_CAPTURE_BUFFERS];
        VSLFrame *frames[MAX_CAPTURE_BUFFERS];
    } capture;

    // Stream info
    int width;
    int height;
    uint32_t pixelformat;
    bool streaming;

    // Statistics
    uint64_t frames_decoded;
    uint64_t total_decode_time_us;
} VSLDecoderV4L2;
```

#### Initialization Sequence

```c
VSLDecoder* vsl_decoder_create_v4l2(uint32_t codec, int fps)
{
    // 1. Open device
    int fd = open("/dev/video1", O_RDWR | O_NONBLOCK);

    // 2. Query capabilities
    struct v4l2_capability cap;
    ioctl(fd, VIDIOC_QUERYCAP, &cap);
    // Verify V4L2_CAP_VIDEO_M2M or V4L2_CAP_VIDEO_M2M_MPLANE

    // 3. Set OUTPUT format (compressed)
    struct v4l2_format fmt = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .fmt.pix_mp = {
            .pixelformat = V4L2_PIX_FMT_H264,  // or V4L2_PIX_FMT_HEVC
            .plane_fmt[0].sizeimage = 2 * 1024 * 1024,  // 2MB per buffer
        }
    };
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    // 4. Request OUTPUT buffers
    struct v4l2_requestbuffers req = {
        .count = NUM_OUTPUT_BUFFERS,
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .memory = V4L2_MEMORY_MMAP,
    };
    ioctl(fd, VIDIOC_REQBUFS, &req);

    // 5. mmap OUTPUT buffers
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = { .type = req.type, .index = i };
        ioctl(fd, VIDIOC_QUERYBUF, &buf);
        mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    }

    // Note: CAPTURE queue setup happens after resolution is known
    // (after first decoded frame or SOURCE_CHANGE event)
}
```

#### Decode Loop

```c
int vsl_decode_frame_v4l2(VSLDecoder *dec, const void *data,
                          unsigned int len, size_t *consumed,
                          VSLFrame **output)
{
    // 1. Find free OUTPUT buffer
    int out_idx = find_free_output_buffer(dec);

    // 2. Copy compressed data to OUTPUT buffer
    memcpy(dec->output.mmap[out_idx], data, len);

    // 3. Queue OUTPUT buffer
    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .index = out_idx,
        .memory = V4L2_MEMORY_MMAP,
    };
    buf.m.planes[0].bytesused = len;
    ioctl(dec->fd, VIDIOC_QBUF, &buf);

    // 4. Start streaming if not already
    if (!dec->streaming) {
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        ioctl(dec->fd, VIDIOC_STREAMON, &type);
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        ioctl(dec->fd, VIDIOC_STREAMON, &type);
        dec->streaming = true;
    }

    // 5. Poll for completion
    struct pollfd pfd = { .fd = dec->fd, .events = POLLIN };
    poll(&pfd, 1, timeout_ms);

    // 6. Dequeue CAPTURE buffer (decoded frame)
    struct v4l2_buffer cap_buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        .memory = V4L2_MEMORY_DMABUF,
    };
    if (ioctl(dec->fd, VIDIOC_DQBUF, &cap_buf) == 0) {
        *output = dec->capture.frames[cap_buf.index];
        *consumed = len;
        return VSL_DEC_FRAME_DEC;
    }

    return VSL_DEC_SUCCESS;
}
```

#### Resolution Change Handling

```c
// V4L2 SOURCE_CHANGE event indicates resolution/format change
void handle_source_change(VSLDecoderV4L2 *dec)
{
    // 1. Stop CAPTURE streaming
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(dec->fd, VIDIOC_STREAMOFF, &type);

    // 2. Get new format
    struct v4l2_format fmt = { .type = type };
    ioctl(dec->fd, VIDIOC_G_FMT, &fmt);
    dec->width = fmt.fmt.pix_mp.width;
    dec->height = fmt.fmt.pix_mp.height;

    // 3. Request new CAPTURE buffers with DMABUF
    struct v4l2_requestbuffers req = {
        .count = NUM_CAPTURE_BUFFERS,
        .type = type,
        .memory = V4L2_MEMORY_DMABUF,
    };
    ioctl(dec->fd, VIDIOC_REQBUFS, &req);

    // 4. Allocate DMA buffers and queue them
    for (int i = 0; i < req.count; i++) {
        // Allocate from DMA heap
        dec->capture.frames[i] = vsl_frame_alloc_dma(dec->width, dec->height);

        // Queue buffer
        struct v4l2_buffer buf = {
            .type = type,
            .index = i,
            .memory = V4L2_MEMORY_DMABUF,
        };
        buf.m.planes[0].m.fd = vsl_frame_dma_fd(dec->capture.frames[i]);
        ioctl(dec->fd, VIDIOC_QBUF, &buf);
    }

    // 5. Restart CAPTURE streaming
    ioctl(dec->fd, VIDIOC_STREAMON, &type);
}
```

---

## Encoder Implementation Design

### File: `lib/encoder_v4l2.c`

#### Data Structures

```c
typedef struct {
    int fd;                          // V4L2 device fd

    // OUTPUT queue (raw frames)
    struct {
        int count;
        int dmabuf_fds[MAX_OUTPUT_BUFFERS];
    } output;

    // CAPTURE queue (compressed data)
    struct {
        int count;
        size_t size;
        void *mmap[MAX_CAPTURE_BUFFERS];
    } capture;

    // Encoder settings
    int width;
    int height;
    int fps;
    int bitrate;
    int gop_size;

    // Crop region (for 4K tiling)
    struct {
        int x, y, w, h;
    } crop;

    // Statistics
    uint64_t frames_encoded;
    uint64_t total_encode_time_us;
    uint64_t total_bytes;
} VSLEncoderV4L2;
```

#### Initialization Sequence

```c
VSLEncoder* vsl_encoder_create_v4l2(uint32_t profile, uint32_t codec, int fps)
{
    // 1. Open device
    int fd = open("/dev/video0", O_RDWR | O_NONBLOCK);

    // 2. Set OUTPUT format (raw frames - NV12)
    struct v4l2_format fmt = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .fmt.pix_mp = {
            .width = width,
            .height = height,
            .pixelformat = V4L2_PIX_FMT_NV12,
            .num_planes = 1,
        }
    };
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    // 3. Set CAPTURE format (compressed)
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    // 4. Set encoder controls
    struct v4l2_ext_controls ctrls = { ... };
    // V4L2_CID_MPEG_VIDEO_BITRATE
    // V4L2_CID_MPEG_VIDEO_GOP_SIZE
    // V4L2_CID_MPEG_VIDEO_H264_PROFILE
    // V4L2_CID_MPEG_VIDEO_H264_LEVEL
    ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);

    // 5. Set crop region (for 4K tiling support)
    struct v4l2_selection sel = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .target = V4L2_SEL_TGT_CROP,
        .r = { .left = crop_x, .top = crop_y,
               .width = crop_w, .height = crop_h }
    };
    ioctl(fd, VIDIOC_S_SELECTION, &sel);

    // 6. Request buffers (DMABUF for input, MMAP for output)
    // ...
}
```

#### Encode Frame

```c
int vsl_encode_frame_v4l2(VSLEncoder *enc, const VSLFrame *input,
                          VSLFrame *output, int *keyframe)
{
    // 1. Queue input frame (DMABUF)
    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .index = find_free_buffer(),
        .memory = V4L2_MEMORY_DMABUF,
    };
    buf.m.planes[0].m.fd = vsl_frame_dma_fd(input);
    buf.m.planes[0].bytesused = vsl_frame_size(input);
    ioctl(enc->fd, VIDIOC_QBUF, &buf);

    // 2. Queue output buffer (MMAP)
    struct v4l2_buffer out_buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        .index = find_free_capture_buffer(),
        .memory = V4L2_MEMORY_MMAP,
    };
    ioctl(enc->fd, VIDIOC_QBUF, &out_buf);

    // 3. Poll for completion
    struct pollfd pfd = { .fd = enc->fd, .events = POLLIN };
    poll(&pfd, 1, timeout_ms);

    // 4. Dequeue compressed output
    ioctl(enc->fd, VIDIOC_DQBUF, &out_buf);

    // 5. Copy to output frame
    size_t encoded_size = out_buf.m.planes[0].bytesused;
    memcpy(vsl_frame_data(output), enc->capture.mmap[out_buf.index], encoded_size);

    // 6. Check if keyframe
    *keyframe = (out_buf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0;

    // 7. Return input buffer
    ioctl(enc->fd, VIDIOC_DQBUF, &buf);  // OUTPUT queue

    return encoded_size;
}
```

---

## 4K Tiling Support

The encoder hardware (VC8000E) supports maximum 1920x1080. For 4K content, the application must:

### Tiling Strategy

```
┌─────────────────────────────────────────┐
│            4K Source (3840x2160)        │
│                                         │
│  ┌─────────────────┬─────────────────┐  │
│  │                 │                 │  │
│  │   Tile 0        │   Tile 1        │  │
│  │   (Top-Left)    │   (Top-Right)   │  │
│  │   1920x1080     │   1920x1080     │  │
│  │                 │                 │  │
│  ├─────────────────┼─────────────────┤  │
│  │                 │                 │  │
│  │   Tile 2        │   Tile 3        │  │
│  │   (Bot-Left)    │   (Bot-Right)   │  │
│  │   1920x1080     │   1920x1080     │  │
│  │                 │                 │  │
│  └─────────────────┴─────────────────┘  │
│                                         │
└─────────────────────────────────────────┘
```

### V4L2 Crop-Based Tiling

```c
// Create encoder with crop for each tile
for (int tile = 0; tile < 4; tile++) {
    VSLEncoder *enc = vsl_encoder_create_v4l2(...);

    // Calculate crop region
    int crop_x = (tile % 2) * 1920;
    int crop_y = (tile / 2) * 1080;

    // Set V4L2 crop selection
    struct v4l2_selection sel = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .target = V4L2_SEL_TGT_CROP,
        .r = { .left = crop_x, .top = crop_y,
               .width = 1920, .height = 1080 }
    };
    ioctl(enc->fd, VIDIOC_S_SELECTION, &sel);
}
```

---

## API Design

### Public API (Compatible with existing API)

```c
// Decoder
VSLDecoder* vsl_decoder_create(uint32_t codec, int fps);
VSLDecoderRetCode vsl_decode_frame(VSLDecoder *dec, const void *data,
                                   unsigned int len, size_t *consumed,
                                   VSLFrame **output);
int vsl_decoder_width(const VSLDecoder *dec);
int vsl_decoder_height(const VSLDecoder *dec);
int vsl_decoder_release(VSLDecoder *dec);

// Encoder (existing API unchanged)
VSLEncoder* vsl_encoder_create(VSLEncoderProfile profile,
                               uint32_t codec, int fps);
int vsl_encoder_frame(VSLEncoder *enc, const VSLFrame *input,
                      VSLFrame *output, const VSLRect *crop,
                      int *keyframe);
int vsl_encoder_release(VSLEncoder *enc);
```

### Backend Selection

The library supports both V4L2 and Hantro backends with runtime selection and automatic fallback.

#### Backend Enum

```c
typedef enum {
    VSL_CODEC_BACKEND_AUTO,     // Auto-detect best backend (default)
    VSL_CODEC_BACKEND_HANTRO,   // Force libcodec.so/vpu_wrapper
    VSL_CODEC_BACKEND_V4L2,     // Force V4L2 kernel driver
} VSLCodecBackend;
```

#### Extended API

```c
// Extended create functions with explicit backend selection
VSLDecoder* vsl_decoder_create_ex(uint32_t codec, int fps,
                                   VSLCodecBackend backend);
VSLEncoder* vsl_encoder_create_ex(VSLEncoderProfile profile,
                                   uint32_t codec, int fps,
                                   VSLCodecBackend backend);

// Original API uses AUTO backend (backwards compatible)
// vsl_decoder_create() calls vsl_decoder_create_ex(..., VSL_CODEC_BACKEND_AUTO)
```

#### Environment Variable Override

```c
// Environment variable: VSL_CODEC_BACKEND
// Values: "auto", "v4l2", "hantro"
//
// Example usage:
//   VSL_CODEC_BACKEND=hantro ./my_app   # Force Hantro even if V4L2 available
//   VSL_CODEC_BACKEND=v4l2 ./my_app     # Force V4L2 (fail if unavailable)
//   VSL_CODEC_BACKEND=auto ./my_app     # Auto-detect (default)

#define VSL_CODEC_BACKEND_ENV "VSL_CODEC_BACKEND"
```

#### Auto-Detection Logic

```c
// Backend selection priority (for VSL_CODEC_BACKEND_AUTO):
//
// 1. Check VSL_CODEC_BACKEND environment variable first
//    - If set to "hantro", use Hantro backend
//    - If set to "v4l2", use V4L2 backend (fail if unavailable)
//    - If set to "auto" or unset, continue with auto-detection
//
// 2. Check V4L2 device availability
//    - Decoder: /dev/video1 (vsi_v4l2dec)
//    - Encoder: /dev/video0 (vsi_v4l2enc)
//    - If available and accessible, prefer V4L2
//
// 3. Fallback to Hantro if V4L2 unavailable
//    - Check /dev/hantrodec (decoder) or /dev/hantroenc (encoder)
//    - Use libcodec.so via vpu_wrapper
//
// 4. Return NULL if no backend available

static VSLCodecBackend vsl_detect_backend(bool is_encoder)
{
    // 1. Check environment variable override
    const char *env = getenv(VSL_CODEC_BACKEND_ENV);
    if (env) {
        if (strcasecmp(env, "hantro") == 0) {
            return VSL_CODEC_BACKEND_HANTRO;
        } else if (strcasecmp(env, "v4l2") == 0) {
            return VSL_CODEC_BACKEND_V4L2;
        }
        // "auto" or unknown value falls through to detection
    }

    // 2. Check V4L2 device availability (preferred)
    const char *v4l2_dev = is_encoder ? "/dev/video0" : "/dev/video1";
    if (access(v4l2_dev, R_OK | W_OK) == 0) {
        // Verify it's the correct device type by querying capabilities
        int fd = open(v4l2_dev, O_RDWR | O_NONBLOCK);
        if (fd >= 0) {
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                if (cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) {
                    close(fd);
                    return VSL_CODEC_BACKEND_V4L2;
                }
            }
            close(fd);
        }
    }

    // 3. Fallback: Check Hantro device availability
    const char *hantro_dev = is_encoder ? "/dev/hantroenc" : "/dev/hantrodec";
    if (access(hantro_dev, R_OK | W_OK) == 0) {
        return VSL_CODEC_BACKEND_HANTRO;
    }

    // 4. No backend available - AUTO will fail at create time
    return VSL_CODEC_BACKEND_AUTO;
}
```

#### Implementation Pattern

```c
// lib/decoder.c - Unified decoder entry point

#include "decoder_hantro.h"
#include "decoder_v4l2.h"

VSLDecoder* vsl_decoder_create(uint32_t codec, int fps)
{
    return vsl_decoder_create_ex(codec, fps, VSL_CODEC_BACKEND_AUTO);
}

VSLDecoder* vsl_decoder_create_ex(uint32_t codec, int fps,
                                   VSLCodecBackend backend)
{
    VSLCodecBackend effective = backend;

    // Resolve AUTO to concrete backend
    if (effective == VSL_CODEC_BACKEND_AUTO) {
        effective = vsl_detect_backend(false /* is_encoder */);
        if (effective == VSL_CODEC_BACKEND_AUTO) {
            fprintf(stderr, "vsl_decoder_create: no codec backend available\n");
            errno = ENODEV;
            return NULL;
        }
    }

    switch (effective) {
    case VSL_CODEC_BACKEND_V4L2:
        return vsl_decoder_create_v4l2(codec, fps);

    case VSL_CODEC_BACKEND_HANTRO:
        return vsl_decoder_create_hantro(codec, fps);

    default:
        errno = EINVAL;
        return NULL;
    }
}
```

#### Compile-Time Configuration

Both backends are compiled in by default. Use CMake options to exclude:

```cmake
option(ENABLE_V4L2_CODEC "Enable V4L2 codec backend" ON)
option(ENABLE_HANTRO_CODEC "Enable Hantro/libcodec.so backend" ON)

# At least one backend must be enabled
if(NOT ENABLE_V4L2_CODEC AND NOT ENABLE_HANTRO_CODEC)
    message(FATAL_ERROR "At least one codec backend must be enabled")
endif()
```

---

## Performance Considerations

### Benchmark Results (i.MX 8M Plus, 2025-01-01)

#### Decoder Performance (300 frames, H.264 Main Profile)

| Resolution | libcodec.so | V4L2 (GStreamer) | Improvement |
|------------|-------------|------------------|-------------|
| 640x480    | 34.00 ms    | 0.82 ms          | **41x faster** |
| 1280x720   | 111.00 ms   | 1.97 ms          | **56x faster** |
| 1920x1080  | 151.00 ms   | 4.12 ms          | **37x faster** |

**Key Finding**: V4L2 decoder is 37-56x faster than libcodec.so across all resolutions.

#### Encoder Performance (300 frames, NV12 → H.264)

| Resolution | libcodec.so (vpuenc) | V4L2 (v4l2h264enc) | Notes |
|------------|---------------------|---------------------|-------|
| 640x480    | SIGSEGV (crash)     | 5.10 ms             | V4L2 stable |
| 1280x720   | SIGSEGV (crash)     | 12.70 ms            | V4L2 stable |
| 1920x1080  | SIGSEGV (crash)     | 27.50 ms (H.264)    | V4L2 stable |
| 1920x1080  | N/A                 | 27.77 ms (HEVC)     | HEVC comparable |

**Key Finding**: libcodec.so encoder (vpuenc_h264) crashes with SIGSEGV. V4L2 encoder is stable and performant.

#### Performance Summary

```
V4L2 Advantages:
✓ Decoder: 37-56x faster than libcodec.so
✓ Encoder: Stable (libcodec.so crashes)
✓ Encoder: ~27ms/frame for 1080p = 36 fps capability
✓ Standard Linux API (portable)
✓ Active kernel driver maintenance

Recommended Action: Implement V4L2 backend as primary codec path
```

### Buffer Management

```
V4L2 Buffer Strategy:
- OUTPUT queue: 2-4 buffers (compressed data)
- CAPTURE queue: 4-8 buffers (decoded frames)
- Use DMABUF for zero-copy with DMA heap

Memory Types:
- V4L2_MEMORY_MMAP: Driver-allocated, good for compressed data
- V4L2_MEMORY_DMABUF: External DMA buffers, zero-copy sharing
```

---

## Implementation Phases

### Phase 1: V4L2 Decoder
1. Create `lib/decoder_v4l2.c`
2. Implement basic decode flow
3. Add DMABUF support for output
4. Handle resolution changes
5. Benchmark and validate

### Phase 2: V4L2 Encoder
1. Create `lib/encoder_v4l2.c`
2. Implement basic encode flow
3. Add DMABUF support for input
4. Implement crop for tiling
5. Benchmark and validate

### Phase 3: Integration
1. Add backend selection API
2. Auto-detection logic
3. Fallback handling
4. Documentation updates

---

## Benchmark Plan

### Decoder Benchmarks (COMPLETED)

| Test | Resolution | Profile | Frames | libcodec.so | V4L2 | Status |
|------|------------|---------|--------|-------------|------|--------|
| D1 | 640x480 | Main | 300 | 34.0 ms | 0.82 ms | ✅ Done |
| D2 | 1280x720 | Main | 300 | 111.0 ms | 1.97 ms | ✅ Done |
| D3 | 1920x1080 | Main | 300 | 151.0 ms | 4.12 ms | ✅ Done |
| D4 | 1920x1080 | High (B-frames) | 300 | TBD | TBD | Pending |

### Encoder Benchmarks (COMPLETED)

| Test | Resolution | Bitrate | Frames | libcodec.so | V4L2 | Status |
|------|------------|---------|--------|-------------|------|--------|
| E1 | 640x480 | 2 Mbps | 300 | CRASH | 5.10 ms | ✅ Done |
| E2 | 1280x720 | 5 Mbps | 300 | CRASH | 12.70 ms | ✅ Done |
| E3 | 1920x1080 | 10 Mbps | 300 | CRASH | 27.50 ms | ✅ Done |
| E3-HEVC | 1920x1080 | 10 Mbps | 300 | N/A | 27.77 ms | ✅ Done |
| E4 | 3840x2160 (4 tiles) | 25 Mbps | 150 | N/A | TBD | Future |

---

## Appendix: V4L2 Control IDs

### Decoder Controls
```c
V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY
V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY_ENABLE
```

### Encoder Controls
```c
V4L2_CID_MPEG_VIDEO_BITRATE
V4L2_CID_MPEG_VIDEO_GOP_SIZE
V4L2_CID_MPEG_VIDEO_H264_PROFILE
V4L2_CID_MPEG_VIDEO_H264_LEVEL
V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP
V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP
V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP
V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME
V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE
V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE
```

---

## Conclusion and Recommendations

### Summary of Findings

The benchmark results clearly demonstrate that V4L2 is the superior codec interface:

| Metric | libcodec.so | V4L2 | Winner |
|--------|-------------|------|--------|
| Decoder Performance | 34-151 ms/frame | 0.8-4.1 ms/frame | **V4L2 (37-56x)** |
| Encoder Stability | SIGSEGV crashes | Stable | **V4L2** |
| Encoder Performance | N/A (crashes) | 5-27 ms/frame | **V4L2** |
| API Portability | NXP-specific binary | Linux standard | **V4L2** |
| Maintenance | Proprietary blob | Active kernel driver | **V4L2** |

### Recommendations

1. **Implement V4L2 backend as primary codec path** - The performance and stability advantages are overwhelming

2. **Deprecate libcodec.so path** - The existing Hantro wrapper can remain as fallback for older systems, but should not be the default

3. **Prioritize decoder implementation** - The decoder sees the largest performance improvement (37-56x) and is critical for display pipelines

4. **Encoder can follow** - While stable, the V4L2 encoder performance (27ms @ 1080p = 36fps) is adequate for real-time encoding

5. **4K tiling** - Implement crop-based tiling for 4K support using the V4L2 selection API

### Implementation Priority

```
Phase 1 (High Priority): V4L2 Decoder
- Eliminate 150ms+ decode latency
- Enable real-time 1080p decode at 60fps

Phase 2 (Medium Priority): V4L2 Encoder
- Replace unstable libcodec.so encoder
- 27ms/frame is acceptable for 30fps targets

Phase 3 (Lower Priority): 4K Tiling
- Build on V4L2 encoder with crop support
- 4 parallel encoders for 4K30
```

---

## References

1. [V4L2 Stateful Decoder Interface](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-decoder.html)
2. [V4L2 Stateful Encoder Interface](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-encoder.html)
3. [V4L2 DMABUF](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dmabuf.html)
4. [NXP i.MX 8M Plus Reference Manual](https://www.nxp.com/docs/en/reference-manual/IMX8MPRM.pdf)
