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

#include <stdint.h>
#include "dither-interframe-stbn-source-pmj.h"

/*
 * Keep PMJ deterministic and thread-stable. This source builds a progressive
 * 64x64 stratum from sequence/channel/depth, then applies reversible scrambles
 * over tile coordinates so spatial structure stays balanced while sequence
 * changes reduce frame-to-frame correlation.
 */
static uint32_t
sixel_interframe_stbn_pmj_mix_u32_common(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;

    return value;
}

static uint32_t
sixel_interframe_stbn_pmj_permute6_common(uint32_t value, uint32_t key)
{
    value &= 63U;
    value ^= key & 63U;
    value *= 0xe95e1dd5U;
    value &= 63U;
    value ^= value >> 3;
    value ^= value >> 5;
    value ^= (key >> 9) & 63U;
    value *= 0x7feb352dU;
    value &= 63U;

    return value;
}

static uint32_t
sixel_interframe_stbn_pmj_permute12_common(uint32_t value, uint32_t key)
{
    value &= 4095U;
    value ^= key & 4095U;
    value *= 0xe95e1dd5U;
    value &= 4095U;
    value ^= value >> 3;
    value ^= value >> 5;
    value ^= (key >> 9) & 4095U;
    value *= 0x7feb352dU;
    value &= 4095U;

    return value;
}

static uint16_t const
sixel_interframe_stbn_pmj_part1by1_lut6_common[] = {
    0x0000U, 0x0001U, 0x0004U, 0x0005U, 0x0010U, 0x0011U, 0x0014U, 0x0015U,
    0x0040U, 0x0041U, 0x0044U, 0x0045U, 0x0050U, 0x0051U, 0x0054U, 0x0055U,
    0x0100U, 0x0101U, 0x0104U, 0x0105U, 0x0110U, 0x0111U, 0x0114U, 0x0115U,
    0x0140U, 0x0141U, 0x0144U, 0x0145U, 0x0150U, 0x0151U, 0x0154U, 0x0155U,
    0x0400U, 0x0401U, 0x0404U, 0x0405U, 0x0410U, 0x0411U, 0x0414U, 0x0415U,
    0x0440U, 0x0441U, 0x0444U, 0x0445U, 0x0450U, 0x0451U, 0x0454U, 0x0455U,
    0x0500U, 0x0501U, 0x0504U, 0x0505U, 0x0510U, 0x0511U, 0x0514U, 0x0515U,
    0x0540U, 0x0541U, 0x0544U, 0x0545U, 0x0550U, 0x0551U, 0x0554U, 0x0555U
};

static uint32_t
sixel_interframe_stbn_pmj_interleave6_common(uint32_t x, uint32_t y)
{
    /*
     * Expand 6-bit coordinates into even/odd lanes and combine them
     * into a 12-bit Morton code. This keeps PMJ deterministic while
     * avoiding per-bit loops and repeated bit spreading in hot loops.
     */
    return (uint32_t)sixel_interframe_stbn_pmj_part1by1_lut6_common[x & 63U]
        | ((uint32_t)sixel_interframe_stbn_pmj_part1by1_lut6_common[y & 63U]
           << 1);
}

static uint32_t
sixel_interframe_stbn_pmj_channel_u32_common(int channel, int depth)
{
    int wrapped;

    wrapped = 0;
    if (depth <= 0) {
        return 0U;
    }
    if (channel >= 0 && channel < depth) {
        return (uint32_t)channel;
    }

    wrapped = channel % depth;
    if (wrapped < 0) {
        wrapped += depth;
    }

    return (uint32_t)wrapped;
}

static void
sixel_interframe_stbn_source_pmj_build_channel_cache_common(
    uint32_t sequence_index,
    uint32_t channel_u32,
    uint32_t depth_u32,
    sixel_interframe_stbn_pmj_channel_cache_common_t *channel_cache)
{
    uint32_t seed;
    uint32_t phase_key;
    uint32_t coord_key;

    seed = 0U;
    phase_key = 0U;
    coord_key = 0U;

    if (channel_cache == NULL) {
        return;
    }

    seed = sequence_index * 0x9e3779b9U;
    seed ^= (channel_u32 + 1U) * 0x85ebca6bU;
    seed ^= (depth_u32 + 1U) * 0xc2b2ae35U;
    seed = sixel_interframe_stbn_pmj_mix_u32_common(seed);

    phase_key = seed ^ 0x9e3779b9U;
    coord_key = seed ^ 0x243f6a88U;

    channel_cache->seed = seed;
    channel_cache->coord_key_x = coord_key ^ 0xa511e9b3U;
    channel_cache->coord_key_y = coord_key ^ 0x63d83595U;
    channel_cache->rank_key = seed ^ 0xb7e15162U;
    channel_cache->offset_x = sixel_interframe_stbn_pmj_permute6_common(
        sequence_index + 0x68bc21ebU,
        phase_key);
    channel_cache->offset_y = sixel_interframe_stbn_pmj_permute6_common(
        sequence_index ^ 0x02e5be93U,
        phase_key >> 7);
}

static uint16_t
sixel_interframe_stbn_pmj_sample_u16_cached_internal(
    sixel_interframe_stbn_pmj_channel_cache_common_t const *channel_cache,
    int x,
    int y)
{
    uint32_t scrambled_x;
    uint32_t scrambled_y;
    uint32_t rank;
    uint32_t jitter_seed;
    uint32_t jitter_u6;
    int sample_x;
    int sample_y;
    uint16_t sample_u16;

    scrambled_x = 0U;
    scrambled_y = 0U;
    rank = 0U;
    jitter_seed = 0U;
    jitter_u6 = 0U;
    sample_x = 0;
    sample_y = 0;
    sample_u16 = 0U;

    if (channel_cache == NULL) {
        return 0U;
    }

    /*
     * Wrap to the 64x64 tile without branches. Unsigned conversion is
     * modulo 2^N, so masking low 6 bits is equivalent to modulo 64.
     */
    sample_x = (int)((uint32_t)(x + (int)channel_cache->offset_x) & 63U);
    sample_y = (int)((uint32_t)(y + (int)channel_cache->offset_y) & 63U);

    scrambled_x = sixel_interframe_stbn_pmj_permute6_common(
        (uint32_t)sample_x,
        channel_cache->coord_key_x);
    scrambled_y = sixel_interframe_stbn_pmj_permute6_common(
        (uint32_t)sample_y,
        channel_cache->coord_key_y);
    rank = sixel_interframe_stbn_pmj_interleave6_common(scrambled_x,
                                                       scrambled_y);
    rank = sixel_interframe_stbn_pmj_permute12_common(rank,
                                                    channel_cache->rank_key);

    jitter_seed = channel_cache->seed;
    jitter_seed ^= (uint32_t)sample_x * 0x6a09e667U;
    jitter_seed ^= (uint32_t)sample_y * 0xbb67ae85U;
    jitter_u6 = sixel_interframe_stbn_pmj_mix_u32_common(jitter_seed) & 63U;
    rank = (rank + jitter_u6) & 4095U;

    /*
     * Convert 12-bit rank to rounded 16-bit sample without division.
     * This matches `(rank * 65535 + 2047) / 4095` for all rank values.
     */
    sample_u16 = (uint16_t)((rank * 65551U + 2055U) >> 12);

    return sample_u16;
}

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_common(uint32_t sequence_index,
                                                 int x,
                                                 int y,
                                                 int channel,
                                                 int depth)
{
    uint32_t channel_u32;
    uint32_t depth_u32;
    sixel_interframe_stbn_pmj_channel_cache_common_t channel_cache;
    uint16_t sample_u16;

    channel_u32 = 0U;
    depth_u32 = 0U;
    channel_cache.seed = 0U;
    channel_cache.coord_key_x = 0U;
    channel_cache.coord_key_y = 0U;
    channel_cache.rank_key = 0U;
    channel_cache.offset_x = 0U;
    channel_cache.offset_y = 0U;
    sample_u16 = 0U;

    channel_u32 = sixel_interframe_stbn_pmj_channel_u32_common(channel, depth);
    if (depth > 0) {
        depth_u32 = (uint32_t)depth;
    }
    sixel_interframe_stbn_source_pmj_build_channel_cache_common(sequence_index,
                                                              channel_u32,
                                                              depth_u32,
                                                              &channel_cache);
    sample_u16 = sixel_interframe_stbn_pmj_sample_u16_cached_internal(
        &channel_cache,
        x,
        y);

    return sample_u16;
}

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_cached_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth)
{
    uint32_t sequence_index;
    uint32_t channel_u32;
    sixel_interframe_stbn_pmj_channel_cache_common_t const *channel_cache;

    sequence_index = 0U;
    channel_u32 = 0U;
    channel_cache = NULL;

    if (stbn_state != NULL) {
        sequence_index = stbn_state->sequence_index;
    }

    if (stbn_state == NULL
            || stbn_state->pmj_cache_valid == 0
            || depth <= 0
            || depth > SIXEL_MAX_CHANNELS
            || depth != stbn_state->pmj_cache_depth) {
        return sixel_interframe_stbn_source_pmj_sample_u16_common(
            sequence_index,
            x,
            y,
            channel,
            depth);
    }

    channel_u32 = sixel_interframe_stbn_pmj_channel_u32_common(channel, depth);
    if (channel_u32 >= (uint32_t)stbn_state->pmj_cache_depth) {
        return sixel_interframe_stbn_source_pmj_sample_u16_common(
            sequence_index,
            x,
            y,
            channel,
            depth);
    }

    channel_cache = &stbn_state->pmj_channel_cache[(int)channel_u32];
    return sixel_interframe_stbn_pmj_sample_u16_cached_internal(
        channel_cache,
        x,
        y);
}

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_tiled_common(
    sixel_interframe_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth)
{
    uint32_t sequence_index;
    uint32_t channel_u32;
    int tile_x;
    int tile_y;
    size_t tile_index;

    sequence_index = 0U;
    channel_u32 = 0U;
    tile_x = 0;
    tile_y = 0;
    tile_index = 0U;

    if (stbn_state != NULL) {
        sequence_index = stbn_state->sequence_index;
    }

    if (stbn_state == NULL
            || stbn_state->pmj_tile_enabled == 0
            || stbn_state->pmj_tile_valid == 0
            || depth <= 0
            || depth > SIXEL_MAX_CHANNELS
            || depth != stbn_state->pmj_tile_depth
            || stbn_state->pmj_tile_sequence_index != sequence_index) {
        return sixel_interframe_stbn_source_pmj_sample_u16_cached_common(
            stbn_state,
            x,
            y,
            channel,
            depth);
    }

    channel_u32 = sixel_interframe_stbn_pmj_channel_u32_common(channel, depth);
    if (channel_u32 >= (uint32_t)stbn_state->pmj_tile_depth) {
        return sixel_interframe_stbn_source_pmj_sample_u16_cached_common(
            stbn_state,
            x,
            y,
            channel,
            depth);
    }

    tile_x = (int)((uint32_t)x & (SIXEL_INTERFRAME_PMJ_TILE_SIDE - 1U));
    tile_y = (int)((uint32_t)y & (SIXEL_INTERFRAME_PMJ_TILE_SIDE - 1U));
    tile_index = (size_t)tile_y * (size_t)SIXEL_INTERFRAME_PMJ_TILE_SIDE
        + (size_t)tile_x;

    return stbn_state->pmj_tile_u16[(int)channel_u32][tile_index];
}

uint16_t
sixel_interframe_stbn_source_pmj_sample_u16_row_cached_common(
    sixel_interframe_stbn_state_common_t *stbn_state,
    int x,
    int y,
    int channel,
    int depth)
{
    uint32_t sequence_index;
    uint32_t channel_u32;
    int tile_x;
    int tile_y;
    int cache_x;
    int cache_channel;
    uint16_t row_sample;
    sixel_interframe_stbn_pmj_channel_cache_common_t const *channel_cache;

    sequence_index = 0U;
    channel_u32 = 0U;
    tile_x = 0;
    tile_y = 0;
    cache_x = 0;
    cache_channel = 0;
    row_sample = 0U;
    channel_cache = NULL;

    if (stbn_state == NULL) {
        return sixel_interframe_stbn_source_pmj_sample_u16_common(
            0U,
            x,
            y,
            channel,
            depth);
    }

    sequence_index = stbn_state->sequence_index;
    if (stbn_state->pmj_cache_valid == 0
            || depth <= 0
            || depth > SIXEL_MAX_CHANNELS
            || depth != stbn_state->pmj_cache_depth
            || stbn_state->pmj_cache_sequence_index != sequence_index) {
        return sixel_interframe_stbn_source_pmj_sample_u16_cached_common(
            stbn_state,
            x,
            y,
            channel,
            depth);
    }

    channel_u32 = sixel_interframe_stbn_pmj_channel_u32_common(channel, depth);
    if (channel_u32 >= (uint32_t)depth) {
        return sixel_interframe_stbn_source_pmj_sample_u16_cached_common(
            stbn_state,
            x,
            y,
            channel,
            depth);
    }

    tile_x = (int)((uint32_t)x & (SIXEL_INTERFRAME_PMJ_TILE_SIDE - 1U));
    tile_y = (int)((uint32_t)y & (SIXEL_INTERFRAME_PMJ_TILE_SIDE - 1U));
    if (stbn_state->pmj_row_cache_valid == 0
            || stbn_state->pmj_row_cache_depth != depth
            || stbn_state->pmj_row_cache_sequence_index != sequence_index
            || stbn_state->pmj_row_cache_y != tile_y) {
        for (cache_channel = 0; cache_channel < depth; ++cache_channel) {
            channel_cache = &stbn_state->pmj_channel_cache[cache_channel];
            for (cache_x = 0; cache_x < SIXEL_INTERFRAME_PMJ_TILE_SIDE;
                    ++cache_x) {
                row_sample = sixel_interframe_stbn_pmj_sample_u16_cached_internal(
                    channel_cache,
                    cache_x,
                    tile_y);
                stbn_state->pmj_row_u16[cache_channel][cache_x] = row_sample;
            }
        }
        stbn_state->pmj_row_cache_depth = depth;
        stbn_state->pmj_row_cache_sequence_index = sequence_index;
        stbn_state->pmj_row_cache_y = tile_y;
        stbn_state->pmj_row_cache_valid = 1;
    }

    return stbn_state->pmj_row_u16[(int)channel_u32][tile_x];
}

SIXELSTATUS
sixel_interframe_stbn_source_pmj_prepare_state_common(
    sixel_dither_t const *dither,
    sixel_interframe_stbn_state_common_t *stbn_state,
    int can_update)
{
    SIXELSTATUS status;
    int depth;
    int width;
    int height;
    int channel;
    int tile_x;
    int tile_y;
    int need_cache_rebuild;
    uint32_t sequence_index;
    uint32_t depth_u32;
    uint64_t frame_pixels;
    size_t tile_index;
    sixel_interframe_stbn_pmj_channel_cache_common_t *channel_cache;

    status = SIXEL_OK;
    depth = 0;
    width = 0;
    height = 0;
    channel = 0;
    tile_x = 0;
    tile_y = 0;
    need_cache_rebuild = 0;
    sequence_index = 0U;
    depth_u32 = 0U;
    frame_pixels = 0U;
    tile_index = 0U;
    channel_cache = NULL;

    status = sixel_interframe_stbn_prepare_state_default_common(
        dither,
        stbn_state,
        can_update);
    if (status != SIXEL_OK || stbn_state == NULL) {
        return status;
    }

    /*
     * Keep cache initialization conservative. Missing or inconsistent
     * metadata must fall back to the stateless PMJ sampler path.
     */
    if (dither == NULL) {
        stbn_state->pmj_cache_valid = 0;
        stbn_state->pmj_cache_depth = 0;
        stbn_state->pmj_cache_sequence_index = 0U;
        stbn_state->pmj_row_cache_valid = 0;
        stbn_state->pmj_row_cache_depth = 0;
        stbn_state->pmj_row_cache_sequence_index = 0U;
        stbn_state->pmj_row_cache_y = 0;
        stbn_state->pmj_tile_valid = 0;
        stbn_state->pmj_tile_depth = 0;
        stbn_state->pmj_tile_sequence_index = 0U;
        stbn_state->pmj_tile_enabled = 0;
        return status;
    }
    depth = dither->interframe_state.depth;
    if (depth <= 0 || depth > SIXEL_MAX_CHANNELS) {
        stbn_state->pmj_cache_valid = 0;
        stbn_state->pmj_cache_depth = 0;
        stbn_state->pmj_cache_sequence_index = 0U;
        stbn_state->pmj_row_cache_valid = 0;
        stbn_state->pmj_row_cache_depth = 0;
        stbn_state->pmj_row_cache_sequence_index = 0U;
        stbn_state->pmj_row_cache_y = 0;
        stbn_state->pmj_tile_valid = 0;
        stbn_state->pmj_tile_depth = 0;
        stbn_state->pmj_tile_sequence_index = 0U;
        stbn_state->pmj_tile_enabled = 0;
        return status;
    }

    sequence_index = stbn_state->sequence_index;
    need_cache_rebuild = 1;
    if (stbn_state->pmj_cache_valid != 0
            && stbn_state->pmj_cache_depth == depth
            && stbn_state->pmj_cache_sequence_index == sequence_index) {
        need_cache_rebuild = 0;
    }
    if (need_cache_rebuild != 0) {
        depth_u32 = (uint32_t)depth;
        for (channel = 0; channel < depth; ++channel) {
            channel_cache = &stbn_state->pmj_channel_cache[channel];
            sixel_interframe_stbn_source_pmj_build_channel_cache_common(
                sequence_index,
                (uint32_t)channel,
                depth_u32,
                channel_cache);
        }

        stbn_state->pmj_cache_depth = depth;
        stbn_state->pmj_cache_sequence_index = sequence_index;
        stbn_state->pmj_cache_valid = 1;
        stbn_state->pmj_row_cache_valid = 0;
        stbn_state->pmj_row_cache_depth = 0;
        stbn_state->pmj_row_cache_sequence_index = 0U;
        stbn_state->pmj_row_cache_y = 0;
    }

    width = dither->interframe_state.width;
    height = dither->interframe_state.height;
    if (width > 0 && height > 0) {
        frame_pixels = (uint64_t)(uint32_t)width * (uint64_t)(uint32_t)height;
    }
    if (frame_pixels < (uint64_t)SIXEL_INTERFRAME_PMJ_TILE_ENABLE_MIN_PIXELS) {
        stbn_state->pmj_tile_enabled = 0;
        stbn_state->pmj_tile_valid = 0;
        stbn_state->pmj_tile_depth = 0;
        stbn_state->pmj_tile_sequence_index = 0U;
        return status;
    }

    stbn_state->pmj_tile_enabled = 1;
    if (need_cache_rebuild == 0
            && stbn_state->pmj_tile_valid != 0
            && stbn_state->pmj_tile_depth == depth
            && stbn_state->pmj_tile_sequence_index == sequence_index) {
        return status;
    }

    for (channel = 0; channel < depth; ++channel) {
        channel_cache = &stbn_state->pmj_channel_cache[channel];
        for (tile_y = 0; tile_y < SIXEL_INTERFRAME_PMJ_TILE_SIDE; ++tile_y) {
            for (tile_x = 0; tile_x < SIXEL_INTERFRAME_PMJ_TILE_SIDE; ++tile_x) {
                tile_index = (size_t)tile_y
                    * (size_t)SIXEL_INTERFRAME_PMJ_TILE_SIDE + (size_t)tile_x;
                stbn_state->pmj_tile_u16[channel][tile_index] =
                    sixel_interframe_stbn_pmj_sample_u16_cached_internal(
                        channel_cache,
                        tile_x,
                        tile_y);
            }
        }
    }

    stbn_state->pmj_tile_depth = depth;
    stbn_state->pmj_tile_sequence_index = sequence_index;
    stbn_state->pmj_tile_valid = 1;
    return status;
}

sixel_interframe_stbn_source_backend_common_t const
sixel_interframe_stbn_source_pmj_backend_common = {
    SIXEL_INTERFRAME_STBN_SOURCE_PMJ,
    sixel_interframe_stbn_source_pmj_sample_u16_common,
    sixel_interframe_stbn_source_pmj_prepare_state_common
};

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
