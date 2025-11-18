/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef _VSI_VENDOR_EXT_H_
#define _VSI_VENDOR_EXT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "OMX_Index.h"

/* Second view frame flag:
 * This flag is set when the buffer content contains second view frame from MVC
 * stream
 *  @ingroup buf
 */
#define OMX_BUFFERFLAG_SECOND_VIEW 0x00010000

/* VP8 temporal layer frame flags:
 * One of these flags is set when the buffer contains encoded VP8 temporal layer
 * frame
 *  @ingroup buf
 */
#define OMX_BUFFERFLAG_BASE_LAYER 0x00020000
#define OMX_BUFFERFLAG_FIRST_LAYER 0x00040000
#define OMX_BUFFERFLAG_SECOND_LAYER 0x00080000
#define OMX_BUFFERFLAG_THIRD_LAYER 0x00100000

/* Struct for allocated buffer data. Carried in pInputPortPrivate */
typedef struct ALLOC_PRIVATE {
    OMX_U8* pBufferData; // Virtual address of the buffer
    OMX_U64 nBusAddress; // Physical address of the buffer
    OMX_U32 nBufferSize; // Allocated size
} ALLOC_PRIVATE;

/** Structure for RFC (reference frame compression) table data */
typedef struct RFC_TABLE {
    OMX_U8* pLumaBase;
    OMX_U64 nLumaBusAddress;
    OMX_U8* pChromaBase;
    OMX_U64 nChromaBusAddress;
} RFC_TABLE;

/* Struct for output buffer data. Carried in pOutputPortPrivate */
typedef struct OUTPUT_BUFFER_PRIVATE {
    OMX_U8*   pLumaBase;         // Virtual address of the luminance buffer
    OMX_U64   nLumaBusAddress;   // Physical address of the luminance buffer
    OMX_U32   nLumaSize;         // Size of the luminance data
    OMX_U8*   pChromaBase;       // Virtual address of the chrominance buffer
    OMX_U64   nChromaBusAddress; // Physical address of the chrominance buffer
    OMX_U32   nChromaSize;       // Size of the chrominance data
    RFC_TABLE sRfcTable;         // RFC table data (G2 only)
    OMX_U32   nBitDepthLuma;     // Luma component valid bit depth
    OMX_U32   nBitDepthChroma;   // Chroma component valid bit depth
    OMX_U32   nFrameWidth;       // Picture width in pixels
    OMX_U32   nFrameHeight;      // Picture height in pixels
    OMX_U32   nStride;           // Picture stride in bytes
    OMX_U32   nPicId[2];         // Identifier of the picture in decoding order
                       // For H264 interlace stream, nPicId[0]/nPicId[1] are
                       // used for top/bottom field */
    OMX_BOOL realloc;
    OMX_BOOL singleField; // Flag to indicate single field in output buffer
} OUTPUT_BUFFER_PRIVATE;

typedef enum OMX_INDEXVSITYPE {
    OMX_IndexVsiStartUnused = OMX_IndexVendorStartUnused + 0x00100000,
    OMX_IndexParamVideoMvcStream,
    OMX_IndexConfigVideoIntraArea,
    OMX_IndexConfigVideoRoiArea,
    OMX_IndexConfigVideoRoiDeltaQp,
    OMX_IndexConfigVideoAdaptiveRoi,
    OMX_IndexConfigVideoVp8TemporalLayers,
    OMX_IndexParamVideoHevc, /**< reference: OMX_VIDEO_PARAM_HEVCTYPE */
    OMX_IndexParamVideoVp9,  /**< reference: OMX_VIDEO_PARAM_VP9TYPE */
    OMX_IndexParamVideoG2Config,
    OMX_IndexParamVideoG1Config
} OMX_INDEXVSITYPE;

typedef enum OMX_VIDEO_CODINGVSITYPE {
    OMX_VIDEO_CodingVsiStartUnused =
        OMX_VIDEO_CodingVendorStartUnused + 0x00100000,
    OMX_VIDEO_CodingSORENSON,
    OMX_VIDEO_CodingDIVX,
    OMX_VIDEO_CodingDIVX3,
    OMX_VIDEO_CodingVP6,
    OMX_VIDEO_CodingAVS,
    OMX_VIDEO_CodingHEVC,
    OMX_VIDEO_CodingVP9
} OMX_VIDEO_CODINGVSITYPE;

typedef enum OMX_COLOR_FORMATVSITYPE {
    OMX_COLOR_FormatVsiStartUnused =
        OMX_COLOR_FormatVendorStartUnused + 0x00100000,
    OMX_COLOR_FormatYUV411SemiPlanar,
    OMX_COLOR_FormatYUV411PackedSemiPlanar,
    OMX_COLOR_FormatYUV440SemiPlanar,
    OMX_COLOR_FormatYUV440PackedSemiPlanar,
    OMX_COLOR_FormatYUV444SemiPlanar,
    OMX_COLOR_FormatYUV444PackedSemiPlanar,
    OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled, /* G2 tiled format */
    OMX_COLOR_FormatYUV420SemiPlanar8x4Tiled, /* G1 tiled format */
    OMX_COLOR_FormatYUV420SemiPlanarP010      /* MS P010 format */
} OMX_COLOR_FORMATVSITYPE;

/** Structure for configuring H.264 MVC mode */
typedef struct OMX_VIDEO_PARAM_MVCSTREAMTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bIsMVCStream;
} OMX_VIDEO_PARAM_MVCSTREAMTYPE;

/** Structure for configuring Intra area for 8290/H1/H2 encoder */
typedef struct OMX_VIDEO_CONFIG_INTRAAREATYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnable;
    OMX_U32         nTop;    /* Top mb row inside area [0..heightMbs-1]      */
    OMX_U32         nLeft;   /* Left mb row inside area [0..widthMbs-1]      */
    OMX_U32         nBottom; /* Bottom mb row inside area [top..heightMbs-1] */
    OMX_U32         nRight;  /* Right mb row inside area [left..widthMbs-1]  */
} OMX_VIDEO_CONFIG_INTRAAREATYPE;

/** Structure for configuring ROI area for 8290/H1/H2 encoder */
typedef struct OMX_VIDEO_CONFIG_ROIAREATYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnable;
    OMX_U32         nArea;   /* ROI area number [1..2]                       */
    OMX_U32         nTop;    /* Top mb row inside area [0..heightMbs-1]      */
    OMX_U32         nLeft;   /* Left mb row inside area [0..widthMbs-1]      */
    OMX_U32         nBottom; /* Bottom mb row inside area [top..heightMbs-1] */
    OMX_U32         nRight;  /* Right mb row inside area [left..widthMbs-1]  */
} OMX_VIDEO_CONFIG_ROIAREATYPE;

/** Structure for configuring ROI Delta QP for 8290/H1/H2 encoder */
typedef struct OMX_VIDEO_CONFIG_ROIDELTAQPTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_U32         nArea;    /* ROI area number [1..2]               */
    OMX_S32         nDeltaQP; /* QP delta value [-127..0] for VP8     */
                              /*                [-15..0]  for H264    */
} OMX_VIDEO_CONFIG_ROIDELTAQPTYPE;

/** Structure for configuring Adaptive ROI for H1 encoder */
typedef struct OMX_VIDEO_CONFIG_ADAPTIVEROITYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_S32         nAdaptiveROI; /* QP delta for adaptive ROI [-51..0]       */
    OMX_S32 nAdaptiveROIColor;    /* Color temperature for the Adaptive ROI   */
                                  /* -10=2000K, 0=3000K, 10=5000K             */
} OMX_VIDEO_CONFIG_ADAPTIVEROITYPE;

/** Structure for configuring VP8 temporal layers */
typedef struct OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_U32 nBaseLayerBitrate; /* Bits per second [10000..40000000]    */
    OMX_U32 nLayer1Bitrate;    /* Bits per second [10000..40000000]    */
    OMX_U32 nLayer2Bitrate;    /* Bits per second [10000..40000000]    */
    OMX_U32 nLayer3Bitrate;    /* Bits per second [10000..40000000]    */
} OMX_VIDEO_CONFIG_VP8TEMPORALLAYERTYPE;

typedef enum OMX_VIDEO_HEVCPROFILETYPE {
    OMX_VIDEO_HEVCProfileMain   = 0x01, /**< Main profile */
    OMX_VIDEO_HEVCProfileMain10 = 0x02, /**< Main10 profile */
    OMX_VIDEO_HEVCProfileMainStillPicture =
        0x04, /**< Main still picture profile */
    OMX_VIDEO_HEVCProfileKhronosExtensions =
        0x6F000000, /**< Reserved region for introducing Khronos Standard
                       Extensions */
    OMX_VIDEO_HEVCProfileVendorStartUnused =
        0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_HEVCProfileMax = 0x7FFFFFFF
} OMX_VIDEO_HEVCPROFILETYPE;

typedef enum OMX_VIDEO_HEVCLEVELTYPE {
    OMX_VIDEO_HEVCLevel1  = 0x01,   /**< Level 1   */
    OMX_VIDEO_HEVCLevel2  = 0x02,   /**< Level 2   */
    OMX_VIDEO_HEVCLevel21 = 0x04,   /**< Level 2.1 */
    OMX_VIDEO_HEVCLevel3  = 0x08,   /**< Level 3   */
    OMX_VIDEO_HEVCLevel31 = 0x10,   /**< Level 3.1 */
    OMX_VIDEO_HEVCLevel4  = 0x20,   /**< Level 4   */
    OMX_VIDEO_HEVCLevel41 = 0x40,   /**< Level 4.1 */
    OMX_VIDEO_HEVCLevel5  = 0x80,   /**< Level 5   */
    OMX_VIDEO_HEVCLevel51 = 0x100,  /**< Level 5.1 */
    OMX_VIDEO_HEVCLevel52 = 0x200,  /**< Level 5.2 */
    OMX_VIDEO_HEVCLevel6  = 0x400,  /**< Level 6   */
    OMX_VIDEO_HEVCLevel61 = 0x800,  /**< Level 6.1 */
    OMX_VIDEO_HEVCLevel62 = 0x1000, /**< Level 6.2 */
    OMX_VIDEO_HEVCLevelKhronosExtensions =
        0x6F000000, /**< Reserved region for introducing Khronos Standard
                       Extensions */
    OMX_VIDEO_HEVCLevelVendorStartUnused =
        0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_HEVCLevelMax = 0x7FFFFFFF
} OMX_VIDEO_HEVCLEVELTYPE;

typedef struct OMX_VIDEO_PARAM_HEVCTYPE {
    OMX_U32                   nSize;
    OMX_VERSIONTYPE           nVersion;
    OMX_U32                   nPortIndex;
    OMX_VIDEO_HEVCPROFILETYPE eProfile;
    OMX_VIDEO_HEVCLEVELTYPE   eLevel;
    OMX_U32                   nPFrames;
    OMX_U32                   nRefFrames;
    OMX_U32                   nBitDepthLuma;
    OMX_U32                   nBitDepthChroma;
    OMX_BOOL                  bStrongIntraSmoothing;
    OMX_S32                   nTcOffset;
    OMX_S32                   nBetaOffset;
    OMX_BOOL                  bEnableDeblockOverride;
    OMX_BOOL                  bDeblockOverride;
    OMX_BOOL                  bEnableSAO;
    OMX_BOOL                  bEnableScalingList;
    OMX_BOOL                  bCabacInitFlag;
} OMX_VIDEO_PARAM_HEVCTYPE;

/** VP9 profiles */
typedef enum OMX_VIDEO_VP9PROFILETYPE {
    OMX_VIDEO_VP9Profile0 = 0x01, /* 8-bit 4:2:0 */
    OMX_VIDEO_VP9Profile1 = 0x02, /* 8-bit 4:2:2, 4:4:4, alpha channel */
    OMX_VIDEO_VP9Profile2 =
        0x04, /* 10-bit/12-bit 4:2:0, YouTube Premium Content Profile */
    OMX_VIDEO_VP9Profile3 =
        0x08, /* 10-bit/12-bit 4:2:2, 4:4:4, alpha channel */
    OMX_VIDEO_VP9ProfileUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP9ProfileKhronosExtensions =
        0x6F000000, /**< Reserved region for introducing Khronos Standard
                       Extensions */
    OMX_VIDEO_VP9ProfileVendorStartUnused =
        0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VP9ProfileMax = 0x7FFFFFFF
} OMX_VIDEO_VP9PROFILETYPE;

/** VP9 Param */
typedef struct OMX_VIDEO_PARAM_VP9TYPE {
    OMX_U32                  nSize;
    OMX_VERSIONTYPE          nVersion;
    OMX_U32                  nPortIndex;
    OMX_VIDEO_VP9PROFILETYPE eProfile;
    OMX_U32                  nBitDepthLuma;
    OMX_U32                  nBitDepthChroma;
} OMX_VIDEO_PARAM_VP9TYPE;

/** G2 Decoder pixel formats */
typedef enum OMX_VIDEO_G2PIXELFORMAT {
    OMX_VIDEO_G2PixelFormat_Default = 0x0,
    OMX_VIDEO_G2PixelFormat_8bit =
        0x01, /* 10 bit data is clamped to 8 bit per pixel */
    OMX_VIDEO_G2PixelFormat_P010    = 0x02, /* MS P010 format */
    OMX_VIDEO_G2PixelFormat_Custom1 = 0x03
} OMX_VIDEO_G2PIXELFORMAT;

/** Structure for configuring G2 decoder */
typedef struct OMX_VIDEO_PARAM_G2CONFIGTYPE {
    OMX_U32                 nSize;
    OMX_VERSIONTYPE         nVersion;
    OMX_U32                 nPortIndex;
    OMX_BOOL                bEnableTiled;
    OMX_VIDEO_G2PIXELFORMAT ePixelFormat;
    OMX_BOOL                bEnableRFC;
    OMX_U32                 nGuardSize;
    OMX_BOOL                bEnableAdaptiveBuffers;
    OMX_U32                 bEnableSecureMode;
    OMX_U32                 bEnableCtsTest; /* used for VP9 adaptive CTS test */
    OMX_BOOL                bEnableRingBuffer;
    OMX_BOOL                bEnableFetchOnePic;
} OMX_VIDEO_PARAM_G2CONFIGTYPE;

/** Structure for configuring G1 decoder */
typedef struct OMX_VIDEO_PARAM_G1CONFIGTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bEnableTiled; /* Store reference pictures in tiled format */
    OMX_BOOL bAllowFieldDBP; /* If the stream is interlaced, use field DPB */
    OMX_U32  nGuardSize;
    OMX_BOOL bEnableAdaptiveBuffers;
    OMX_U32  bEnableSecureMode;
} OMX_VIDEO_PARAM_G1CONFIGTYPE;

typedef union OMX_VIDEO_PARAM_CONFIGTYPE {
    OMX_VIDEO_PARAM_G1CONFIGTYPE g1_conf;
    OMX_VIDEO_PARAM_G2CONFIGTYPE g2_conf;
} OMX_VIDEO_PARAM_CONFIGTYPE;
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _VSI_VENDOR_EXT_H_
