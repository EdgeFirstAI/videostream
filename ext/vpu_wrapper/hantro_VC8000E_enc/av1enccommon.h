/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Description : AV1 Encoder common definitions for control code and system model
--
------------------------------------------------------------------------------*/

#ifndef __AV1_ENC_COMMON_H__
#define __AV1_ENC_COMMON_H__

#include "base_type.h"

typedef enum {
  AV1_EIGHTTAP,
  AV1_EIGHTTAP_SMOOTH,
  AV1_EIGHTTAP_SHARP,
  AV1_BILINEAR,
  AV1_SWITCHABLE,
  AV1_INTERP_FILTER_END = AV1_BILINEAR+1,
} Av1_InterpFilter;

// Table that converts 0-51 Q-range values passed in outside to the Qindex
// range used internally.
static const int quantizer_to_qindex[] = {
  1,  1,  2,  3,  4,  5,  6,  7,  8,  9,
  10, 12, 13, 16, 18, 21, 24, 27, 31, 35,
  40, 45, 51, 57, 64, 72, 82, 92, 99, 106,
  110,118,124,131,140,144,151,155,163,167,
  174,178,184,189,196,203,209,216,224,230,
  237,244};

#endif
