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
#include "dither-temporal-stbn-source-pmj.h"

/*
 * Keep PMJ deterministic and thread-stable. This source builds a progressive
 * 64x64 stratum from sequence/channel/depth, then applies reversible scrambles
 * over tile coordinates so spatial structure stays balanced while sequence
 * changes reduce frame-to-frame correlation.
 */
static uint32_t
sixel_temporal_stbn_pmj_mix_u32_common(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;

    return value;
}

static uint32_t
sixel_temporal_stbn_pmj_permute6_common(uint32_t value, uint32_t key)
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
sixel_temporal_stbn_pmj_permute12_common(uint32_t value, uint32_t key)
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

static uint32_t
sixel_temporal_stbn_pmj_part1by1_6_common(uint32_t value)
{
    value &= 63U;
    value = (value | (value << 4)) & 0x0f0fU;
    value = (value | (value << 2)) & 0x3333U;
    value = (value | (value << 1)) & 0x5555U;

    return value;
}

static uint32_t
sixel_temporal_stbn_pmj_interleave6_common(uint32_t x, uint32_t y)
{
    uint32_t code;

    /*
     * Expand 6-bit coordinates into even/odd lanes and combine them
     * into a 12-bit Morton code. This keeps PMJ deterministic while
     * avoiding per-bit loops on the hot sampling path.
     */
    code = sixel_temporal_stbn_pmj_part1by1_6_common(x);
    code |= sixel_temporal_stbn_pmj_part1by1_6_common(y) << 1;
    return code;
}

static uint32_t
sixel_temporal_stbn_pmj_channel_u32_common(int channel, int depth)
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

uint16_t
sixel_temporal_stbn_source_pmj_sample_u16_common(uint32_t sequence_index,
                                                 int x,
                                                 int y,
                                                 int channel,
                                                 int depth)
{
    uint32_t channel_u32;
    uint32_t depth_u32;
    uint32_t seed;
    uint32_t phase_key;
    uint32_t coord_key;
    uint32_t offset_x;
    uint32_t offset_y;
    uint32_t scrambled_x;
    uint32_t scrambled_y;
    uint32_t rank;
    uint32_t jitter_seed;
    uint32_t jitter_u6;
    int sample_x;
    int sample_y;
    uint16_t sample_u16;

    channel_u32 = 0U;
    depth_u32 = 0U;
    seed = 0U;
    phase_key = 0U;
    coord_key = 0U;
    offset_x = 0U;
    offset_y = 0U;
    scrambled_x = 0U;
    scrambled_y = 0U;
    rank = 0U;
    jitter_seed = 0U;
    jitter_u6 = 0U;
    sample_x = 0;
    sample_y = 0;
    sample_u16 = 0U;

    channel_u32 = sixel_temporal_stbn_pmj_channel_u32_common(channel, depth);
    if (depth > 0) {
        depth_u32 = (uint32_t)depth;
    }

    seed = sequence_index * 0x9e3779b9U;
    seed ^= (channel_u32 + 1U) * 0x85ebca6bU;
    seed ^= (depth_u32 + 1U) * 0xc2b2ae35U;
    seed = sixel_temporal_stbn_pmj_mix_u32_common(seed);

    phase_key = seed ^ 0x9e3779b9U;
    coord_key = seed ^ 0x243f6a88U;

    offset_x = sixel_temporal_stbn_pmj_permute6_common(
        sequence_index + 0x68bc21ebU,
        phase_key);
    offset_y = sixel_temporal_stbn_pmj_permute6_common(
        sequence_index ^ 0x02e5be93U,
        phase_key >> 7);

    /*
     * Wrap to the 64x64 tile without branches. Unsigned conversion is
     * modulo 2^N, so masking low 6 bits is equivalent to modulo 64.
     */
    sample_x = (int)((uint32_t)(x + (int)offset_x) & 63U);
    sample_y = (int)((uint32_t)(y + (int)offset_y) & 63U);

    scrambled_x = sixel_temporal_stbn_pmj_permute6_common(
        (uint32_t)sample_x,
        coord_key ^ 0xa511e9b3U);
    scrambled_y = sixel_temporal_stbn_pmj_permute6_common(
        (uint32_t)sample_y,
        coord_key ^ 0x63d83595U);
    rank = sixel_temporal_stbn_pmj_interleave6_common(scrambled_x,
                                                       scrambled_y);
    rank = sixel_temporal_stbn_pmj_permute12_common(
        rank,
        seed ^ 0xb7e15162U);

    jitter_seed = seed;
    jitter_seed ^= (uint32_t)sample_x * 0x6a09e667U;
    jitter_seed ^= (uint32_t)sample_y * 0xbb67ae85U;
    jitter_u6 = sixel_temporal_stbn_pmj_mix_u32_common(jitter_seed) & 63U;
    rank = (rank + jitter_u6) & 4095U;

    /*
     * Convert 12-bit rank to rounded 16-bit sample without division.
     * This matches `(rank * 65535 + 2047) / 4095` for all rank values.
     */
    sample_u16 = (uint16_t)((rank * 65551U + 2055U) >> 12);

    return sample_u16;
}

SIXELSTATUS
sixel_temporal_stbn_source_pmj_prepare_state_common(
    sixel_dither_t const *dither,
    sixel_temporal_stbn_state_common_t *stbn_state,
    int can_update)
{
    return sixel_temporal_stbn_prepare_state_default_common(
        dither,
        stbn_state,
        can_update);
}

sixel_temporal_stbn_source_backend_common_t const
sixel_temporal_stbn_source_pmj_backend_common = {
    SIXEL_TEMPORAL_STBN_SOURCE_PMJ,
    sixel_temporal_stbn_source_pmj_sample_u16_common,
    sixel_temporal_stbn_source_pmj_prepare_state_common
};

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
