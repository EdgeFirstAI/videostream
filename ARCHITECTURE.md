# VideoStream Library - Architecture Guide

**Version:** 2.2  
**Last Updated:** 2026-02-03

---

## Overview

VideoStream is a video I/O library for embedded Linux:

- **Zero-Copy IPC** - Share video frames between processes via DmaBuf
- **V4L2 Integration** - Camera capture, hardware codecs, device discovery
- **GStreamer Plugins** - `vslsink`/`vslsrc` elements
- **CLI Tool** - Streaming, recording, device enumeration
- **Rust Bindings** - Safe API with async support

```mermaid
graph LR
    subgraph Sources
        CAM[V4L2 Camera]
        GST[GStreamer]
    end

    subgraph VideoStream
        IPC[IPC Host/Client]
        ENC[V4L2 Encoder]
        DEC[V4L2 Decoder]
        DISC[Device Discovery]
    end

    subgraph Outputs
        CLI[CLI Recording]
        APP[Application]
    end

    CAM --> IPC
    GST --> IPC
    IPC --> ENC --> CLI
    IPC --> APP
    DEC --> APP
    DISC -.-> CAM
    DISC -.-> ENC
```

---

## System Architecture

```mermaid
graph TB
    subgraph "Host Process"
        H[VSLHost]
        TS[Task Thread]
    end

    subgraph "Client Process"
        C[VSLClient]
    end

    subgraph "IPC"
        US[UNIX Socket]
        FD[FD Passing]
    end

    subgraph "Memory"
        DB[DmaBuf]
    end

    H -->|Push| TS
    TS <-->|Messages| US
    US <-->|Lock/Unlock| C
    TS -->|SCM_RIGHTS| FD
    FD --> C
    C -->|mmap| DB
```

### Components

| Component | Role |
|-----------|------|
| **VSLHost** | Frame pool, client servicing, FD passing |
| **VSLClient** | Connect, receive frames, lock/unlock |
| **VSLFrame** | Frame metadata and memory mapping |
| **vslsink** | GStreamer sink → IPC host |
| **vslsrc** | IPC client → GStreamer source |
| **VSLEncoder** | V4L2 hardware encoding |
| **VSLDecoder** | V4L2 hardware decoding |

### Threading

- **Host**: Main thread + task thread for client servicing
- **Client**: Main thread + optional timeout watchdog
- All IPC operations are thread-safe with mutex protection

---

## IPC Protocol

### Message Flow

| Message | Direction | Purpose |
|---------|-----------|---------|
| `FRAME_EVENT` | Host → Client | New frame available |
| `LOCK_REQUEST` | Client → Host | Request frame access |
| `LOCK_RESPONSE` | Host → Client | Grant access with FD |
| `UNLOCK_REQUEST` | Client → Host | Release frame |

### Frame Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Available: Host posts frame
    Available --> Locked: Client locks
    Locked --> Available: Unlock (refs > 0)
    Locked --> Recycled: Unlock (refs = 0)
    Available --> Recycled: Timeout expired
    Recycled --> [*]
```

### Camera → IPC → Client Pipeline

```mermaid
sequenceDiagram
    participant CAM as V4L2 Camera
    participant SINK as vslsink
    participant HOST as IPC Host
    participant CLIENT as IPC Client
    participant APP as Application

    CAM->>SINK: DmaBuf frame
    SINK->>HOST: vsl_host_post(frame, lifespan=100ms)
    HOST->>CLIENT: FRAME_EVENT + FD (SCM_RIGHTS)
    
    Note over CLIENT,APP: FD received immediately - can use for DMA
    
    alt Fast DMA path (within 100ms lifespan)
        CLIENT->>APP: Pass FD to encoder/GPU
        APP->>APP: DMA operation completes
        Note over HOST: Frame auto-recycled after lifespan
    else Slow CPU path (needs extended lifespan)
        CLIENT->>HOST: LOCK_REQUEST (serial)
        HOST->>HOST: Extend lifespan, ref_count++
        CLIENT->>APP: mmap(fd) → pixel data
        APP->>APP: CPU processing...
        CLIENT->>HOST: UNLOCK_REQUEST
        HOST->>HOST: ref_count-- (recycle if 0)
    end
```

### Camera → Encode → File Pipeline

```mermaid
sequenceDiagram
    participant CAM as V4L2 Camera
    participant SINK as vslsink
    participant REC as videostream record
    participant ENC as V4L2 Encoder
    participant FILE as Output File

    CAM->>SINK: DmaBuf frame (NV12)
    SINK->>REC: FRAME_EVENT + FD
    
    Note over REC,ENC: No lock needed - DMA completes within lifespan
    
    REC->>ENC: Queue frame FD (OUTPUT)
    ENC->>ENC: Hardware encode
    REC->>ENC: Dequeue bitstream (CAPTURE)
    REC->>FILE: Write H.264/HEVC NAL units
    
    Note over SINK: Frame auto-recycled after 100ms
```

### Frame Lock Decision

```mermaid
flowchart TD
    START[Receive FRAME_EVENT + FD] --> USE{How will you<br/>use the frame?}
    
    USE -->|DMA: encoder, GPU, display| FAST{Completes within<br/>100ms lifespan?}
    FAST -->|Yes| DIRECT[Use FD directly<br/>No lock needed]
    DIRECT --> AUTO[Frame auto-recycled<br/>after lifespan]
    
    FAST -->|No| LOCK
    USE -->|CPU: read pixels, process| LOCK[Lock frame<br/>Extends lifespan]
    LOCK --> MMAP[mmap fd → pointer]
    MMAP --> PROCESS[Process data]
    PROCESS --> UNLOCK[Unlock frame]
    UNLOCK --> RECYCLE[Frame recycled<br/>when refs = 0]
```

**Lock Guidelines:**

| Access Type | Lock Required | Why |
|-------------|---------------|-----|
| DMA to encoder | **No** | Hardware completes within 100ms lifespan |
| DMA to GPU | **No** | Hardware completes within 100ms lifespan |
| CPU read/write | **Yes** | Need stable mmap, CPU may be slow |
| ML inference | **Yes** | Processing time unpredictable |
| Multiple clients | **Yes** | Each client locks independently |

---

## GStreamer Integration

### Zero-Copy Flow

```mermaid
flowchart LR
    subgraph Sources
        VT[videotestsrc]
        V4L2[v4l2src]
    end

    subgraph vslsink
        POOL[DmaBuf Pool]
        IPC[IPC Host]
    end

    subgraph Clients
        APP[Application]
        ENC[Encoder]
    end

    VT -->|"Uses pool"| POOL
    V4L2 -->|"Own dmabuf"| IPC
    POOL --> IPC
    IPC -->|"FD"| APP
    IPC -->|"FD"| ENC
```

### Memory Handling

| Source | Memory | Copy Required |
|--------|--------|---------------|
| v4l2src (dmabuf) | DmaBuf | No |
| libcamerasrc (dmabuf) | DmaBuf | No |
| videotestsrc | Pool DmaBuf | No |
| Any (system memory) | Copy to pool | Yes |

### Properties

| Plugin | Property | Description |
|--------|----------|-------------|
| vslsink | `path` | UNIX socket path |
| vslsink | `lifespan` | Frame timeout (ms) |
| vslsink | `pool-size` | DmaBuf pool size |
| vslsrc | `path` | Socket to connect |
| vslsrc | `timeout` | Frame wait timeout |
| vslsrc | `reconnect` | Auto-reconnect |

---

## V4L2 Hardware

### Codec Architecture

```mermaid
graph TB
    subgraph Application
        API[VideoStream API]
    end

    subgraph VideoStream
        DISC[Device Discovery]
        ENC[Encoder]
        DEC[Decoder]
    end

    subgraph Kernel
        M2M[V4L2 mem2mem]
    end

    subgraph Hardware
        VPU[VPU]
    end

    API --> DISC --> ENC & DEC
    ENC & DEC --> M2M --> VPU
```

### Supported Codecs

| Codec | Encode | Decode |
|-------|--------|--------|
| H.264/AVC | ✓ | ✓ |
| H.265/HEVC | ✓ | ✓ |
| VP8 | - | ✓ |
| VP9 | - | ✓ |

### V4L2 mem2mem

```mermaid
flowchart LR
    OUT[OUTPUT Queue<br/>Raw/Compressed] -->|QBUF| HW[Hardware] -->|DQBUF| CAP[CAPTURE Queue<br/>Compressed/Raw]
```

| Operation | OUTPUT | CAPTURE |
|-----------|--------|---------|
| Encode | Raw NV12/BGRA | H.264/HEVC |
| Decode | H.264/HEVC | Raw NV12 |

### Device Discovery

```mermaid
flowchart TD
    SCAN[Scan /dev/video*] --> CAP{Capabilities}
    CAP -->|VIDEO_CAPTURE| CAMERA[Camera]
    CAP -->|VIDEO_M2M| M2M{Format check}
    M2M -->|Compressed output| ENCODER
    M2M -->|Compressed input| DECODER
    M2M -->|Raw both| CONVERTER
```

| Type | Description | Examples |
|------|-------------|----------|
| Camera | VIDEO_CAPTURE | mxc-isi, neoisp |
| Encoder | M2M, compressed out | wave6-enc, vsi_v4l2enc |
| Decoder | M2M, compressed in | wave6-dec, vsi_v4l2dec |
| Converter | M2M, raw both | mxc-isi-m2m |

### Platform Support

| Platform | Encoder | Decoder | Camera |
|----------|---------|---------|--------|
| i.MX 95 | wave6-enc | wave6-dec | mxc-isi-cap |
| i.MX 8M Plus | vsi_v4l2enc | vsi_v4l2dec | VIV |

---

## CLI Tool

### Architecture

```mermaid
graph TB
    subgraph CLI
        CMD[Commands]
    end

    subgraph Rust
        BIND[videostream crate]
    end

    subgraph C
        LIB[libvideostream.so]
    end

    CMD --> BIND --> LIB
```

### Commands

| Command | Purpose |
|---------|---------|
| `stream` | Camera → IPC socket |
| `record` | Camera → H.264/H.265 file |
| `convert` | Annex B → MP4 |
| `devices` | V4L2 device discovery |
| `receive` | IPC → performance test |
| `info` | System capabilities |

---

## Example Pipelines

### Camera to IPC

```bash
# Terminal 1: Host
gst-launch-1.0 v4l2src ! vslsink path=/tmp/cam

# Terminal 2: Client
videostream receive --ipc /tmp/cam
```

### Camera to Recording

```bash
videostream record --device /dev/video0 --format NV12 \
    --encoder /dev/video10 -t 10 output.h264

videostream convert output.h264 output.mp4
```

### IPC to Encoding

```bash
# Terminal 1: Camera stream
gst-launch-1.0 libcamerasrc ! vslsink path=/tmp/vsl

# Terminal 2: Encode from IPC
videostream record --ipc /tmp/vsl --backend v4l2 -t 5 video.h264
```
