# Hardware Compatibility Catalog

This document catalogs tested hardware configurations for VideoStream.

---

## NXP

### i.MX 8M Plus

**Architecture:** Cortex-A53 (quad) + Cortex-M7  
**Codec:** Hantro VC8000E (H.264/HEVC encode/decode)  
**Accelerators:** G2D, ISI, MIPI-CSI2

#### Tested Boards

| Board | Kernel | Status |
|-------|--------|--------|
| i.MX 8M Plus EVK | 6.12.34 | ✓ Tested |
| Au-Zone Maivin (Verdin i.MX 8M Plus) | 5.15.183-rt | ✓ Tested |

#### Cameras

##### OV5640 (OmniVision 5MP)

**Tested on:** i.MX 8M Plus EVK  
**Interface:** MIPI-CSI2  
**Pipeline:** OV5640 (integrated ISP) → mxc-mipi-csi2 → mxc_isi → /dev/video*

**Supported Resolutions:**

| Resolution | Aspect | 15fps | 30fps | 60fps |
|------------|--------|-------|-------|-------|
| 160x120 | 4:3 | ✓ | ✓ | ✗ |
| 176x144 | 11:9 | ✓ | ✓ | ✗ |
| 320x240 | 4:3 | ✓ | ✓ | ✗ |
| 640x360 | 16:9 | ✓ | ✓ | ✗ |
| 640x480 | 4:3 | ✓ | ✓ | ✓ |
| 720x480 | 3:2 | ✓ | ✓ | ✗ |
| 720x576 | 5:4 | ✓ | ✓ | ✗ |
| 1024x768 | 4:3 | ✓ | ✓ | ✗ |
| 1280x720 | 16:9 | ✓ | ✓ | ✗ |
| 1920x1080 | 16:9 | ✓ | ✓ | ✗ |
| 2592x1944 | 4:3 | ✓ | ✗ | ✗ |

**Supported Formats:** RGBP, RGB3, BGR3, YUYV, YUV4, NV12, NM12, YM24, XR24, AR24

**Notes:**
- Only 640x480 supports 60fps
- 2592x1944 (5MP) limited to 15fps
- Non-native resolutions (800x600, 960x540, 1280x960, 1600x1200) fail at STREAMON
- DMABUF export supported

##### OS08A20 (OmniVision 8MP)

**Tested on:** Au-Zone Maivin (Verdin i.MX 8M Plus)  
**Interface:** MIPI-CSI2  
**Pipeline:** OS08A20 → mxc-mipi-csi2 → mxc_isi → VeriSilicon ISP (viv_v4l2_device) → /dev/video3

**Supported Resolutions:**

| Resolution | Aspect | 30fps | Notes |
|------------|--------|-------|-------|
| 640x360 | 16:9 | ✓ | |
| 640x480 | 4:3 | ✓ | |
| 800x600 | 4:3 | ✓ | |
| 1024x768 | 4:3 | ✓ | |
| 1280x720 | 16:9 | ✓ | 720p HD |
| 1280x960 | 4:3 | ✓ | |
| 1600x1200 | 4:3 | ✓ | |
| 1920x1080 | 16:9 | ✓ | 1080p Full HD |
| 2048x1536 | 4:3 | ✓ | |
| 2560x1440 | 16:9 | ✓ | 1440p QHD |
| 2592x1944 | 4:3 | ✓ | 5MP |
| 3840x2160 | 16:9 | ✓ | 4K UHD |

**Supported Formats:** YUYV, NV12, NV16, BG12 (RAW Bayer)

**Notes:**
- Native sensor resolution: 3840x2160 (cropped from 8MP sensor)
- Stepwise resolution support: 176x144 to 4096x3072 (step 16x8)
- 960x540 adjusts to 960x544 (8-pixel height alignment)
- RAW Bayer (BG12) available on /dev/video3 only
- VeriSilicon ISP provides debayering, format conversion, and scaling

**Sensor Modes (from `sensor.query`):**

| Mode | Resolution | FPS | Bit Depth | HDR |
|------|------------|-----|-----------|-----|
| 0 | 1920x1080 | 60 | 10-bit | No |
| 1 | 1920x1080 | 30 | 10-bit | Yes |
| 2 | 3840x2160 | 30 | 12-bit | No (default) |
| 3 | 3840x2160 | 30 | 10-bit | Yes |

**VeriSilicon ISP JSON Control:**
- Custom ioctl via `V4L2_CID_VIV_EXTCTRL` (0x98f901)
- Commands use JSON format: `{"id":"command.name", ...}`
- Mirror/flip: `dwe.s.hflip`, `dwe.s.vflip`, `dwe.g.hflip`, `dwe.g.vflip`
- Sensor query: `sensor.query` (returns all modes with fps, resolution, bit depth)
- Mode switch: `sensor.s.mode` (must stop stream first)
- AE/AWB: `ae.g.cfg`, `ae.s.cfg`, `awb.g.cfg`, `awb.s.cfg`
- See i.MX 8M Plus Camera and Display Guide for full command list

---

### i.MX 95

**Architecture:** Cortex-A55 (hex) + Cortex-M33 + Cortex-M7  
**Codec:** Chips&Media Wave6 (H.264/HEVC encode/decode via V4L2)  
**Accelerators:** G2D (DPU-based), ISI M2M scaler, PXP, OpenCL, JPEG codec

#### Scaling Options

| Accelerator | Algorithm | Quality | Notes |
|-------------|-----------|---------|-------|
| ISI M2M | Polyphase filter | High | Downscale only, no Bayer |
| G2D (DPU) | Bilinear | Good | General purpose, rotation |
| OpenCL | Configurable | Best | Dewarp support |
| VPU | Built-in | Good | Decoder output only |

**Note:** NEO-ISP has no scaler - use ISI M2M or G2D for post-ISP scaling.

#### Tested Boards

| Board | Kernel | Status |
|-------|--------|--------|
| i.MX 95 FRDM | 6.12.49 | ✓ Codec probed, no camera |
| i.MX 95 19x19 EVK | 6.12.49 | ✓ Documented |
| i.MX 95 15x15 EVK | 6.12.49 | ✓ Documented |
| i.MX 95 Verdin | 6.12.49 | ✓ Documented |

#### V4L2 Codec Devices

| Device | Driver | Function | Formats |
|--------|--------|----------|---------|
| wave6-dec | wave6 | Decoder | H.264, HEVC |
| wave6-enc | wave6 | Encoder | H.264, HEVC |
| mxc-jpeg-dec | mxc-jpeg | JPEG Decoder | JPEG |
| mxc-jpeg-enc | mxc-jpeg | JPEG Encoder | JPEG |

**Notes:**
- Device paths vary; probe by driver name and capability
- See [IMX95.md](IMX95.md) for detailed codec investigation

#### Camera Subsystem

**Architecture:** NEO-ISP (M2M) + ISI + MIPI CSI-2

Unlike i.MX 8M Plus (VeriSilicon ISP), i.MX 95 uses:
- **NEO-ISP:** Memory-to-memory ISP for RAW Bayer processing
- **ISI:** Image Sensing Interface (up to 8 channels)
- **libcamera:** Required for pipeline management and 3A algorithms

**Device Tree Selection:** Camera support requires specific DTB files:

| Camera | EVK 19x19 | EVK 15x15 | Verdin |
|--------|-----------|-----------|--------|
| OX03C10 (4x GMSL) | imx95-19x19-evk-ox03c10-isp-*.dtb | imx95-15x15-evk-ox03c10-isp-*.dtb | imx95-19x19-verdin-ox03c10-isp-*.dtb |
| OS08A20 | imx95-19x19-evk-os08a20-isp-*.dtb | imx95-15x15-evk-os08a20-isp-*.dtb | imx95-19x19-verdin-os08a20-isp-*.dtb |
| OX05B1S | imx95-19x19-evk-ox05b1s-isp-*.dtb | imx95-15x15-evk-ox05b1s-isp-*.dtb | imx95-19x19-verdin-ox05b1s-isp-*.dtb |
| AP1302 (smart) | imx95-19x19-evk-adv7535-ap1302.dtb | — | imx95-15x15-verdin-adv7535-ap1302.dtb |

**Camera Pipeline:**
```
Sensor → MIPI CSI-2 → CSI Pixel Formatter → ISI → DDR
                                                    ↓
                                              NEO-ISP (M2M)
                                                    ↓
                                              Processed Frame
```

**NEO-ISP V4L2 Topology (M2M mode):**

| Device | Type | Function |
|--------|------|----------|
| neoisp-input0 | video_output | RAW frame input |
| neoisp-input1 | video_output | Short exposure (HDR) |
| neoisp-params | meta_output | 3A parameters input |
| neoisp-frame | video_capture | Processed frame output |
| neoisp-ir | video_capture | IR image output |
| neoisp-stats | meta_capture | 3A statistics output |

**NEO-ISP Output Formats:**
- YUV: NV12, NV21, NV16, NV61, YUYV, UYVY, VYUY, YUV24, YUVX32
- RGB: BGR24, RGB24, BGRX32, RGBX32
- Grey: GREY, Y10, Y12, Y16

**Supported Cameras (per NXP documentation):**

| Sensor | Type | Resolution | Features | DTB Suffix |
|--------|------|------------|----------|------------|
| OX03C10 | RAW | 1920x1280 | 3-exposure HDR, 180° FOV, GMSL | ox03c10-isp |
| OX05B1S | RAW | 2592x1944 | RGB-IR 4x4, 160° FOV | ox05b1s-isp |
| OS08A20 | RAW | 3840x2160 | 2-exposure HDR, 120-130° FOV | os08a20-isp |
| AP1302/AR0144 | Smart | 1280x800 | On-board ISP (YUV output) | ap1302 |
| OV5640 | Smart | 2592x1944 | On-board ISP (YUV output) | N/A (8M only) |

**Key Differences from i.MX 8M Plus:**
- No VeriSilicon ISP JSON control interface
- Uses libcamera + NXP NEO-ISP pipeline handler
- ISP is M2M (not inline with capture)
- NEO-ISP has no scaler - output must match sensor native resolution
- Supports GMSL deserializer for multi-camera

**libcamera Configuration:**
```bash
# Force NEO-ISP pipeline (required for RAW sensors)
export LIBCAMERA_PIPELINES_MATCH_LIST='nxp/neo,imx8-isi,simple'

# Enable debug logging
export LIBCAMERA_LOG_LEVELS='NxpNeo:DEBUG,ISI:DEBUG'

# Camera identifier (OS08A20 example)
CAMERA0="/base/soc/bus@42000000/i2c@42530000/os08a20_mipi@36"
```

**GStreamer with libcamera:**
```bash
# Single camera preview
gst-launch-1.0 libcamerasrc camera-name="${CAMERA0}" ! \
    video/x-raw,format=YUY2 ! waylandsink

# Camera to H.264 encode
gst-launch-1.0 libcamerasrc ! "video/x-raw,width=1920,height=1080" ! \
    v4l2h264enc extra-controls="controls,video_bitrate=8000000" ! \
    h264parse ! mp4mux ! filesink location=output.mp4
```

**Known Limitations:**
- OX03C10 may hang under heavy system load
- PipeWire service may conflict with OX03C10 (disable with `systemctl --user mask pipewire`)

#### Cameras

*No cameras tested yet (no camera connected to FRDM board)*

---

## Legend

- ✓ = Tested and working
- ✗ = Tested and not working / not supported
- — = Not yet tested
