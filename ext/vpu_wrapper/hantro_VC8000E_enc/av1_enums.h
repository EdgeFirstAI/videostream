/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_ENUMS_H_
#define AV1_COMMON_ENUMS_H_

//#include "./aom_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SW_NONE_FRAME    (-1)
#define SW_INTRA_FRAME   0
#define SW_LAST_FRAME    1
#define SW_LAST2_FRAME   2
#define SW_LAST3_FRAME   3
#define SW_GOLDEN_FRAME  4
#define SW_BWDREF_FRAME  5
#define SW_ALTREF2_FRAME 6
#define SW_ALTREF_FRAME  7

#define SW_EXTREF_FRAME     (SW_ALTREF_FRAME + 1)
#define SW_LAST_REF_FRAMES (SW_LAST3_FRAME - SW_LAST_FRAME + 1)
#define SW_INTER_REFS_PER_FRAME (SW_ALTREF_FRAME - SW_LAST_FRAME + 1)

#define MAX_MODE_LF_DELTAS 2


#define INTER_TX_SIZE_BUF_LEN 16
#define MAX_CTU_SIZE 64
#define MAX_COEFF_NUM          1024
#define ABOVE_MAX_MISIZE_Y     1024
#define ABOVE_MAX_MISIZE_UV    512
#define LEFT_MAX_MISIZE_Y      16
#define LEFT_MAX_MISIZE_UV     8

#define SW_REF_FRAMES_LOG2 3
#define SW_REF_FRAMES (1 << SW_REF_FRAMES_LOG2)
#define DEFAULT_EXPLICIT_ORDER_HINT_BITS 8

#define DEFAULT_DELTA_Q_RES 4
#define DEFAULT_DELTA_LF_RES 2
#define MI_SIZE_LOG2 2

// Maximum number of tile rows and tile columns
#define SW_MAX_TILE_ROWS 1024
#define SW_MAX_TILE_COLS 1024

#define PROFILE_BITS 3
// The following three profiles are currently defined.
// Profile 0.  8-bit and 10-bit 4:2:0 and 4:0:0 only.
// Profile 1.  8-bit and 10-bit 4:4:4
// Profile 2.  8-bit and 10-bit 4:2:2
//            12-bit  4:0:0, 4:2:2 and 4:4:4
// Since we have three bits for the profiles, it can be extended later.
typedef enum SW_BITSTREAM_PROFILE {
  SW_PROFILE_0,
  SW_PROFILE_1,
  SW_PROFILE_2,
  SW_MAX_PROFILES,
} SW_BITSTREAM_PROFILE;


// 4 scratch frames for the new frames to support a maximum of 4 cores decoding
// in parallel, 3 for scaled references on the encoder.
// TODO(hkuang): Add ondemand frame buffers instead of hardcoding the number
// of framebuffers.
// TODO(jkoleszar): These 3 extra references could probably come from the
// normal reference pool.
#define SW_FRAME_BUFFERS (SW_REF_FRAMES + 7)

#define IMPLIES(a, b) (!(a) || (b))  //  Logical 'a implies b' (or 'a -> b')

typedef enum SW_COMPOUND_DIST_WEIGHT_MODE {
  SW_DIST,
} SW_COMPOUND_DIST_WEIGHT_MODE;

// frame transform mode
typedef enum {
  SW_ONLY_4X4,         // use only 4x4 transform
  SW_TX_MODE_LARGEST,  // transform size is the largest possible for pu size
  SW_TX_MODE_SELECT,   // transform specified for each block
  SW_TX_MODES,
} SW_TX_MODE;


//#define COMPOUND_WEIGHT_MODE DIST
//#define FRAME_NUM_LIMIT (INT_MAX - MAX_FRAME_DISTANCE - 1)
//#define LEVEL_MAJOR_MAX ((1 << LEVEL_MAJOR_BITS) - 1)
/*
#define OP_POINTS_MINUS1_BITS 5


#define BLOCK_SIZE_GROUPS 4
#define TX_SIZE_CONTEXTS 3
#define TXK_TYPE_BUF_LEN 64
#define INTER_OFFSET(mode) ((mode)-NEARESTMV)
#define INTER_COMPOUND_OFFSET(mode) (u8)((mode)-NEAREST_NEARESTMV)

// Number of possible contexts for a color index.
// As can be seen from av1_get_palette_color_index_context(), the possible
// contexts are (2,0,0), (2,2,1), (3,2,0), (4,1,0), (5,0,0). These are mapped to
// a value from 0 to 4 using 'palette_color_index_context_lookup' table.
#define PALETTE_COLOR_INDEX_CONTEXTS 5

// Palette Y mode context for a block is determined by number of neighboring
// blocks (top and/or left) using a palette for Y plane. So, possible Y mode'
// context values are:
// 0 if neither left nor top block uses palette for Y plane,
// 1 if exactly one of left or top block uses palette for Y plane, and
// 2 if both left and top blocks use palette for Y plane.
#define PALETTE_Y_MODE_CONTEXTS 3

// Palette UV mode context for a block is determined by whether this block uses
// palette for the Y plane. So, possible values are:
// 0 if this block doesn't use palette for Y plane.
// 1 if this block uses palette for Y plane (i.e. Y palette size > 0).
#define PALETTE_UV_MODE_CONTEXTS 2

// Map the number of pixels in a block size to a context
//   64(BLOCK_8X8, BLOCK_4x16, BLOCK_16X4)  -> 0
//  128(BLOCK_8X16, BLOCK_16x8)             -> 1
//   ...
// 4096(BLOCK_64X64)                        -> 6
#define PALATTE_BSIZE_CTXS 7

#define KF_MODE_CONTEXTS 5

#define MAX_SEGMENTS 8 */
#define MAX_MB_PLANE 3 

/* Shift down with rounding for use when n >= 0, value >= 0 */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (((1 << (n)) >> 1))) >> (n))

// Note: Some enums use the attribute 'packed' to use smallest possible integer
// type, so that we can save memory when they are used in structs/arrays.
/*
// #define TX_PAD_2D \
//  ((MAX_TX_SIZE + TX_PAD_HOR) * (MAX_TX_SIZE + TX_PAD_VER) + TX_PAD_END)
*/
//#define SWITCHABLE_FILTER_CONTEXTS ((SWITCHABLE_FILTERS + 1) * 4)

#define SW_EXTREF_FRAME     (SW_ALTREF_FRAME + 1)
#define SW_LAST_REF_FRAMES (SW_LAST3_FRAME - SW_LAST_FRAME + 1)

#define SW_FWD_REFS (SW_GOLDEN_FRAME - SW_LAST_FRAME + 1)
#define SW_FWD_RF_OFFSET(ref) (ref - SW_LAST_FRAME)
#define SW_BWD_REFS (SW_ALTREF_FRAME - SW_BWDREF_FRAME + 1)
#define SW_BWD_RF_OFFSET(ref) (ref - SW_BWDREF_FRAME)

#define SW_SINGLE_REFS (SW_FWD_REFS + SW_BWD_REFS)

typedef enum {
    SW_IDENTITY = 0,      // identity transformation, 0-parameter
    SW_TRANSLATION = 1,   // translational motion 2-parameter
    SW_ROTZOOM = 2,       // simplified affine with rotation + zoom only, 4-parameter
    SW_AFFINE = 3,        // affine, 6-parameter
    SW_TRANS_TYPES,
} SW_TransformationType;

typedef enum {
    SW_EIGHTTAP_REGULAR,
    SW_EIGHTTAP_SMOOTH,
    SW_MULTITAP_SHARP,
    SW_BILINEAR,
    SW_INTERP_FILTERS_ALL,
    SW_SWITCHABLE_FILTERS = SW_BILINEAR,
    SW_SWITCHABLE = SW_SWITCHABLE_FILTERS + 1,
    SW_EXTRA_FILTERS = SW_INTERP_FILTERS_ALL - SW_SWITCHABLE_FILTERS,
} SW_InterpFilter;

#define SW_LOG_SWITCHABLE_FILTERS 2
/*
#define SWITCHABLE_FILTER_CONTEXTS ((SWITCHABLE_FILTERS + 1) * 4)
#define SW_INTER_FILTER_COMP_OFFSET (SWITCHABLE_FILTERS + 1)
#define SW_INTER_FILTER_DIR_OFFSET ((SWITCHABLE_FILTERS + 1) * 2)
*/
#define CDEF_MAX_STRENGTHS 16
#define MAX_LOOP_FILTER 63
/* Constant values while waiting for the sequence header */
#define FRAME_ID_LENGTH 15
#define DELTA_FRAME_ID_LENGTH 14

#define FRAME_CONTEXTS (FRAME_BUFFERS + 1)
// Extra frame context which is always kept at default values
#define FRAME_CONTEXT_DEFAULTS (FRAME_CONTEXTS - 1)
#define PRIMARY_REF_BITS 3
#define PRIMARY_REF_NONE 7

#define NUM_PING_PONG_BUFFERS 2

#define MAX_NUM_TEMPORAL_LAYERS 8
#define MAX_NUM_SPATIAL_LAYERS 4
/* clang-format off */
// clang-format seems to think this is a pointer dereference and not a
// multiplication.
#define MAX_NUM_OPERATING_POINTS \
  MAX_NUM_TEMPORAL_LAYERS * MAX_NUM_SPATIAL_LAYERS
/* clang-format on*/

// TODO(jingning): Turning this on to set up transform coefficient
// processing timer.
#define TXCOEFF_TIMER 0
#define TXCOEFF_COST_TIMER 0

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENUMS_H_
