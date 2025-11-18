#ifndef _SOFT_VP9_ENUMS_H
#define _SOFT_VP9_ENUMS_H

#include "base_type.h"

#define VP9_FRAME_MARKER 0x2

#define VP9_SYNC_CODE_0 0x49
#define VP9_SYNC_CODE_1 0x83
#define VP9_SYNC_CODE_2 0x42

#define REFS_PER_FRAME 3

#define NONE -1
#define INTRA_FRAME 0
#define LAST_FRAME 1
#define GOLDEN_FRAME 2
#define ALTREF_FRAME 3
#define MAX_REF_FRAMES 4

#define EIGHTTAP 0
#define EIGHTTAP_SMOOTH 1
#define EIGHTTAP_SHARP 2
#define SWITCHABLE_FILTERS 3 /* Number of switchable filters */
#define BILINEAR 3
// The codec can operate in four possible inter prediction filter mode:
// 8-tap, 8-tap-smooth, 8-tap-sharp, and switching between the three.
#define SWITCHABLE_FILTER_CONTEXTS (SWITCHABLE_FILTERS + 1)
#define SWITCHABLE 4 /* should be the last one */

#define FRAME_CONTEXTS_LOG2 2
#define VP9_FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)


#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8


#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)


#define VP9_TX_SIZE_CONTEXTS 2

#define MV_CLASSES 11
#define CLASS0_BITS 1
#define CLASS0_SIZE (1 << CLASS0_BITS)
#define MV_OFFSET_BITS (MV_CLASSES + CLASS0_BITS - 2)

#define ENTROPY_NODES_PART1 4
#define ENTROPY_NODES_PART2 8
#define INTER_MODE_CONTEXTS 7

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 5
#define MBSKIP_CONTEXTS 3

#define BLOCK_TYPES 2
#define REF_TYPES 2  // intra=0, inter=1
#define COEF_BANDS 6
#define PREV_COEF_CONTEXTS 6
#define PREV_COEF_CONTEXTS_PAD 8

// block transform size
typedef u8 TX_SIZE;
typedef u8 vp9_prob;


#define TX_4X4 ((TX_SIZE)0)    // 4x4 transform
#define TX_8X8 ((TX_SIZE)1)    // 8x8 transform
#define TX_16X16 ((TX_SIZE)2)  // 16x16 transform
#define TX_32X32 ((TX_SIZE)3)  // 32x32 transform
#define TX_SIZE_MAX ((TX_SIZE)4)

#define VP9_DEF_UPDATE_PROB 252

#define COMP_PRED_CONTEXTS 2
#define MV_JOINTS 4

#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 2

#define VP9_MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (6 - VP9_MI_SIZE_LOG2)  // 64 = 2^6

#define MI_SIZE (1 << VP9_MI_SIZE_LOG2)              // pixels per mi-unit
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block

#define MI_MASK (MI_BLOCK_SIZE - 1)


#define MIN_TILE_WIDTH_B64 4
#define MAX_TILE_WIDTH_B64 64

// Maximum number of tile rows and tile columns
#define SW_MAX_TILE_ROWS 1024
#define SW_MAX_TILE_COLS 1024

#define VP9_MAX_TILE_WIDTH 4096
#define VP9_MAX_TILE_AREA (4096 * 2304)

typedef enum{
    ONLY_4X4 = 0,        // only 4x4 transform used
    ALLOW_8X8 = 1,       // allow block transform size up to 8x8
    ALLOW_16X16 = 2,     // allow block transform size up to 16x16
    ALLOW_32X32 = 3,     // allow block transform size up to 32x32
    TX_MODE_SELECT = 4,  // transform specified for each block
    TX_MODES = 5,
}TX_MODE;

enum MbPredictionMode {
  DC_PRED,  /* average of above and left pixels */
  V_PRED,   /* vertical prediction */
  H_PRED,   /* horizontal prediction */
  D45_PRED, /* Directional 45 deg prediction  [anti-clockwise from 0 deg hor] */
  D135_PRED, /* Directional 135 deg prediction [anti-clockwise from 0 deg hor]
                */
  D117_PRED, /* Directional 112 deg prediction [anti-clockwise from 0 deg hor]
                */
  D153_PRED, /* Directional 157 deg prediction [anti-clockwise from 0 deg hor]
                */
  D27_PRED, /* Directional 22 deg prediction  [anti-clockwise from 0 deg hor] */
  D63_PRED, /* Directional 67 deg prediction  [anti-clockwise from 0 deg hor] */
  TM_PRED,  /* Truemotion prediction */
  NEARESTMV,
  NEARMV,
  ZEROMV,
  NEWMV,
  SPLITMV,
  MB_MODE_COUNT
};

#define VP9_INTRA_MODES (TM_PRED + 1) /* 10 */

#define VP9_INTER_MODES (1 + NEWMV - NEARESTMV)

#define BLOCK_SIZE_GROUPS 4

enum FrameType {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  NUM_FRAME_TYPES,
};

enum PartitionType {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
  PARTITION_TYPES
};

#define PARTITION_PLOFFSET 4  // number of probability models per block size
#define NUM_PARTITION_CONTEXTS (4 * PARTITION_PLOFFSET)

#define VP9_SWITCHABLE_FILTERS 3 /* number of switchable filters */

#define PREDICTION_PROBS 3

#define MAX_MB_SEGMENTS 8
#define MB_SEG_TREE_PROBS (MAX_MB_SEGMENTS - 1)

/* The first nodes of the entropy probs are unconstrained, the rest are
 * modeled with statistic distribution. */
#define UNCONSTRAINED_NODES 3
#define MODEL_NODES (ENTROPY_NODES - UNCONSTRAINED_NODES)
#define PIVOT_NODE 2  // which node is pivot
#define COEFPROB_MODELS 128

typedef u32 vp9_coeff_count[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                           [UNCONSTRAINED_NODES + 1];
typedef u8 vp9_coeff_probs[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                          [UNCONSTRAINED_NODES];

#endif