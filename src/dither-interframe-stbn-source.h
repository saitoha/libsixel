/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Shared STBN interframe source selection and sampling hooks.
 */
#ifndef LIBSIXEL_DITHER_INTERFRAME_STBN_SOURCE_H
#define LIBSIXEL_DITHER_INTERFRAME_STBN_SOURCE_H

#include <stdint.h>
#include "dither.h"
#include "lookup-common.h"

#define SIXEL_INTERFRAME_STBN_SOURCE_HASH 0
#define SIXEL_INTERFRAME_STBN_SOURCE_MASK 1
#define SIXEL_INTERFRAME_STBN_SOURCE_PMJ  2

#define SIXEL_INTERFRAME_PMJ_TILE_SIDE 64
#define SIXEL_INTERFRAME_PMJ_TILE_PIXELS \
    (SIXEL_INTERFRAME_PMJ_TILE_SIDE * SIXEL_INTERFRAME_PMJ_TILE_SIDE)
#define SIXEL_INTERFRAME_PMJ_TILE_ENABLE_MIN_PIXELS 1024
#define SIXEL_INTERFRAME_FLOAT_LUT_SIZE 256

#define SIXEL_INTERFRAME_NOISE_STRENGTH_DEFAULT 0.055f

/*
 * Shared v1 STBN strength in byte-domain units.
 *
 *  - 0 disables STBN bias.
 *  - 24 keeps visible decorrelation while preserving palette locality.
 */
#define SIXEL_INTERFRAME_STBN_V1_STRENGTH_U8 24

typedef uint16_t (*sixel_interframe_stbn_sample_u16_fn)(
    uint32_t sequence_index,
    int x,
    int y,
    int channel,
    int depth);

typedef struct sixel_interframe_stbn_pmj_channel_cache_common {
    uint32_t seed;
    uint32_t coord_key_x;
    uint32_t coord_key_y;
    uint32_t rank_key;
    uint32_t offset_x;
    uint32_t offset_y;
} sixel_interframe_stbn_pmj_channel_cache_common_t;

typedef struct sixel_interframe_stbn_state_common {
    uint32_t sequence_index;
    uint8_t sample_source_id;
    int stbn_strength_u8;
    sixel_interframe_stbn_sample_u16_fn sample_u16;
    int pmj_cache_valid;
    int pmj_cache_depth;
    uint32_t pmj_cache_sequence_index;
    int pmj_row_cache_valid;
    int pmj_row_cache_depth;
    uint32_t pmj_row_cache_sequence_index;
    int pmj_row_cache_y;
    uint16_t pmj_row_u16[SIXEL_MAX_CHANNELS][SIXEL_INTERFRAME_PMJ_TILE_SIDE];
    int pmj_float_lut_valid;
    int pmj_float_lut_pixelformat;
    int pmj_float_lut_depth;
    float pmj_float_u8_to_float[SIXEL_MAX_CHANNELS]
                               [SIXEL_INTERFRAME_FLOAT_LUT_SIZE];
    int pmj_tile_valid;
    int pmj_tile_depth;
    uint32_t pmj_tile_sequence_index;
    int pmj_tile_enabled;
    uint16_t pmj_tile_u16[SIXEL_MAX_CHANNELS][SIXEL_INTERFRAME_PMJ_TILE_PIXELS];
    sixel_interframe_stbn_pmj_channel_cache_common_t
        pmj_channel_cache[SIXEL_MAX_CHANNELS];
} sixel_interframe_stbn_state_common_t;

typedef SIXELSTATUS (*sixel_interframe_stbn_prepare_state_common_fn)(
    sixel_dither_t const *dither,
    sixel_interframe_stbn_state_common_t *stbn_state,
    int can_update);

typedef struct sixel_interframe_stbn_source_backend_common {
    uint8_t source_id;
    sixel_interframe_stbn_sample_u16_fn sample_u16;
    sixel_interframe_stbn_prepare_state_common_fn prepare_state;
} sixel_interframe_stbn_source_backend_common_t;

int
sixel_interframe_stbn_wrap_tile_coord_common(int value, int tile_size);

uint16_t
sixel_interframe_stbn_sample_u8_to_u16_common(uint8_t sample_u8);

int
sixel_interframe_stbn_state_uses_source_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    uint8_t source_id);

int32_t
sixel_interframe_stbn_sample_centered_u16_common(uint16_t sample_u16);

int32_t
sixel_interframe_stbn_sample_centered_state_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth);

int32_t
sixel_interframe_stbn_bias_u8_from_centered_common(int32_t centered,
                                                  int strength_u8);

/*
 * Hot-path helper: convert one 16-bit sample to signed byte-domain bias
 * with symmetric rounding, matching sixel_interframe_stbn_bias_u8_from_centered
 * behavior without cross-translation-unit calls.
 */
static inline int32_t
sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(uint16_t sample_u16,
                                                           int strength_u8)
{
    int32_t centered;
    int64_t bias;

    centered = 0;
    bias = 0;
    centered = (int32_t)sample_u16 - 32768;
    bias = (int64_t)centered * (int64_t)strength_u8;
    if (bias >= 0) {
        bias = (bias + 16384) / 32768;
    } else {
        bias = (bias - 16384) / 32768;
    }

    return (int32_t)bias;
}

int32_t
sixel_interframe_stbn_bias_u8_state_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8);

uint16_t
sixel_interframe_stbn_sample_hash_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth);

uint16_t
sixel_interframe_stbn_sample_mask_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth);

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_cached_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth);

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_tiled_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth);

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_row_cached_common(
    sixel_interframe_stbn_state_common_t *stbn_state,
    int x,
    int y,
    int channel,
    int depth);

SIXELSTATUS
sixel_interframe_stbn_prepare_state_default_common(
    sixel_dither_t const *dither,
    sixel_interframe_stbn_state_common_t *stbn_state,
    int can_update);

sixel_interframe_stbn_source_backend_common_t const *
sixel_interframe_stbn_source_backend_from_id_common(uint8_t source_id);

sixel_interframe_stbn_source_backend_common_t const *
sixel_interframe_stbn_source_backend_from_token_common(int strategy_token);

sixel_interframe_stbn_sample_u16_fn
sixel_interframe_stbn_sample_fn_from_source_id_common(uint8_t source_id);

uint16_t
sixel_interframe_stbn_sample_u16_state_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth);

#endif /* LIBSIXEL_DITHER_INTERFRAME_STBN_SOURCE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
