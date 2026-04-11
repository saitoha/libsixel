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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include "bluenoise_64x64.h"
#include "dither-interframe-stbn-source.h"
#include "dither-interframe-stbn-source-mask-table.h"

static uint32_t
sixel_interframe_stbn_mask_table_mix_u32_common(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;

    return value;
}

static uint32_t
sixel_interframe_stbn_mask_table_channel_u32_common(int channel, int depth)
{
    int wrapped;
    uint32_t channel_u32;

    wrapped = 0;
    channel_u32 = 0U;
    if (depth <= 0) {
        return channel_u32;
    }

    wrapped = channel % depth;
    if (wrapped < 0) {
        wrapped += depth;
    }
    channel_u32 = (uint32_t)wrapped;

    return channel_u32;
}

int
sixel_interframe_stbn_mask_table_is_available_common(void)
{
    return 1;
}

int
sixel_interframe_stbn_mask_table_try_sample_u16_common(uint32_t sequence_index,
                                                      int x,
                                                      int y,
                                                      int channel,
                                                      int depth,
                                                      uint16_t *sample)
{
    uint32_t channel_u32;
    uint32_t depth_u32;
    uint32_t phase;
    uint32_t offset_x;
    uint32_t offset_y;
    int sample_x;
    int sample_y;
    size_t sample_index;
    uint8_t sample_u8;
    uint16_t sample_u16;

    channel_u32 = 0U;
    depth_u32 = 0U;
    phase = 0U;
    offset_x = 0U;
    offset_y = 0U;
    sample_x = 0;
    sample_y = 0;
    sample_index = 0U;
    sample_u8 = 0U;
    sample_u16 = 0U;

    channel_u32 = sixel_interframe_stbn_mask_table_channel_u32_common(
        channel,
        depth);
    if (depth > 0) {
        depth_u32 = (uint32_t)depth;
    }

    phase = sequence_index * 0x9e3779b9U;
    phase ^= (channel_u32 + 1U) * 0x85ebca6bU;
    phase ^= (depth_u32 + 1U) * 0xc2b2ae35U;
    phase = sixel_interframe_stbn_mask_table_mix_u32_common(phase);

    offset_x = phase & (uint32_t)(SIXEL_BN_W - 1);
    offset_y = (phase >> 8) & (uint32_t)(SIXEL_BN_H - 1);
    sample_x = sixel_interframe_stbn_wrap_tile_coord_common(
        x + (int)offset_x,
        SIXEL_BN_W);
    sample_y = sixel_interframe_stbn_wrap_tile_coord_common(
        y + (int)offset_y,
        SIXEL_BN_H);
    sample_index = (size_t)sample_y * (size_t)SIXEL_BN_W + (size_t)sample_x;
    sample_u8 = sixel_bn64[sample_index];
    sample_u16 = sixel_interframe_stbn_sample_u8_to_u16_common(sample_u8);

    if (sample != NULL) {
        *sample = sample_u16;
    }

    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
