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

#ifndef HANTRO_DECODER_VERSION_H
#define HANTRO_DECODER_VERSION_H

#define COMPONENT_VERSION_MAJOR    2
#define COMPONENT_VERSION_MINOR    0
#define COMPONENT_VERSION_REVISION 23
#define COMPONENT_VERSION_STEP     0

#ifdef IS_G1_DECODER
#define COMPONENT_NAME_VIDEO      "OMX.hantro.G1.video.decoder"
#define COMPONENT_NAME_IMAGE      "OMX.hantro.G1.image.decoder"
#elif IS_G2_DECODER
#define COMPONENT_NAME_VIDEO      "OMX.hantro.G2.video.decoder"
#define COMPONENT_NAME_IMAGE      "OMX.hantro.G2.image.decoder"
#else
#define COMPONENT_NAME_VIDEO      "OMX.hantro.81x0.video.decoder"
#define COMPONENT_NAME_IMAGE      "OMX.hantro.81x0.image.decoder"
#endif

/** Role 1 - MPEG4 Decoder */
#define COMPONENT_NAME_MPEG4 "OMX.hantro.G1.video.decoder.mpeg4"
#define COMPONENT_ROLE_MPEG4 "video_decoder.mpeg4"

/** Role 2 - H264 Decoder */
#define COMPONENT_NAME_H264  "OMX.hantro.G1.video.decoder.avc"
#define COMPONENT_ROLE_H264  "video_decoder.avc"

/** Role 3 - H263 Decoder */
#define COMPONENT_NAME_H263  "OMX.hantro.G1.video.decoder.h263"
#define COMPONENT_ROLE_H263  "video_decoder.h263"

/** Role 4 - VC1 Decoder */
#define COMPONENT_NAME_VC1   "OMX.hantro.G1.video.decoder.wmv"
#define COMPONENT_ROLE_VC1   "video_decoder.wmv"

/** Role 5 - MPEG2 Decoder */
#define COMPONENT_NAME_MPEG2 "OMX.hantro.G1.video.decoder.mpeg2"
#define COMPONENT_ROLE_MPEG2 "video_decoder.mpeg2"

/** Role 6 - AVS Decoder */
#define COMPONENT_NAME_AVS  "OMX.hantro.G1.video.decoder.avs"
#define COMPONENT_ROLE_AVS  "video_decoder.avs"

/** Role 7 - VP8 Decoder */
#define COMPONENT_NAME_VP8  "OMX.hantro.G1.video.decoder.vp8"
#define COMPONENT_ROLE_VP8  "video_decoder.vp8"

/** Role 8 - VP6 Decoder */
#define COMPONENT_NAME_VP6 "OMX.hantro.G1.video.decoder.vp6"
#define COMPONENT_ROLE_VP6 "video_decoder.vp6"

/** Role 9 - MJPEG Decoder */
#define COMPONENT_NAME_MJPEG  "OMX.hantro.G1.video.decoder.jpeg"
#define COMPONENT_ROLE_MJPEG  "video_decoder.jpeg"

/** Role 10 - RV Decoder */
#define COMPONENT_NAME_RV "OMX.hantro.G1.video.decoder.rv"
#define COMPONENT_ROLE_RV "video_decoder.rv"

/** Role 11 - Post Processor in standalone mode */
#define COMPONENT_NAME_PP "OMX.hantro.G1.video.decoder.pp"
#define COMPONENT_ROLE_PP "video_decoder.pp"

/** Role 12 - HEVC Decoder */
#define COMPONENT_NAME_HEVC "OMX.hantro.G2.video.decoder.hevc"
#define COMPONENT_ROLE_HEVC "video_decoder.hevc"

/** Role 13 - VP9 Decoder */
#define COMPONENT_NAME_VP9 "OMX.hantro.G2.video.decoder.vp9"
#define COMPONENT_ROLE_VP9 "video_decoder.vp9"

/* Image decoders */
/** Role 1 - JPEG Decoder */
#define COMPONENT_NAME_JPEG  "OMX.hantro.G1.image.decoder.jpeg"
#define COMPONENT_ROLE_JPEG  "image_decoder.jpeg"

/** Role 2 - WEBP Decoder */
#define COMPONENT_NAME_WEBP  "OMX.hantro.G1.image.decoder.webp"
#define COMPONENT_ROLE_WEBP  "image_decoder.webp"

#endif // HANTRO_DECODER_VERSION_H
