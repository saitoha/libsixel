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
#include "dither-interframe-stbn-source-hash.h"

static uint32_t
sixel_interframe_stbn_hash_u32_common(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

uint16_t
sixel_interframe_stbn_source_hash_sample_u16_common(uint32_t sequence_index,
                                                  int x,
                                                  int y,
                                                  int channel,
                                                  int depth)
{
    uint32_t key;
    uint32_t ch;

    key = 0U;
    ch = 0U;

    if (depth > 0) {
        ch = (uint32_t)(channel % depth);
    }

    key = sequence_index * 0x9e3779b9U;
    key ^= (uint32_t)x * 0x6a09e667U;
    key ^= (uint32_t)y * 0xbb67ae85U;
    key ^= ch * 0x3c6ef372U;
    key = sixel_interframe_stbn_hash_u32_common(key);

    return (uint16_t)(key >> 16);
}

SIXELSTATUS
sixel_interframe_stbn_source_hash_prepare_state_common(
    sixel_dither_t const *dither,
    sixel_interframe_stbn_state_common_t *stbn_state,
    int can_update)
{
    return sixel_interframe_stbn_prepare_state_default_common(
        dither,
        stbn_state,
        can_update);
}

sixel_interframe_stbn_source_backend_common_t const
sixel_interframe_stbn_source_hash_backend_common = {
    SIXEL_INTERFRAME_STBN_SOURCE_HASH,
    sixel_interframe_stbn_source_hash_sample_u16_common,
    sixel_interframe_stbn_source_hash_prepare_state_common
};

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
