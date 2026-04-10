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
#include "dither-temporal-stbn-source-mask.h"

uint16_t
sixel_temporal_stbn_source_mask_sample_u16_common(uint32_t sequence_index,
                                                  int x,
                                                  int y,
                                                  int channel,
                                                  int depth)
{
    /*
     * Keep mask source output identical to hash while dedicated STBN mask
     * tables are under integration.
     */
    return sixel_temporal_stbn_sample_hash_u16_common(sequence_index,
                                                      x,
                                                      y,
                                                      channel,
                                                      depth);
}

SIXELSTATUS
sixel_temporal_stbn_source_mask_prepare_state_common(
    sixel_dither_t const *dither,
    sixel_temporal_stbn_state_common_t *stbn_state,
    int can_update)
{
    /*
     * Mask source currently follows the default frame-index behavior.
     * Keeping a dedicated prepare hook here localizes future table lifecycle
     * changes to this module.
     */
    return sixel_temporal_stbn_prepare_state_default_common(
        dither,
        stbn_state,
        can_update);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
