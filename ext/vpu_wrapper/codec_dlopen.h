#pragma once

#include "hantro_dec/codec.h"

#include "hantro_dec/codec_avs.h"
#include "hantro_dec/codec_h264.h"
#include "hantro_dec/codec_hevc.h"
#include "hantro_dec/codec_jpeg.h"
#include "hantro_dec/codec_mpeg2.h"
#include "hantro_dec/codec_mpeg4.h"
#include "hantro_dec/codec_rv.h"
#include "hantro_dec/codec_vc1.h"
#include "hantro_dec/codec_vp6.h"
#include "hantro_dec/codec_vp8.h"
#include "hantro_dec/codec_vp9.h"
#include "hantro_dec/codec_webp.h"

int
Codec_OpenLib();
int
Codec_CloseLib();