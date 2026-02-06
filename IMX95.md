# i.MX 95 Platform Support Investigation

**Status:** Active Development
**Date:** 2026-01-30
**Author:** Sébastien Taylor
**Target:** NXP i.MX 95 19x19 EVK with OS08A20 Camera Module

## Executive Summary

This document proposes adding i.MX 95 support to VideoStream. The i.MX 95 has a fundamentally different codec architecture compared to the i.MX 8M Plus:

| Component | i.MX 8M Plus | i.MX 95 |
|-----------|--------------|---------|
| **VPU** | Hantro VC8000e (proprietary API + V4L2) | Chips&Media Wave6 (V4L2 only) |
| **V4L2 Encoder** | vsi_v4l2enc | wave6-enc |
| **V4L2 Decoder** | vsi_v4l2dec | wave6-dec |
| **JPEG Codec** | mxc-jpeg | mxc-jpeg (separate enc/dec devices) |
| **GPU** | Vivante GC7000 | Mali |
| **2D Acceleration** | G2D (libg2d.so) | G2D via DPU (libg2d-dpu.so) + PXP available |
| **Camera ISI** | imx8-isi | imx95-isi (fsl,imx95-isi) |
| **CSI-2** | imx8-mipi-csi2 | dwc-mipi-csi2 (Synopsys DWC MIPI CSI-2) |

**Key Findings:**
1. The i.MX 95 uses standard V4L2 CODEC API exclusively - no proprietary Hantro/libcodec.so interface
2. G2D is available via `/usr/lib/libg2d.so.2` → `libg2d-dpu.so.2.5.0` (DPU-based G2D)
3. PXP (Pixel Pipeline) driver is also available (`imx-pxp-v3`) for future investigation
4. **Device paths should NOT be hardcoded** - use capability probing instead
5. **OS08A20 camera working** - Raw Bayer capture at 60fps (1080p) or 30fps (4K)
6. **NeoISP YUV output working** - Use `LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'` (see Section 11)

---

## 1. Hardware Investigation Results

### 1.1 Board Information

```
Model: NXP FRDM-IMX95
Kernel: 6.12.49-lts-next
Architecture: aarch64
```

### 1.2 V4L2 Device Probing

**Important:** Device numbers (`/dev/videoX`) should NOT be hardcoded. Instead, probe devices by capability:

```c
// Probe algorithm:
// 1. Enumerate /dev/video* devices
// 2. Query VIDIOC_QUERYCAP for driver name and capabilities
// 3. For M2M devices, check format support to determine role:
//    - CAPTURE has compressed formats → ENCODER
//    - OUTPUT has compressed formats → DECODER
//    - Neither has compressed → IMAGE PROCESSOR
```

**Probe Results on i.MX 95 FRDM:**

| Device | Driver | Card Name | Capability | Role | Codecs |
|--------|--------|-----------|------------|------|--------|
| `/dev/video0` | wave6-dec | C&M Wave6 VPU decoder | M2M_MPLANE | DECODER | H.264, HEVC |
| `/dev/video1` | wave6-enc | C&M Wave6 VPU encoder | M2M_MPLANE | ENCODER | H.264, HEVC |
| `/dev/video2` | mxc-jpeg | mxc-jpeg codec | M2M_MPLANE | DECODER | JPEG |
| `/dev/video3` | mxc-jpeg | mxc-jpeg codec | M2M_MPLANE | ENCODER | JPEG |

**Device Discovery Strategy:**
1. Scan `/sys/class/video4linux/video*/name` or probe via `VIDIOC_QUERYCAP`
2. Match driver name (`wave6-enc`, `wave6-dec`, `vsi_v4l2enc`, `vsi_v4l2dec`, `mxc-jpeg`)
3. Or enumerate formats to detect encoder vs decoder role
4. Cache discovered device paths at initialization

### 1.3 Decoder Supported Formats (video0)

**INPUT (OUTPUT queue):**
- `HEVC` - HEVC compressed
- `H264` - H.264 compressed

**OUTPUT (CAPTURE queue):**
- `YU12` - Planar YUV 4:2:0 (I420)
- `NV12` - Y/UV 4:2:0 (semi-planar)
- `NV21` - Y/VU 4:2:0 (semi-planar)
- `YM12` - Planar YUV 4:2:0 (N-C, non-contiguous)
- `NM12` - Y/UV 4:2:0 (N-C, non-contiguous)
- `NM21` - Y/VU 4:2:0 (N-C, non-contiguous)

### 1.4 Encoder Supported Formats (video1)

**INPUT (OUTPUT queue):**
- YUV formats: `YU12`, `NV12`, `NV21`, `422P`, `NV16`, `NV61`, `YUYV`, `YUV3`, `NV24`, `NV42`
- Non-contiguous: `YM12`, `NM12`, `NM21`, `YM16`, `NM16`, `NM61`
- 10-bit: `P010`
- RGB formats: `RGB3`, `BGR3`, `BA24` (ARGB), `BX24` (XRGB), `AB24` (RGBA), `XB24` (RGBX), `XR24` (BGRX), `AR24` (BGRA), `RX24` (XBGR), `RA24` (ABGR), `AR30` (ARGB 2-10-10-10)

**OUTPUT (CAPTURE queue):**
- `HEVC` - HEVC compressed
- `H264` - H.264 compressed

### 1.5 Memory Support

All V4L2 devices support:
- ✅ `V4L2_MEMORY_MMAP`
- ✅ `V4L2_MEMORY_DMABUF`

DMA Heaps available:
- `/dev/dma_heap/linux,cma` (physically contiguous)
- `/dev/dma_heap/linux,cma-uncached` (physically contiguous, uncached)
- `/dev/dma_heap/system` (scatter-gather)

### 1.6 Encoder Controls

The Wave6 encoder exposes comprehensive V4L2 controls:

**General:**
- `V4L2_CID_HFLIP`, `V4L2_CID_VFLIP`, `V4L2_CID_ROTATE` (0-270°)
- `V4L2_CID_MPEG_VIDEO_GOP_SIZE` (0-2047, default 30)
- `V4L2_CID_MPEG_VIDEO_BITRATE` (1-240000000, default 2097152)
- `V4L2_CID_MPEG_VIDEO_BITRATE_MODE` (CBR/VBR)
- `V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE`
- `V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME`

**H.264 Specific:**
- `V4L2_CID_MPEG_VIDEO_H264_PROFILE` (Baseline/Main/High)
- `V4L2_CID_MPEG_VIDEO_H264_LEVEL` (1.0-5.2)
- `V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP` (0-51)
- `V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP` (0-51)
- `V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP` (0-51)
- `V4L2_CID_MPEG_VIDEO_H264_MIN_QP` / `MAX_QP`
- `V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE` (CAVLC/CABAC)
- `V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE`
- `V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM`

**HEVC Specific:**
- `V4L2_CID_MPEG_VIDEO_HEVC_PROFILE` (Main/Main10)
- `V4L2_CID_MPEG_VIDEO_HEVC_LEVEL`
- `V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP` / `MAX_QP`
- `V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP` / `P_FRAME_QP` / `B_FRAME_QP`
- `V4L2_CID_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE`
- `V4L2_CID_MPEG_VIDEO_HEVC_STRONG_SMOOTHING`

**Advanced:**
- `V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR`
- `V4L2_CID_MPEG_VIDEO_FRAME_SKIP_MODE`
- Region of Interest (ROI) support

### 1.7 Decoder Controls

- `V4L2_CID_MIN_BUFFERS_FOR_CAPTURE` (1-32)
- `V4L2_CID_MPEG_VIDEO_H264_PROFILE` (read-only, parsed from stream)
- `V4L2_CID_MPEG_VIDEO_HEVC_PROFILE` (read-only, parsed from stream)
- Display delay control

### 1.8 2D Graphics and Scaling Acceleration

The i.MX 95 provides multiple hardware accelerators for image scaling and processing:

| Accelerator | Hardware | Scaling Algorithm | Formats | Use Case |
|-------------|----------|-------------------|---------|----------|
| **ISI M2M** | ISI | Polyphase filter (bilinear+) | YUV/RGB only | Post-ISP scaling |
| **G2D** | DPU | Bilinear interpolation | YUV/RGB | General scaling, CSC, rotation |
| **OpenCL** | GPU (Mali) | Configurable | YUV/RGB | Dewarp, advanced processing |
| **PXP** | PXP | TBD | YUV/RGB | Alternative to G2D |
| **VPU** | Wave6 | Built-in post-proc | Decoded video | Decoder output scaling |

**Note:** The NEO-ISP has **no scaler** - output must match sensor native resolution. Use ISI M2M or G2D for post-ISP scaling.

#### ISI M2M Scaler

The ISI M2M device (`/dev/video8`) provides hardware scaling with **polyphase filtering** (not simple pixel decimation):

- **Algorithm:** Multi-tap polyphase filter providing bilinear-quality or better interpolation
- **Direction:** Downscale only (output ≤ input resolution)
- **Quality:** Smooth scaling with anti-aliasing, no pixel-skip artifacts
- **Input formats:** YUYV, RGBP, RGB3, BGR3, XR24, AR24, AB24, XB24
- **Output formats:** 17 formats including NV12, NV16, YUYV, RGB variants
- **Limitation:** Cannot process RAW Bayer - use for post-ISP/post-NeoISP scaling

```bash
# Example: Scale 4K to 1080p using ISI M2M (GStreamer)
gst-launch-1.0 ... ! video/x-raw,width=3840,height=2160 ! \
    v4l2convert output-io-mode=dmabuf-import capture-io-mode=dmabuf ! \
    video/x-raw,width=1920,height=1080 ! ...
```

#### G2D Library (DPU-based)

On i.MX 95, G2D is implemented via the **DPU (Display Processing Unit)**, not a GPU like on i.MX 8M Plus:

- **Location:** `/usr/lib/libg2d.so.2` → `libg2d-dpu.so.2.5.0`
- **Version:** G2D API 2.5.0 (DPU-based implementation)
- **Hardware:** `G2D_HARDWARE_DPU_V2` (DPU V2)
- **Header:** `/usr/include/g2d.h` (MIT licensed)
- **Scaling:** Bilinear interpolation

**Key Difference from i.MX 8M Plus:**
| Feature | i.MX 8M Plus | i.MX 95 |
|---------|--------------|---------|
| G2D Backend | Vivante GPU | DPU V2 |
| Library | libg2d.so | libg2d-dpu.so |
| Hardware ID | `G2D_HARDWARE_2D` | `G2D_HARDWARE_DPU_V2` |

**Supported Operations:**
- `g2d_blit()` - Blit with scaling, rotation, format conversion
- `g2d_copy()` - Fast memory copy
- `g2d_clear()` - Fast clear
- `g2d_multi_blit()` - Multi-source blitting (composition)
- `g2d_blitEx()` - Extended blit

**Supported Formats:**
- RGB: RGB565, RGBA8888, BGRA8888, ARGB8888, RGB888, BGR888, etc.
- YUV: NV12, NV21, I420, YV12, YUYV, UYVY, NV16, NV61
- 10-bit: RGBA1010102, GRAY10
- Grayscale: GRAY8

**DMA-BUF Integration:**
```c
// Get g2d_buf from DMA-BUF fd
struct g2d_buf* g2d_buf_from_fd(int fd);
// Export g2d_buf as DMA-BUF fd
int g2d_buf_export_fd(struct g2d_buf* buf);
```

**GStreamer Elements:**
- `imxvideoconvert_g2d` - G2D-based video converter (scaling, CSC, rotation)
- `imxcompositor_g2d` - G2D-based multi-input compositor

#### PXP (Pixel Pipeline)

- **Driver:** `imx-pxp-v3` available in `/sys/bus/platform/drivers/`
- **Status:** Available for future investigation
- **Use case:** Alternative to G2D for certain operations
- **Hardware ID:** `G2D_HARDWARE_PXP_V2` (if accessed via G2D API)

#### OpenCL Scaler

For advanced scaling with dewarp support:

- **Element:** `imxvideoconvert_ocl`
- **Features:** Downscale, dewarp, color range conversion, deinterlace
- **Dewarp:** Supports lens distortion correction
- **Limitation:** Output format fixed to RGB for some operations

```bash
# Dewarp + downscale example
gst-launch-1.0 libcamerasrc ! video/x-raw,format=NV12 ! \
    imxvideoconvert_ocl dewarp-enable=true output-width=640 output-height=480 ! \
    waylandsink
```

### 1.9 Camera/ISI Interface

- **ISI:** `fsl,imx95-isi` - Image Sensing Interface (up to 8 channels)
- **MIPI CSI-2:** `dwc-mipi-csi2` - Synopsys DesignWare MIPI CSI-2 controller
- **Camera CSR:** `nxp,imx95-camera-csr` - Camera Control/Status Registers
- **Drivers:** `isi-capture`, `isi-m2m`, `mxc-isi`, `mxc-isi_v1`

**Note:** No camera is currently connected to the FRDM board, so camera capture testing is pending.

### 1.10 NEO-ISP (Image Signal Processing)

The i.MX 95 uses a fundamentally different ISP architecture compared to i.MX 8M Plus:

| Feature | i.MX 8M Plus | i.MX 95 |
|---------|--------------|---------|
| **ISP Type** | VeriSilicon (inline) | NEO-ISP (M2M) |
| **API** | Custom JSON over ioctl | Standard V4L2 M2M |
| **Control Interface** | `V4L2_CID_VIV_EXTCTRL` | V4L2 controls + libcamera IPA |
| **3A Algorithms** | ISP daemon | libcamera IPA module |
| **Pipeline Management** | Direct V4L2 | libcamera pipeline handler |

**NEO-ISP V4L2 Devices:**

```
neoisp-input0 (/dev/video?) - RAW frame input (video_output)
neoisp-input1 (/dev/video?) - Short exposure HDR input (video_output)
neoisp-params (/dev/video?) - 3A parameters (meta_output)
neoisp-frame  (/dev/video?) - Processed frame output (video_capture)
neoisp-ir     (/dev/video?) - IR image output (video_capture)
neoisp-stats  (/dev/video?) - 3A statistics (meta_capture)
```

**NEO-ISP Input Formats (RAW Bayer):**
- 8-bit: SRGGB8, SBGGR8, SGBRG8, SGRBG8
- 10-bit: SRGGB10, SBGGR10, SGBRG10, SGRBG10
- 12-bit: SRGGB12, SBGGR12, SGBRG12, SGRBG12
- 14-bit: SRGGB14, SBGGR14, SGBRG14, SGRBG14

**NEO-ISP Output Formats:**
- YUV: NV12, NV21, NV16, NV61, YUYV, UYVY, VYUY, YUV24, YUVX32, VUYX32
- RGB: BGR24, RGB24, BGRX32, RGBX32
- Grey: GREY, Y10, Y12, Y16, Y16_BE

**Camera Pipeline (RAW sensor with NEO-ISP):**

```
┌──────────┐    ┌──────────────┐    ┌──────────────────┐    ┌─────┐
│  Camera  │───▶│ MIPI CSI-2   │───▶│ CSI Pixel Format │───▶│ ISI │
│  Sensor  │    │ (dwc-mipi)   │    │                  │    │     │
└──────────┘    └──────────────┘    └──────────────────┘    └──┬──┘
                                                               │
                                                               ▼ DDR
                                                          ┌─────────┐
                                                          │ RAW buf │
                                                          └────┬────┘
                                                               │
                                    ┌──────────────────────────┘
                                    ▼
                              ┌───────────┐
                              │  NEO-ISP  │ (M2M processing)
                              │           │
                              │ ┌───────┐ │
                              │ │ 3A    │ │◄── params (from libcamera IPA)
                              │ │ stats │ │──▶ stats (to libcamera IPA)
                              │ └───────┘ │
                              └─────┬─────┘
                                    │
                                    ▼
                              ┌───────────┐
                              │ YUV/RGB   │
                              │ output    │
                              └───────────┘
```

**libcamera Integration:**
- Pipeline handler: `NXP NEO-ISP pipeline handler` (nxp/neo)
- IPA module: Provides 3A (AE, AWB, AF) algorithms (open-source reference implementation)
- Smart cameras (YUV output): Use `NXP ISI pipeline handler` (imx8-isi) - no NEO-ISP needed
- Simple cameras: Use `Simple pipeline handler` (simple) for basic sensors
- UVC cameras: Standard UVC pipeline handler

**Pipeline Handler Selection:**

```bash
# Force pipeline handler order (Neo ISP first, then ISI, then Simple)
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi,simple'

# Enable debug logging
export LIBCAMERA_LOG_LEVELS='NxpNeo:DEBUG,ISI:DEBUG'
```

**Supported Cameras (per NXP documentation):**

| Sensor | Resolution | Features | Notes | DTB Required |
|--------|------------|----------|-------|--------------|
| OX03C10 | 1920x1280 (2.5 MP) | 3-exposure HDR | Via MAX96724 GMSL deserializer | imx95-*-ox03c10-isp-*.dtb |
| OX05B1S | 2592x1944 (5 MP) | RGB-IR 4x4, 10-bit | Direct MIPI | imx95-*-ox05b1s-isp-*.dtb |
| OS08A20 | 3840x2160 (8 MP) | 2-exposure HDR | Direct MIPI, RAW=SBGGR12 | imx95-*-os08a20-isp-*.dtb |
| AP1302/AR0144 | 1280x800 (1 MP) | Smart camera (on-board ISP) | YUV output, no NEO-ISP | imx95-*-ap1302.dtb |
| OV5640 | 2592x1944 (5 MP) | Smart camera (on-board ISP) | YUV output, no NEO-ISP | Direct (i.MX 8M only) |

**Camera Environment Variables:**

```bash
# OS08A20 on i.MX 95 EVK
W=3840
H=2160
RAW=SBGGR12
CAMERA0="/base/soc/bus@42000000/i2c@42530000/os08a20_mipi@36"

# OX05B1S on i.MX 95 EVK
W=2592
H=1944
RAW=SGRBG10
CAMERA0="/base/soc/bus@42000000/i2c@42530000/ox05b1s@36"

# AP1302 Smart Camera (no RAW, YUV output)
W=1280
H=800
CAMERA0="/base/soc/bus@42000000/i2c@42530000/ap1302_mipi@3c"
```

### 1.11 NEO-ISP IPA (Image Processing Algorithm)

The IPA module is a shared library providing 3A control algorithms. Two modes:

1. **In-tree (open-source):** Runs in same process as libcamera (different thread)
2. **Out-of-tree (closed-source):** Runs in separate process via IPC

**IPA Configuration:**

```bash
# Pipeline handler config file
{prefix}/share/libcamera/pipeline/nxp/neo/config.yaml

# Override with environment variable
export LIBCAMERA_NXP_NEO_CONFIG_FILE=/path/to/config.yaml
```

**Config Options:**
- `image1`: Enable HDR short exposure capture stream
- `edata`: Enable embedded data stream

**Camera Calibration Files:**
Located in `<libcamera>/src/ipa/nxp/neo/data/<camera_model>.yaml`

Each camera requires:
1. CameraHelper implementation (sensor-specific handling)
2. Calibration YAML file (algorithm parameters)

### 1.12 NEO-ISP Limitations

From NXP documentation:

1. **M2M Only:** ISP operates in memory-to-memory mode only (no streaming mode bypass)
2. **No Scaler:** NEO-ISP has no scaler - stream size must match sensor native resolution
3. **OX03C10 Stability:** May hang under heavy system load
4. **PipeWire Conflicts:** With OX03C10, PipeWire service may crash continuously
   ```bash
   systemctl --user stop pipewire
   systemctl --user mask pipewire
   ```

---

## 2. Architecture Comparison

### 2.1 Current VideoStream Architecture (i.MX 8M Plus)

```
┌─────────────────────────────────────────────────────────────┐
│                   VideoStream API                            │
│    vsl_encoder_create() / vsl_decoder_create()              │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
    ┌─────────────────┐             ┌─────────────────┐
    │  V4L2 Backend   │             │  Hantro Backend │
    │  (vsi_v4l2)     │             │  (libcodec.so)  │
    ├─────────────────┤             ├─────────────────┤
    │ /dev/video0 enc │             │ /dev/mxc_hantro │
    │ /dev/video1 dec │             │ /dev/mxc_hantro │
    │                 │             │    _vc8000e     │
    └─────────────────┘             └─────────────────┘
           │                               │
           └───────────┬───────────────────┘
                       ▼
              ┌─────────────────┐
              │   DMA Heap      │
              │ /dev/dma_heap/  │
              │   linux,cma     │
              └─────────────────┘
```

### 2.2 Proposed Architecture (Multi-Platform with Device Probing)

```
┌─────────────────────────────────────────────────────────────┐
│                   VideoStream API                            │
│    vsl_encoder_create() / vsl_decoder_create()              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
              ┌───────────────────────────────┐
              │   V4L2 Device Discovery       │
              │   (probe by capability)       │
              │                               │
              │  1. Enumerate /dev/video*     │
              │  2. VIDIOC_QUERYCAP           │
              │  3. Check M2M + formats       │
              │  4. Identify role (enc/dec)   │
              └───────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
    ┌─────────────────┐             ┌─────────────────┐
    │  V4L2 Backend   │             │  Hantro Backend │
    │  (any M2M dev)  │             │  (libcodec.so)  │
    │                 │             │  [i.MX 8MP only]│
    ├─────────────────┤             ├─────────────────┤
    │ wave6-enc/dec   │             │ /dev/mxc_hantro │
    │ vsi_v4l2enc/dec │             │    _vc8000e     │
    │ mxc-jpeg        │             └─────────────────┘
    └─────────────────┘
           │
           ▼
    ┌─────────────────┐
    │ G2D Acceleration│
    │ libg2d.so.2     │
    │ (format conv,   │
    │  scaling, CSC)  │
    └─────────────────┘
           │
           ▼
    ┌─────────────────┐
    │   DMA Heap      │
    │ /dev/dma_heap/  │
    │   linux,cma     │
    └─────────────────┘
```

---

## 3. Implementation Plan

### 3.1 Phase 1: V4L2 Device Discovery

**File:** `lib/v4l2_discovery.c`, `lib/v4l2_discovery.h` (new)

Implement capability-based device discovery instead of hardcoded paths:

```c
/**
 * V4L2 device role determined by probing format support.
 */
typedef enum {
    VSL_V4L2_ROLE_UNKNOWN = 0,
    VSL_V4L2_ROLE_CAMERA,           // CAPTURE only, raw formats
    VSL_V4L2_ROLE_ENCODER,          // M2M, CAPTURE has compressed
    VSL_V4L2_ROLE_DECODER,          // M2M, OUTPUT has compressed
    VSL_V4L2_ROLE_IMAGE_PROCESSOR,  // M2M, raw-to-raw
    VSL_V4L2_ROLE_DISPLAY,          // OUTPUT only
} VSLv4l2Role;

/**
 * Codec types supported by a device.
 */
typedef struct {
    bool h264;
    bool hevc;
    bool jpeg;
    bool vp8;
    bool vp9;
} VSLv4l2Codecs;

/**
 * Discovered V4L2 device information.
 */
typedef struct {
    char path[32];           // e.g., "/dev/video0"
    char driver[32];         // e.g., "wave6-enc"
    char card[32];           // e.g., "C&M Wave6 VPU encoder"
    VSLv4l2Role role;
    VSLv4l2Codecs codecs;
    bool supports_mplane;
    bool supports_dmabuf;
} VSLv4l2Device;

/**
 * Probe all V4L2 devices and categorize by capability.
 * @param devices Output array of discovered devices
 * @param max_devices Maximum number of devices to probe
 * @return Number of devices found
 */
int vsl_v4l2_probe_devices(VSLv4l2Device* devices, int max_devices);

/**
 * Find first device matching role and codec.
 * @param role Desired role (encoder, decoder, etc.)
 * @param codec Desired codec (H264, HEVC, JPEG, 0 for any)
 * @return Device path or NULL if not found
 */
const char* vsl_v4l2_find_device(VSLv4l2Role role, uint32_t codec);
```

**Probing Algorithm:**
```c
int vsl_v4l2_probe_devices(VSLv4l2Device* devices, int max_devices) {
    DIR* dir = opendir("/dev");
    int count = 0;
    
    while ((entry = readdir(dir)) && count < max_devices) {
        if (strncmp(entry->d_name, "video", 5) != 0) continue;
        
        char path[32];
        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
        
        int fd = open(path, O_RDWR | O_NONBLOCK);
        if (fd < 0) continue;
        
        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            VSLv4l2Device* dev = &devices[count];
            strncpy(dev->path, path, sizeof(dev->path));
            strncpy(dev->driver, (char*)cap.driver, sizeof(dev->driver));
            strncpy(dev->card, (char*)cap.card, sizeof(dev->card));
            
            __u32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
                ? cap.device_caps : cap.capabilities;
            
            dev->supports_mplane = caps & V4L2_CAP_VIDEO_M2M_MPLANE;
            dev->role = determine_role(fd, caps);
            probe_codecs(fd, caps, &dev->codecs);
            check_dmabuf_support(fd, caps, &dev->supports_dmabuf);
            
            count++;
        }
        close(fd);
    }
    closedir(dir);
    return count;
}
```

### 3.2 Phase 2: Update Backend Selection

**File:** `lib/codec_backend.c`

Replace hardcoded device paths with discovery-based selection:

```c
// Remove these hardcoded paths:
// #define VSL_V4L2_ENCODER_DEV "/dev/video0"
// #define VSL_V4L2_DECODER_DEV "/dev/video1"

// Use discovery instead:
bool vsl_v4l2_codec_available(bool is_encoder) {
    VSLv4l2Role role = is_encoder ? VSL_V4L2_ROLE_ENCODER : VSL_V4L2_ROLE_DECODER;
    return vsl_v4l2_find_device(role, 0) != NULL;
}

const char* vsl_v4l2_get_device(bool is_encoder, uint32_t codec) {
    VSLv4l2Role role = is_encoder ? VSL_V4L2_ROLE_ENCODER : VSL_V4L2_ROLE_DECODER;
    return vsl_v4l2_find_device(role, codec);
}
```

### 3.3 Phase 3: Format Handling

**File:** `lib/encoder_v4l2.c`

The i.MX 8M Plus VSI driver uses non-standard fourcc codes. The Wave6 driver uses standard codes:

| Format | VSI (i.MX 8MP) | Wave6 (i.MX 95) |
|--------|----------------|-----------------|
| BGRA | `BGR4` | `AR24` |
| RGBA | `AB24` | `AB24` |
| ARGB | `AR24` | `BA24` |
| NV12 | `NV12` | `NV12` |

Update `vsl_to_v4l2_input_format()` to handle platform-specific fourcc mapping.

### 3.4 Phase 4: Multi-planar Buffer Handling

The Wave6 driver uses `V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE` for all operations. The current V4L2 backend already supports this, but verify:

1. **Non-contiguous formats:** Wave6 supports `NM12`, `YM12` etc. for multi-planar with separate DMA mappings
2. **Plane configuration:** Ensure correct plane count and offsets for NV12

### 3.5 Phase 5: JPEG Codec Support (Optional)

Add support for hardware JPEG encode/decode:

- `/dev/video2` - JPEG decode (mxc-jpeg)
- `/dev/video3` - JPEG encode (mxc-jpeg)

This is a separate codec pair from H.264/HEVC and could be exposed as:
```c
VSLDecoder* vsl_jpeg_decoder_create(void);
VSLEncoder* vsl_jpeg_encoder_create(int quality);
```

### 3.6 Phase 6: G2D Integration

The i.MX 95 has G2D available via DPU (`libg2d-dpu.so`). Update G2D support:

**File:** `lib/g2d.c`, `lib/g2d.h`

```c
// G2D is available on both i.MX 8M Plus (GPU-based) and i.MX 95 (DPU-based)
// The API is the same, only the underlying implementation differs

#include <g2d.h>

/**
 * Initialize G2D context.
 * Works with both GPU-based and DPU-based G2D implementations.
 */
int vsl_g2d_init(void** handle) {
    if (g2d_open(handle) != G2D_STATUS_OK) {
        return -1;
    }
    
    // Query hardware type for debugging
    enum g2d_hardware_type hw_type;
    if (g2d_query_hardware(handle, &hw_type) == G2D_STATUS_OK) {
        const char* hw_name;
        switch (hw_type) {
            case G2D_HARDWARE_2D:     hw_name = "GPU 2D"; break;
            case G2D_HARDWARE_VG:     hw_name = "GPU VG"; break;
            case G2D_HARDWARE_DPU_V1: hw_name = "DPU V1"; break;
            case G2D_HARDWARE_DPU_V2: hw_name = "DPU V2"; break;  // i.MX 95
            case G2D_HARDWARE_PXP_V1: hw_name = "PXP V1"; break;
            case G2D_HARDWARE_PXP_V2: hw_name = "PXP V2"; break;
            default:                  hw_name = "Unknown"; break;
        }
        fprintf(stderr, "[g2d] Hardware: %s\n", hw_name);
    }
    return 0;
}

/**
 * Convert frame using G2D with DMA-BUF zero-copy.
 */
int vsl_g2d_convert(void* handle,
                    int src_fd, int src_width, int src_height, enum g2d_format src_fmt,
                    int dst_fd, int dst_width, int dst_height, enum g2d_format dst_fmt) {
    struct g2d_surface src = {
        .format = src_fmt,
        .width = src_width,
        .height = src_height,
        .stride = src_width,  // Adjust based on format
    };
    
    struct g2d_surface dst = {
        .format = dst_fmt,
        .width = dst_width,
        .height = dst_height,
        .stride = dst_width,
    };
    
    // Use DMA-BUF integration (G2D API 2.5+)
    struct g2d_buf* src_buf = g2d_buf_from_fd(src_fd);
    struct g2d_buf* dst_buf = g2d_buf_from_fd(dst_fd);
    
    src.planes[0] = src_buf->buf_paddr;
    dst.planes[0] = dst_buf->buf_paddr;
    
    int ret = g2d_blit(handle, &src, &dst);
    g2d_finish(handle);
    
    g2d_free(src_buf);
    g2d_free(dst_buf);
    
    return (ret == G2D_STATUS_OK) ? 0 : -1;
}
```

**G2D Hardware Detection:**
- `G2D_HARDWARE_2D` / `G2D_HARDWARE_VG` → i.MX 8M Plus (Vivante GPU)
- `G2D_HARDWARE_DPU_V2` → i.MX 95 (DPU-based)
- `G2D_HARDWARE_PXP_V2` → PXP-based (available on i.MX 95, future option)

---

## 4. Code Changes Required

### 4.1 New Files

**`lib/v4l2_discovery.h`** - V4L2 device discovery API
**`lib/v4l2_discovery.c`** - Implementation of device probing

### 4.2 `lib/codec_backend.h`

```c
// Remove hardcoded device paths
// - #define VSL_V4L2_ENCODER_DEV "/dev/video0"
// - #define VSL_V4L2_DECODER_DEV "/dev/video1"

// Add discovery-based API
#include "v4l2_discovery.h"

// Get encoder/decoder device path via probing
const char* vsl_v4l2_get_encoder_device(uint32_t codec);
const char* vsl_v4l2_get_decoder_device(uint32_t codec);
```

### 4.3 `lib/codec_backend.c`

```c
#include "v4l2_discovery.h"

// Cache discovered devices
static VSLv4l2Device g_devices[16];
static int g_device_count = -1;

static void ensure_devices_probed(void) {
    if (g_device_count < 0) {
        g_device_count = vsl_v4l2_probe_devices(g_devices, 16);
    }
}

bool vsl_v4l2_codec_available(bool is_encoder) {
    ensure_devices_probed();
    VSLv4l2Role role = is_encoder ? VSL_V4L2_ROLE_ENCODER : VSL_V4L2_ROLE_DECODER;
    
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].role == role && 
            (g_devices[i].codecs.h264 || g_devices[i].codecs.hevc)) {
            return true;
        }
    }
    return false;
}

const char* vsl_v4l2_get_encoder_device(uint32_t codec) {
    ensure_devices_probed();
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].role != VSL_V4L2_ROLE_ENCODER) continue;
        if (codec == VSL_FOURCC('H','2','6','4') && g_devices[i].codecs.h264) 
            return g_devices[i].path;
        if (codec == VSL_FOURCC('H','E','V','C') && g_devices[i].codecs.hevc)
            return g_devices[i].path;
        if (codec == VSL_FOURCC('J','P','E','G') && g_devices[i].codecs.jpeg)
            return g_devices[i].path;
    }
    return NULL;
}
```

### 4.4 `lib/encoder_v4l2.c`

Update to use discovered device paths and handle driver-specific formats:

```c
// Use discovery instead of hardcoded path
const char* dev_path = vsl_v4l2_get_encoder_device(output_codec);
if (!dev_path) {
    fprintf(stderr, "No V4L2 encoder found for codec\n");
    return NULL;
}
enc->fd = open(dev_path, O_RDWR | O_NONBLOCK);

// Detect driver for format quirks
struct v4l2_capability cap;
ioctl(enc->fd, VIDIOC_QUERYCAP, &cap);
bool is_vsi_driver = (strstr((char*)cap.driver, "vsi") != NULL);

// Format mapping based on driver
static uint32_t
vsl_to_v4l2_input_format(uint32_t fourcc, int* num_planes, bool is_vsi) {
    switch (fourcc) {
    case VSL_FOURCC('B', 'G', 'R', 'A'):
        *num_planes = 1;
        // VSI driver uses non-standard fourcc, Wave6 uses standard
        return is_vsi ? VSI_V4L2_PIX_FMT_BGR4 : V4L2_PIX_FMT_ABGR32;
    case VSL_FOURCC('R', 'G', 'B', 'A'):
        *num_planes = 1;
        return V4L2_PIX_FMT_RGBA32;  // AB24 - same for both
    // ...
    }
}
```

### 4.5 CMake Configuration

```cmake
# V4L2 discovery is always enabled on Linux
if(UNIX AND NOT APPLE)
    target_sources(videostream PRIVATE
        lib/v4l2_discovery.c
    )
endif()

# G2D support (works with both GPU and DPU implementations)
option(ENABLE_G2D "Enable G2D 2D acceleration" ON)
if(ENABLE_G2D)
    find_library(G2D_LIBRARY g2d)
    if(G2D_LIBRARY)
        target_link_libraries(videostream PRIVATE ${G2D_LIBRARY})
        target_compile_definitions(videostream PRIVATE ENABLE_G2D)
    endif()
endif()
```

---

## 5. Testing Plan

### 5.1 Unit Tests

1. **Platform detection test:** Verify correct platform identification
2. **Device path test:** Verify correct device paths for each platform
3. **Format mapping test:** Verify fourcc conversion for each platform

### 5.2 Integration Tests (Requires Hardware)

1. **Encode test:** NV12 → H.264 on i.MX 95
2. **Decode test:** H.264 → NV12 on i.MX 95
3. **Zero-copy test:** Verify DMA-BUF sharing works
4. **Camera capture test:** (pending camera connection)

### 5.3 Cross-Platform Test Matrix

| Test | i.MX 8M Plus | i.MX 95 | x86_64 (SW) |
|------|--------------|---------|-------------|
| H.264 Encode | ✓ | ✓ | N/A |
| H.264 Decode | ✓ | ✓ | N/A |
| HEVC Encode | ✓ | ✓ | N/A |
| HEVC Decode | ✓ | ✓ | N/A |
| JPEG Encode | ✓ | ✓ | N/A |
| JPEG Decode | ✓ | ✓ | N/A |
| DMA-BUF | ✓ | ✓ | N/A |

---

## 6. Risks and Mitigations

### 6.1 Driver Maturity

**Risk:** Wave6 driver is newer and may have bugs or missing features.

**Mitigation:** 
- Test thoroughly with multiple resolutions and frame rates
- Maintain fallback to software encoding if needed
- Report issues upstream to NXP/kernel maintainers

### 6.2 No G2D Hardware

**Risk:** Format conversion without G2D may increase CPU usage.

**Mitigation:**
- Wave6 encoder supports many input formats natively (BGRA, RGBA, NV12, YUYV)
- For unsupported formats, use CPU conversion or require application to provide compatible format

### 6.3 Camera Integration Untested

**Risk:** ISI/MIPI CSI-2 integration is theoretical until camera is connected.

**Mitigation:**
- Design ISI capture support based on kernel driver interfaces
- Test with camera when available
- Support multiple ISI pipeline configurations

### 6.4 Device Numbering Instability

**Risk:** Video device numbers may change based on probe order or kernel config.

**Mitigation:**
- ✅ Use capability-based device discovery (not hardcoded paths)
- ✅ Match by driver name and role probing
- Support environment variable override for device paths (fallback)
- Support udev rules for stable device naming (optional)

---

## 7. SDK and Build Setup

### 7.1 Yocto SDK

Location: `/opt/yocto-sdk-imx95-frdm-6.12.49-2.2.0/`

Setup:
```bash
source /opt/yocto-sdk-imx95-frdm-6.12.49-2.2.0/environment-setup-armv8a-poky-linux
```

### 7.2 Cross-Compilation

```bash
# Source SDK environment
source /opt/yocto-sdk-imx95-frdm-6.12.49-2.2.0/environment-setup-armv8a-poky-linux

# Build VideoStream for i.MX 95
cmake -S . -B build-imx95 \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_IMX95=ON

cmake --build build-imx95 -j$(nproc)
```

### 7.3 Deployment

```bash
# Copy to target
scp -r build-imx95/libvideostream.so* root@10.10.41.59:/usr/lib/

# Test on target
ssh root@10.10.41.59 'LD_LIBRARY_PATH=/usr/lib videostream-test'
```

---

## 8. Timeline Estimate

| Phase | Description | Effort |
|-------|-------------|--------|
| 1 | V4L2 device discovery implementation | 1 day |
| 2 | Update backend selection | 0.5 days |
| 3 | Format handling (driver-specific quirks) | 1 day |
| 4 | Multi-planar buffer verification | 0.5 days |
| 5 | JPEG support (optional) | 1 day |
| 6 | G2D integration update | 0.5 days |
| 7 | Testing and fixes | 2 days |

**Total:** ~6-7 days (excluding camera integration)

---

## 9. Open Questions

1. **Camera module:** What camera will be used with the FRDM board? (IMX219, IMX477, OV5640?)
2. **ISI configuration:** Single or multi-camera setup?
3. **JPEG priority:** Is hardware JPEG encode/decode required for initial release?
4. **NPU integration:** Is Neutron NPU integration planned? (separate from codec support)

---

## 10. References

- [NXP i.MX 95 Applications Processor Reference Manual](https://www.nxp.com/docs/en/reference-manual/IMX95RM.pdf)
- [i.MX 95 Camera Porting Guide](~/Documents/i.MX/i.MX%2095%20Camera%20Porting%20Guide.pdf)
- [Linux V4L2 CODEC API Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-decoder.html)
- [Chips&Media Wave6 Codec Datasheet](https://www.chipsnmedia.com/)
- [VideoStream ARCHITECTURE.md](./ARCHITECTURE.md)

---

## 11. Camera and NeoISP Investigation (2026-01-30)

### 11.1 Test Hardware

- **Board:** NXP i.MX 95 19x19 EVK (IP: 10.10.41.142)
- **Camera:** EXPI-OS08A20 (8MP, MIPI CSI-2, 4-lane)
- **DTB:** `imx95-19x19-evk-os08a20-isp-it6263-lvds0.dtb`
- **BSP Version:** 6.12.49-lts-next

### 11.2 Camera Capture Status

| Feature | Status | Notes |
|---------|--------|-------|
| Camera Detection | ✅ Working | Green LED on module indicates power |
| Raw Bayer Capture | ✅ Working | SBGGR10 format via ISI |
| 4K (3840×2160) | ✅ 30 fps | Native sensor resolution |
| 1080p (1920×1080) | ✅ 60 fps | 2x2 binning mode |
| 720p (1280×720) | ✅ 60 fps | Binning mode |
| VGA (640×480) | ✅ 60 fps | Binning mode |
| 30-sec Sustained | ✅ Verified | 900 frames, no drops |
| YUV/RGB Output | ✅ Working | Hardware NeoISP with `LIBCAMERA_PIPELINES_MATCH_LIST` |

### 11.3 Media Topology

```
/dev/media3 (mxc-isi) - Camera Capture Pipeline:
  os08a20 2-0036 → csidev-4ad30000.csi → formatter@20 → crossbar → mxc_isi.0 → /dev/video0

/dev/media0 (neoisp) - ISP M2M Processing:
  neoisp-input0 (/dev/video11) → neoisp → neoisp-frame (/dev/video14)
  neoisp-params (/dev/video13) ─────────┘  └─→ neoisp-stats (/dev/video16)
```

### 11.4 NeoISP Status - ✅ WORKING

**Solution Found:** Use `LIBCAMERA_PIPELINES_MATCH_LIST` environment variable to force `nxp/neo` pipeline handler to match before `imx8-isi`:

```bash
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'
```

**Problem (Solved):** The `imx8-isi` pipeline handler was claiming the camera first, preventing `nxp/neo` from matching:
```
# Default behavior - imx8-isi matches first:
Found registered pipeline handler 'imx8-isi'     # ← Matches first (raw Bayer only)
Found registered pipeline handler 'nxp/neo'      # ← Never gets to try
```

**Working Architecture:**
```
Sensor → CSI-2 → ISI → NeoISP M2M → YUV/RGB output
   └── nxp/neo pipeline handler manages entire chain
```

**Performance Achieved:**
| Resolution | Format | Frame Rate | Notes |
|------------|--------|------------|-------|
| 1920×1080 | NV12 | **60 fps** | Hardware ISP with full 3A |
| 3840×2160 | NV12 | **30 fps** | 4K with hardware ISP |
| 1920×1080 | H.264 | **60 fps** | Full camera→ISP→encode pipeline |

### 11.5 NeoISP Device Details

| Device | V4L2 Path | Purpose | Formats |
|--------|-----------|---------|---------|
| neoisp-input0 | /dev/video11 | Raw Bayer input | RGGB/BGGR/GBRG/GRBG 8/10/12/14-bit |
| neoisp-input1 | /dev/video12 | HDR short exposure | Same as input0 |
| neoisp-params | /dev/video13 | ISP parameters | META_OUTPUT |
| neoisp-frame | /dev/video14 | YUV/RGB output | NV12, NV21, YUYV, RGB24, etc. |
| neoisp-ir | /dev/video15 | IR image output | GREY |
| neoisp-stats | /dev/video16 | 3A statistics | META_CAPTURE |

**IPA Tuning Files Available:**
- `/usr/share/libcamera/ipa/nxp/neo/os08a20.yaml` - Full ISP config with AWB, BLC, CCM, DRC, Gamma, HDR merge, AGC
- `/usr/share/libcamera/ipa/nxp/neo/uguzzi/database_os08a20_SDR_3840-2160_12bpp.bin`

### 11.6 Software Debayer Performance

Benchmarks using NumPy-based BGGR demosaicing on Cortex-A55:

| Resolution | Raw Capture | Debayer→RGB | Real-time @ 60fps? |
|------------|-------------|-------------|-------------------|
| 640×480 | 60 fps | **142 fps** | ✅ YES |
| 960×540 | 60 fps | **90 fps** | ✅ YES |
| 1280×720 | 60 fps | 52 fps | ❌ (works @ 30fps) |
| 1920×1080 | 60 fps | 22 fps | ❌ |
| 3840×2160 | 30 fps | 9 fps | ❌ |

**Conclusion:** Software debayer achieves real-time up to **960×540 @ 60fps**. Higher resolutions require hardware NeoISP.

### 11.7 Working Capture Commands

**NeoISP YUV capture (RECOMMENDED):**
```bash
# Set environment variable to use NeoISP pipeline handler
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'

# 1080p NV12 @ 60fps with hardware ISP
cam -c 1 --stream pixelformat=NV12,width=1920,height=1080 -C 100

# 4K NV12 @ 30fps with hardware ISP
cam -c 1 --stream pixelformat=NV12,width=3840,height=2160 -C 100

# Save to file
cam -c 1 --stream pixelformat=NV12,width=1920,height=1080 -C 100 --file=/tmp/yuv_#.bin
```

**GStreamer with NeoISP:**
```bash
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'

# Direct YUV capture with FPS display
gst-launch-1.0 libcamerasrc ! video/x-raw,format=NV12,width=1920,height=1080,framerate=60/1 ! \
    fpsdisplaysink text-overlay=false video-sink=fakesink sync=false

# Camera → NeoISP → H.264 encode → file
gst-launch-1.0 libcamerasrc ! video/x-raw,format=NV12,width=1920,height=1080,framerate=60/1 ! \
    v4l2h264enc extra-controls="controls,video_bitrate=8000000" ! h264parse ! \
    filesink location=output.h264

# 4K Camera → NeoISP → H.264 encode
gst-launch-1.0 libcamerasrc ! video/x-raw,format=NV12,width=3840,height=2160,framerate=30/1 ! \
    v4l2h264enc extra-controls="controls,video_bitrate=15000000" ! h264parse ! \
    filesink location=output_4k.h264
```

**Raw Bayer capture (without NeoISP):**
```bash
# Unset or don't set LIBCAMERA_PIPELINES_MATCH_LIST to use imx8-isi handler
unset LIBCAMERA_PIPELINES_MATCH_LIST

# 1080p @ 60fps raw
cam -c 1 --stream width=1920,height=1080 -C 100 --file=/tmp/raw_#.bin

# 4K @ 30fps raw (DNG format for viewing)
cam -c 1 -C 100 --file=/tmp/raw_#.dng
```

### 11.8 Known Issues

1. ~~**imx8-isi claims camera first**~~ - ✅ SOLVED with `LIBCAMERA_PIPELINES_MATCH_LIST`
2. ~~**NeoISP M2M disconnected**~~ - ✅ SOLVED, nxp/neo handler manages full pipeline
3. **No v4l2-ctl on target** - Makes debugging harder; use Python scripts instead

### 11.9 Remaining Work

1. ✅ **NeoISP YUV output working** - 1080p@60fps, 4K@30fps with hardware ISP
2. ✅ **GStreamer encode pipeline working** - Camera → NeoISP → H.264 @ 60fps verified
3. ✅ **4K NeoISP performance verified** - 4K NV12 @ 29.6fps
4. ✅ **Sustained encode verified** - 30-second (1800 frames) encode stable, 0 drops
5. ✅ **V4L2 Wave6 codec verified** - H.264 encode to MP4 working
6. **VSL GStreamer plugin issue** - vslsink requires DMA-BUF but doesn't negotiate properly (see 11.12)
7. **Integrate with VideoStream** - Add environment variable support for camera backend

### 11.12 GStreamer Camera/Encode Pipeline

**Working Pipeline (no explicit format caps):**
```bash
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'

# 1080p H.264 encode to file (10 seconds, 600 frames)
gst-launch-1.0 libcamerasrc ! "video/x-raw,width=1920,height=1080" ! \
    identity eos-after=600 ! \
    v4l2h264enc extra-controls="controls,video_bitrate=8000000" ! \
    h264parse ! mp4mux ! filesink location=output_1080p.mp4

# 4K H.264 encode (2 seconds, 60 frames)
gst-launch-1.0 libcamerasrc ! identity eos-after=60 ! \
    v4l2h264enc extra-controls="controls,video_bitrate=15000000" ! \
    h264parse ! mp4mux ! filesink location=output_4k.mp4
```

**Important Notes:**
1. **Do NOT specify format=NV12** - Let GStreamer auto-negotiate format (usually NV21)
2. **Use width/height only** - Explicit format caps can cause negotiation failures
3. **libcamerasrc outputs DMA-BUF** - v4l2h264enc can consume these directly

**V4L2 Encode Pipeline Verified:**
| Resolution | Bitrate | Format | Result |
|------------|---------|--------|--------|
| 1920×1080 | 8 Mbps | H.264 Baseline | ✅ Working |
| 3840×2160 | 15 Mbps | H.264 Baseline | ✅ Working |

**VSL GStreamer Plugin Status:**
- **vslsink** - Requires DMA-BUF memory but doesn't implement `propose_allocation`
- **Issue:** Pipeline fails with "not-supported" when using vslsink with libcamerasrc
- **Fix needed:** Add DMA-BUF allocator negotiation to vslsink
- **Workaround:** Use direct encode pipeline without VSL IPC

### 11.13 GStreamer Multi-Camera and Advanced Pipelines

**Prerequisites:**
```bash
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'
DISPLAY_W=640
DISPLAY_H=480
```

**Single Camera Preview (Wayland):**
```bash
gst-launch-1.0 \
    libcamerasrc camera-name="${CAMERA0}" ! \
    video/x-raw,format=YUY2 ! queue ! \
    waylandsink window-width=${DISPLAY_W} window-height=${DISPLAY_H}
```

**Multi-Camera Composition (4 cameras with G2D compositor):**
```bash
gst-launch-1.0 -v \
    imxcompositor_g2d name=comp \
        sink_0::xpos=0 sink_0::ypos=0 \
        sink_0::width=${DISPLAY_W} sink_0::height=${DISPLAY_H} \
        sink_1::xpos=0 sink_1::ypos=${DISPLAY_H} \
        sink_1::width=${DISPLAY_W} sink_1::height=${DISPLAY_H} \
        sink_2::xpos=${DISPLAY_W} sink_2::ypos=0 \
        sink_2::width=${DISPLAY_W} sink_2::height=${DISPLAY_H} \
        sink_3::xpos=${DISPLAY_W} sink_3::ypos=${DISPLAY_H} \
        sink_3::width=${DISPLAY_W} sink_3::height=${DISPLAY_H} ! \
    waylandsink \
    libcamerasrc camera-name="${CAMERA0}" ! \
        video/x-raw,format=YUY2 ! queue ! comp.sink_0 \
    libcamerasrc camera-name="${CAMERA1}" ! \
        video/x-raw,format=YUY2 ! queue ! comp.sink_1 \
    libcamerasrc camera-name="${CAMERA2}" ! \
        video/x-raw,format=YUY2 ! queue ! comp.sink_2 \
    libcamerasrc camera-name="${CAMERA3}" ! \
        video/x-raw,format=YUY2 ! queue ! comp.sink_3
```

**RGB-IR Dual Output (OX05B1S RGBIr sensor):**
```bash
gst-launch-1.0 \
    libcamerasrc camera-name="${CAMERA0}" name=src \
    src.src ! video/x-raw,format=YUY2 ! queue ! \
        waylandsink window-width=${DISPLAY_W} window-height=${DISPLAY_H} \
    src.src_0 ! video/x-raw,format=GRAY8 ! queue ! imxvideoconvert_g2d ! \
        waylandsink window-width=${DISPLAY_W} window-height=${DISPLAY_H}
```

**Smart Camera Multi-Stream (AP1302):**
```bash
gst-launch-1.0 -v \
    libcamerasrc camera-name="${CAMERA0}" name=src \
    src.src ! video/x-raw,format=YUY2,width=1280,height=800 ! queue ! \
        waylandsink window-width=${DISPLAY_W} window-height=${DISPLAY_H} \
    src.src_0 ! video/x-raw,format=NV12,width=640,height=400 ! queue ! \
        waylandsink window-width=320 window-height=240
```

### 11.14 libcamera cam Test Application

**List cameras:**
```bash
cam -l
```

**Show stream formats:**
```bash
cam --camera 1 -I
```

**Capture decoded frames to HDMI:**
```bash
# First disable Weston to release DRM/KMS
systemctl stop weston

cam --camera 1 \
    --stream width=${W},height=${H},pixelformat=YUYV \
    --capture=1000 \
    --display=HDMI-A-1
```

**Capture RAW frames to file:**
```bash
cam --camera 1 \
    --stream width=${W},height=${H},pixelformat=${RAW} \
    --capture=20 \
    --file=frame-#.raw
```

**Simultaneous RAW + RGB capture:**
```bash
cam --camera 1 \
    --stream width=${W},height=${H},pixelformat=${RAW},role=raw \
    --stream width=${W},height=${H},pixelformat=BGR888,role=video \
    --capture=20 \
    --file=frame-#.raw
```

**Note:** cam pixel formats use DRM convention (DRM BGR888 = V4L2 RGB888)

### 11.10 Direct V4L2 Capture (VSL Camera API)

#### Complete V4L2 Device Mapping

**Camera/ISI Devices:**
| Device | Name | Type | Purpose |
|--------|------|------|---------|
| /dev/video0 | mxc_isi.0.capture | CAPTURE_MPLANE | ISI channel 0 capture |
| /dev/video1 | mxc_isi.1.capture | CAPTURE_MPLANE | ISI channel 1 capture |
| /dev/video2-7 | mxc_isi.2-7.capture | CAPTURE_MPLANE | ISI channels 2-7 |
| /dev/video8 | mxc_isi.m2m | M2M_MPLANE | ISI scaler/converter |

**NeoISP Devices (8 instances, first at video11-16):**
| Device | Name | Type | Purpose |
|--------|------|------|---------|
| /dev/video11 | neoisp-input0 | OUTPUT_MPLANE | Raw Bayer input |
| /dev/video12 | neoisp-input1 | OUTPUT_MPLANE | HDR short exposure input |
| /dev/video13 | neoisp-params | META_OUTPUT | ISP parameters input |
| /dev/video14 | neoisp-frame | CAPTURE_MPLANE | YUV/RGB output |
| /dev/video15 | neoisp-ir | CAPTURE_MPLANE | IR image output |
| /dev/video16 | neoisp-stats | META_CAPTURE | 3A statistics output |

**Codec Devices:**
| Device | Name | Type | Purpose |
|--------|------|------|---------|
| /dev/video9 | wave6-dec | M2M_MPLANE | H.264/HEVC decoder |
| /dev/video10 | wave6-enc | M2M_MPLANE | H.264/HEVC encoder |
| /dev/video23 | mxc-jpeg-dec | M2M_MPLANE | JPEG decoder |
| /dev/video24 | mxc-jpeg-enc | M2M_MPLANE | JPEG encoder |

#### NeoISP Input Formats (neoisp-input0, /dev/video11)

All raw Bayer formats supported:
```
RGGB (0x42474752): 8-bit Bayer RGRG/GBGB
RG10 (0x30314752): 10-bit Bayer RGRG/GBGB
pRAA (0x41415270): 10-bit Bayer RGRG/GBGB Packed
RG12 (0x32314752): 12-bit Bayer RGRG/GBGB
RG14 (0x34314752): 14-bit Bayer RGRG/GBGB
RG16 (0x36314752): 16-bit Bayer RGRG/GBGB
BA81 (0x31384142): 8-bit Bayer BGBG/GRGR (SBGGR8)
BG10 (0x30314742): 10-bit Bayer BGBG/GRGR (SBGGR10) ← OS08A20 uses this
pBAA (0x41414270): 10-bit Bayer BGBG/GRGR Packed
BG12 (0x32314742): 12-bit Bayer BGBG/GRGR
BG14 (0x34314742): 14-bit Bayer BGBG/GRGR
BYR2 (0x32525942): 16-bit Bayer BGBG/GRGR
GBRG (0x47524247): 8-bit Bayer GBGB/RGRG
GB10 (0x30314247): 10-bit Bayer GBGB/RGRG
GB12 (0x32314247): 12-bit Bayer GBGB/RGRG
GB14 (0x34314247): 14-bit Bayer GBGB/RGRG
GB16 (0x36314247): 16-bit Bayer GBGB/RGRG
GRBG (0x47425247): 8-bit Bayer GRGR/BGBG
BA10 (0x30314142): 10-bit Bayer GRGR/BGBG
BA12 (0x32314142): 12-bit Bayer GRGR/BGBG
GR14 (0x34315247): 14-bit Bayer GRGR/BGBG
GR16 (0x36315247): 16-bit Bayer GRGR/BGBG
GREY (0x59455247): 8-bit Greyscale
Y10  (0x20303159): 10-bit Greyscale
Y12  (0x20323159): 12-bit Greyscale
Y14  (0x20343159): 14-bit Greyscale
Y16  (0x20363159): 16-bit Greyscale
```

#### NeoISP Output Formats (neoisp-frame, /dev/video14)

```
BGR3 (0x33524742): 24-bit BGR 8-8-8
RGB3 (0x33424752): 24-bit RGB 8-8-8
XR24 (0x34325258): 32-bit BGRX 8-8-8-8
XB24 (0x34324258): 32-bit RGBX 8-8-8-8
NV12 (0x3231564e): Y/UV 4:2:0 ← Recommended for encode
NV21 (0x3132564e): Y/VU 4:2:0
NV16 (0x3631564e): Y/UV 4:2:2
NV61 (0x3136564e): Y/VU 4:2:2
UYVY (0x59565955): UYVY 4:2:2
YUV3 (0x33565559): 24-bit YUV 4:4:4 8-8-8
YUVX (0x58565559): 32-bit YUVX 8-8-8-8
VUYX (0x58595556): 32-bit VUYX 8-8-8-8
YUYV (0x56595559): YUYV 4:2:2
VYUY (0x59555956): VYUY 4:2:2
GREY (0x59455247): 8-bit Greyscale
Y10  (0x20303159): 10-bit Greyscale
Y12  (0x20323159): 12-bit Greyscale
Y16  (0x20363159): 16-bit Greyscale
```

#### ISI M2M Formats (mxc_isi.m2m, /dev/video8)

**Input (OUTPUT_MPLANE) - No Bayer support:**
```
YUYV: YUYV 4:2:2
RGBP: 16-bit RGB 5-6-5
RGB3: 24-bit RGB 8-8-8
BGR3: 24-bit BGR 8-8-8
XR24: 32-bit BGRX 8-8-8-8
AR24: 32-bit BGRA 8-8-8-8
AB24: 32-bit RGBA 8-8-8-8
XB24: 32-bit RGBX 8-8-8-8
```

**Output (CAPTURE_MPLANE):**
```
YUYV, YUVA, NV12, NM12, NV16, NM16, YM24
RGBP, RGB3, BGR3, XR24, AR24, RA24, AB24, RX24, XB24, AR30
```

**Note:** The ISI M2M device is for scaling/color conversion only. It cannot do Bayer demosaicing. For raw Bayer sensors, NeoISP M2M is required.

#### ISI Capture Format Note

ISI capture (/dev/video0) advertises many formats including YUV/RGB, but the actual usable format depends on the upstream sensor:
- **OS08A20 (raw Bayer sensor):** Only Bayer formats work (SBGGR10/BG10)
- **YUV cameras:** YUV formats work directly without ISP

**Architecture Options for VSL Camera API:**

```
Option A: libcamera API (Recommended)
┌──────────────────────────────────────────────────────────────┐
│                       libcamera                               │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐   │
│  │ Camera  │───▶│   ISI   │───▶│ NeoISP  │───▶│   YUV   │   │
│  │ Manager │    │ Capture │    │  M2M    │    │ Buffers │   │
│  └─────────┘    └─────────┘    └─────────┘    └─────────┘   │
└──────────────────────────────────────────────────────────────┘
            │
            ▼
    VSL Camera API (wraps libcamera C++ API)
    - Set LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'
    - Request NV12/YUYV/RGB streams
    - Get zero-copy DMA-BUF frames

Option B: Manual V4L2 M2M Pipeline
┌───────────────┐     DMA-BUF     ┌───────────────┐
│  /dev/video0  │ ──────────────▶ │ /dev/video11  │
│  ISI Capture  │   (raw Bayer)   │ NeoISP Input  │
│  (SBGGR10)    │                 │               │
└───────────────┘                 └───────┬───────┘
                                          │
                                          ▼
                                  ┌───────────────┐
                                  │ /dev/video14  │
                                  │ NeoISP Output │
                                  │    (NV12)     │
                                  └───────────────┘
    - Requires media-ctl pipeline configuration
    - Manual buffer coordination between 3 devices
    - Must handle ISP parameters (3A, color matrices)
    - Complex but no libcamera dependency

Option C: Raw Capture + Software Debayer
┌───────────────┐     memcpy      ┌───────────────┐
│  /dev/video0  │ ──────────────▶ │   Software    │
│  ISI Capture  │   (raw Bayer)   │   Debayer     │
│  (SBGGR10)    │                 │   (CPU)       │
└───────────────┘                 └───────────────┘
    - Simplest implementation
    - Performance limited (~60fps at 960x540, ~22fps at 1080p)
    - No ISP tuning (AWB, exposure, etc.)
```

#### Verified V4L2 M2M Test Results

- All devices accept format configuration via `VIDIOC_S_FMT`
- Buffer allocation works on all devices via `VIDIOC_REQBUFS`
- Format negotiation: ISI→NeoISP via SBGGR10, NeoISP→Output via NV12
- NeoISP aligns height to 8 pixels (1080→1088)
- Tested at 640x480 and 1920x1080 resolutions

**V4L2 M2M Test Code (Python):**
```python
import fcntl, struct, os

VIDIOC_S_FMT = 0xc0d05605
VIDIOC_REQBUFS = 0xc0145608
V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9
V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE = 10
V4L2_MEMORY_MMAP = 1

def fourcc(s):
    return ord(s[0]) | (ord(s[1]) << 8) | (ord(s[2]) << 16) | (ord(s[3]) << 24)

def set_format(fd, buf_type, w, h, pixfmt):
    fmt = bytearray(208)
    struct.pack_into("IIIII", fmt, 0, buf_type, w, h, pixfmt, 1)  # type, w, h, fmt, field
    struct.pack_into("B", fmt, 28, 1 if pixfmt != fourcc("NV12") else 2)  # num_planes
    fcntl.ioctl(fd, VIDIOC_S_FMT, fmt)
    return struct.unpack_from("III", fmt, 4)

def request_buffers(fd, buf_type, count):
    req = bytearray(32)
    struct.pack_into("III", req, 0, count, buf_type, V4L2_MEMORY_MMAP)
    fcntl.ioctl(fd, VIDIOC_REQBUFS, req)
    return struct.unpack_from("I", req, 0)[0]

# Test device setup
isi = os.open("/dev/video0", os.O_RDWR)
neo_in = os.open("/dev/video11", os.O_RDWR)
neo_out = os.open("/dev/video14", os.O_RDWR)

set_format(isi, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 1920, 1080, fourcc("BG10"))
set_format(neo_in, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 1920, 1080, fourcc("BG10"))
set_format(neo_out, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 1920, 1080, fourcc("NV12"))

print(f"ISI buffers: {request_buffers(isi, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 4)}")
print(f"NeoISP in:   {request_buffers(neo_in, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 4)}")
print(f"NeoISP out:  {request_buffers(neo_out, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 4)}")
```

#### VSL Camera API Integration Options

| Option | Complexity | Performance | Dependencies | Recommended |
|--------|------------|-------------|--------------|-------------|
| **libcamera C++ API** | Medium | Best (60fps 1080p) | libcamera.so | ✅ Yes |
| **GStreamer libcamerasrc** | Low | Best (60fps 1080p) | GStreamer + libcamera | ✅ Yes |
| **Manual V4L2 M2M** | High | Best (60fps 1080p) | None | For special cases |
| **Raw + SW Debayer** | Low | Limited (22fps 1080p) | None | ≤960x540 only |

**Recommendation:** Use libcamera API via GStreamer (libcamerasrc) or direct C++ integration for production. The manual V4L2 M2M approach is possible but requires:
- Media controller link configuration (`media-ctl`)
- Buffer coordination between ISI capture and NeoISP M2M
- ISP parameter handling (3A algorithms via neoisp-params device)
- Proper sync between capture and processing pipelines

#### Media Controller Pipeline (Active)

```
os08a20 2-0036 (sensor)
    └─▶ csidev-4ad30000.csi (MIPI CSI-2 receiver)
          └─▶ 4ac10000.syscon:formatter@20 (pixel formatter)
                └─▶ crossbar (ISI routing)
                      └─▶ mxc_isi.0 (ISI channel 0)
                            └─▶ mxc_isi.0.capture (/dev/video0)
                                  Format: SBGGR10_1X10 1920x1080

neoisp (M2M ISP, separate media device /dev/media0)
    neoisp-input0 (/dev/video11) ──▶ neoisp ──▶ neoisp-frame (/dev/video14)
    neoisp-params (/dev/video13) ───────┘  └──▶ neoisp-stats (/dev/video16)
```

The nxp/neo libcamera pipeline handler coordinates both pipelines automatically when `LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi'` is set.

### 11.11 References

- [libcamera PIPELINES_MATCH_LIST patch](https://patchwork.libcamera.org/patch/19819/)
- [NXP i.MX 95 Camera Porting Guide](https://www.nxp.com/docs/en/user-guide/UG10215.pdf)
- [NXP libcamera fork](https://github.com/nxp-imx/libcamera)
- [NXP neo-ipa-uguzzi](https://github.com/nxp-imx/neo-ipa-uguzzi)

---

## Appendix A: V4L2 Device Probe Results

### A.0 Capability-Based Probe Output

```
V4L2 Device Probe Results
=========================

Device: /dev/video0
  Driver: wave6-dec
  Card: wave6-dec
  Type: CODEC/M2M
  Caps: M2M_MPLANE STREAMING 
  Role: DECODER (compressed -> raw)
  Codecs: H.264 HEVC 

Device: /dev/video1
  Driver: wave6-enc
  Card: wave6-enc
  Type: CODEC/M2M
  Caps: M2M_MPLANE STREAMING 
  Role: ENCODER (raw -> compressed)
  Codecs: H.264 HEVC 

Device: /dev/video2
  Driver: mxc-jpeg
  Card: mxc-jpeg codec
  Type: CODEC/M2M
  Caps: M2M_MPLANE STREAMING 
  Role: DECODER (compressed -> raw)
  Codecs: JPEG 

Device: /dev/video3
  Driver: mxc-jpeg
  Card: mxc-jpeg codec
  Type: CODEC/M2M
  Caps: M2M_MPLANE STREAMING 
  Role: ENCODER (raw -> compressed)
  Codecs: JPEG 
```

### A.1 Wave6 Decoder (video0)

```
Device: /dev/video0
  Driver: wave6-dec
  Card: wave6-dec
  Bus: platform:wave6-dec
  Version: 6.12.49
  Capabilities: 0x84204000
  Device Caps: 0x04204000
    VIDEO_M2M_MPLANE
    STREAMING

  OUTPUT formats:
    [0] HEVC - HEVC
    [1] H264 - H.264

  CAPTURE formats:
    [0] YU12 - Planar YUV 4:2:0
    [1] NV12 - Y/UV 4:2:0
    [2] NV21 - Y/VU 4:2:0
    [3] YM12 - Planar YUV 4:2:0 (N-C)
    [4] NM12 - Y/UV 4:2:0 (N-C)
    [5] NM21 - Y/VU 4:2:0 (N-C)
```

### A.2 Wave6 Encoder (video1)

```
Device: /dev/video1
  Driver: wave6-enc
  Card: wave6-enc
  Bus: platform:wave6-enc
  Version: 6.12.49
  Capabilities: 0x84204000
  Device Caps: 0x04204000
    VIDEO_M2M_MPLANE
    STREAMING

  OUTPUT formats: (28 formats - YUV, RGB, 10-bit)
  CAPTURE formats: HEVC, H264
```

### A.3 JPEG Decoder (video2)

```
Device: /dev/video2
  Driver: mxc-jpeg codec
  Card: mxc-jpeg codec
  Bus: platform:4c500000.jpegdec
  Version: 6.12.49

  OUTPUT formats: JPEG
  CAPTURE formats: BGR3, AR24, NM12, NV12, YUYV, YUV3, GREY, etc.
```

### A.4 JPEG Encoder (video3)

```
Device: /dev/video3
  Driver: mxc-jpeg codec
  Card: mxc-jpeg codec
  Bus: platform:4c550000.jpegenc
  Version: 6.12.49

  OUTPUT formats: BGR3, AR24, NM12, NV12, YUYV, YUV3, GREY, etc.
  CAPTURE formats: JPEG
```

### A.5 ISI Capture (video0 - with OS08A20 camera)

```
Device: /dev/video0
  Driver: mxc-isi
  Card: mxc-isi-cap
  Caps: 0xa4201000, DevCaps: 0x24201000
  Flags: VIDEO_CAPTURE_MPLANE, STREAMING

  CAPTURE_MPLANE formats (41 total):
    YUV: YUYV, YUVA, NV12, NM12, NV16, NM16, YM24
    RGB: RGBP, RGB3, BGR3, XR24, AR24, RA24, AB24, RX24, XB24, AR30
    Grey: GREY, Y10, Y12, Y14
    Bayer 8-bit: BA81, GBRG, GRBG, RGGB
    Bayer 10-bit: BG10, GB10, BA10, RG10
    Bayer 12-bit: BG12, GB12, BA12, RG12
    Bayer 14-bit: BG14, GB14, GR14, RG14
    Bayer 16-bit: BYR2, GB16, GR16, RG16
    Compressed: MJPG

  Note: With OS08A20, only Bayer formats produce valid data.
        YUV/RGB formats require a YUV-output camera.
```

### A.6 ISI M2M Scaler (video8)

```
Device: /dev/video8
  Driver: mxc-isi
  Card: mxc-isi-cap
  Type: M2M_MPLANE

  OUTPUT_MPLANE formats (8 - input to scaler):
    YUYV, RGBP, RGB3, BGR3, XR24, AR24, AB24, XB24
    Note: NO Bayer formats - cannot demosaic!

  CAPTURE_MPLANE formats (17 - output from scaler):
    YUV: YUYV, YUVA, NV12, NM12, NV16, NM16, YM24
    RGB: RGBP, RGB3, BGR3, XR24, AR24, RA24, AB24, RX24, XB24, AR30
```

### A.7 NeoISP M2M (video11-16, first instance)

```
Device: /dev/video11 (neoisp-input0)
  Driver: neoisp
  Type: OUTPUT_MPLANE (write raw Bayer here)
  Formats: 29 Bayer/Grey formats (8-16 bit, all patterns)

Device: /dev/video14 (neoisp-frame)
  Driver: neoisp
  Type: CAPTURE_MPLANE (read YUV/RGB here)
  Formats: 19 formats (NV12, NV21, NV16, NV61, YUYV, UYVY, VYUY,
           BGR3, RGB3, XR24, XB24, YUV3, YUVX, VUYX, GREY, Y10, Y12, Y16)
```

---

## Appendix B: Media Controller Topology

### B.1 Camera Pipeline (/dev/media3)

```
$ media-ctl -d /dev/media3 -p

Media device: FSL Capture Media Device (mxc-isi)
Bus: platform:4ad50000.isi

Entity Topology:
  os08a20 2-0036 (sensor, 3 pads)
      └─▶ pad0 [SBGGR10_1X10/1920x1080]
            │
  csidev-4ad30000.csi (CSI-2, 2 pads)
      └─▶ pad0 ◀── os08a20:0 [ENABLED]
      └─▶ pad1 [SBGGR10_1X10/1920x1080]
            │
  formatter@20 (2 pads)
      └─▶ pad0 ◀── csidev:1 [ENABLED,IMMUTABLE]
      └─▶ pad1 [SBGGR10_1X10/1920x1080]
            │
  crossbar (13 pads, routes sensor to ISI channels)
      └─▶ pad2 ◀── formatter:1 [ENABLED,IMMUTABLE]
      └─▶ pad5 ──▶ mxc_isi.0:0 [ENABLED,IMMUTABLE]
            │
  mxc_isi.0 (2 pads)
      └─▶ pad0 ◀── crossbar:5 [ENABLED,IMMUTABLE]
      └─▶ pad1 ──▶ mxc_isi.0.capture:0 [ENABLED,IMMUTABLE]
            │
  mxc_isi.0.capture (/dev/video0)
```

### B.2 NeoISP Pipeline (/dev/media0)

```
$ media-ctl -d /dev/media0 -p

Media device: neoisp
Bus: platform:4ae00000.isp

Entity Topology:
  neoisp (V4L2 subdev, 6 pads)
      pad0: SINK  ◀── neoisp-input0:0 [ENABLED,IMMUTABLE]
      pad1: SINK  ◀── neoisp-input1:0 (HDR)
      pad2: SINK  ◀── neoisp-params:0 [ENABLED]
      pad3: SOURCE ──▶ neoisp-frame:0 [ENABLED]
      pad4: SOURCE ──▶ neoisp-ir:0
      pad5: SOURCE ──▶ neoisp-stats:0 [ENABLED]

  neoisp-input0 (/dev/video11) - Raw Bayer input
  neoisp-input1 (/dev/video12) - HDR short exposure
  neoisp-params (/dev/video13) - ISP parameters (3A)
  neoisp-frame  (/dev/video14) - YUV/RGB output
  neoisp-ir     (/dev/video15) - IR output
  neoisp-stats  (/dev/video16) - 3A statistics
```
