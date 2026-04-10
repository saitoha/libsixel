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
 * Keep PMJ v1 deterministic and lightweight: sequence index chooses a
 * progressive 64x64 stratum, then we jitter it with a small hash so
 * animation frames decorrelate without introducing thread-dependent state.
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
sixel_temporal_stbn_pmj_channel_u32_common(int channel, int depth)
{
    int wrapped;

    wrapped = 0;
    if (depth <= 0) {
        return 0U;
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
    uint32_t strata;
    uint32_t strata_x;
    uint32_t strata_y;
    uint32_t offset_x;
    uint32_t offset_y;
    int sample_x;
    int sample_y;
    uint32_t tile_rank;
    uint32_t jitter_seed;
    uint32_t jitter_rank;
    uint16_t sample_u16;

    channel_u32 = 0U;
    depth_u32 = 0U;
    seed = 0U;
    strata = 0U;
    strata_x = 0U;
    strata_y = 0U;
    offset_x = 0U;
    offset_y = 0U;
    sample_x = 0;
    sample_y = 0;
    tile_rank = 0U;
    jitter_seed = 0U;
    jitter_rank = 0U;
    sample_u16 = 0U;

    channel_u32 = sixel_temporal_stbn_pmj_channel_u32_common(channel, depth);
    if (depth > 0) {
        depth_u32 = (uint32_t)depth;
    }

    seed = sequence_index * 0x9e3779b9U;
    seed ^= (channel_u32 + 1U) * 0x85ebca6bU;
    seed ^= (depth_u32 + 1U) * 0xc2b2ae35U;
    seed = sixel_temporal_stbn_pmj_mix_u32_common(seed);

    strata = sequence_index & 4095U;
    strata_x = strata & 63U;
    strata_y = (strata >> 6) & 63U;

    offset_x = (strata_x * 37U + (seed & 63U)) & 63U;
    offset_y = (strata_y * 53U + ((seed >> 8) & 63U)) & 63U;

    sample_x = sixel_temporal_stbn_wrap_tile_coord_common(
        x + (int)offset_x,
        64);
    sample_y = sixel_temporal_stbn_wrap_tile_coord_common(
        y + (int)offset_y,
        64);

    tile_rank = (uint32_t)sample_y * 64U + (uint32_t)sample_x;
    jitter_seed = seed
        ^ ((uint32_t)sample_x * 0x6a09e667U)
        ^ ((uint32_t)sample_y * 0xbb67ae85U);
    jitter_rank = sixel_temporal_stbn_pmj_mix_u32_common(jitter_seed) & 4095U;
    tile_rank = (tile_rank * 73U + jitter_rank) & 4095U;
    sample_u16 = (uint16_t)((tile_rank * 65535U + 2047U) / 4095U);

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
