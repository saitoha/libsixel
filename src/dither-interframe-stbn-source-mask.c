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
#include "dither-interframe-stbn-source-mask.h"
#include "dither-interframe-stbn-source-mask-table.h"

uint16_t
sixel_interframe_stbn_source_mask_sample_u16_common(uint32_t sequence_index,
                                                  int x,
                                                  int y,
                                                  int channel,
                                                  int depth)
{
    uint16_t sample;
    int has_table_sample;

    sample = 0U;
    has_table_sample = 0;

    has_table_sample = sixel_interframe_stbn_mask_table_try_sample_u16_common(
        sequence_index,
        x,
        y,
        channel,
        depth,
        &sample);
    if (has_table_sample != 0) {
        return sample;
    }

    /*
     * Keep hash fallback for safety if table lookup is unavailable.
     */
    return sixel_interframe_stbn_sample_hash_u16_common(sequence_index,
                                                      x,
                                                      y,
                                                      channel,
                                                      depth);
}

SIXELSTATUS
sixel_interframe_stbn_source_mask_prepare_state_common(
    sixel_dither_t const *dither,
    sixel_interframe_stbn_state_common_t *stbn_state,
    int can_update)
{
    int has_table;

    has_table = 0;
    has_table = sixel_interframe_stbn_mask_table_is_available_common();
    (void)has_table;

    /*
     * Mask source follows the shared frame-index rule. Keep a dedicated
     * prepare hook so future table lifecycle changes stay local.
     */
    return sixel_interframe_stbn_prepare_state_default_common(
        dither,
        stbn_state,
        can_update);
}

sixel_interframe_stbn_source_backend_common_t const
sixel_interframe_stbn_source_mask_backend_common = {
    SIXEL_INTERFRAME_STBN_SOURCE_MASK,
    sixel_interframe_stbn_source_mask_sample_u16_common,
    sixel_interframe_stbn_source_mask_prepare_state_common
};

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
