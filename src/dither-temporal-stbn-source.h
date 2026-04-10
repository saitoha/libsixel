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
 * Shared STBN temporal source selection and sampling hooks.
 */
#ifndef LIBSIXEL_DITHER_TEMPORAL_STBN_SOURCE_H
#define LIBSIXEL_DITHER_TEMPORAL_STBN_SOURCE_H

#include <stdint.h>
#include "dither.h"

typedef uint16_t (*sixel_temporal_stbn_sample_u16_fn)(
    uint32_t sequence_index,
    int x,
    int y,
    int channel,
    int depth);

typedef struct sixel_temporal_stbn_state_common {
    uint32_t sequence_index;
    uint8_t sample_source_id;
    sixel_temporal_stbn_sample_u16_fn sample_u16;
} sixel_temporal_stbn_state_common_t;

typedef SIXELSTATUS (*sixel_temporal_stbn_prepare_state_common_fn)(
    sixel_dither_t const *dither,
    sixel_temporal_stbn_state_common_t *stbn_state,
    int can_update);

typedef struct sixel_temporal_stbn_source_backend_common {
    uint8_t source_id;
    sixel_temporal_stbn_sample_u16_fn sample_u16;
    sixel_temporal_stbn_prepare_state_common_fn prepare_state;
} sixel_temporal_stbn_source_backend_common_t;

uint16_t
sixel_temporal_stbn_sample_hash_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth);

uint16_t
sixel_temporal_stbn_sample_mask_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth);

SIXELSTATUS
sixel_temporal_stbn_prepare_state_default_common(
    sixel_dither_t const *dither,
    sixel_temporal_stbn_state_common_t *stbn_state,
    int can_update);

sixel_temporal_stbn_source_backend_common_t const *
sixel_temporal_stbn_source_backend_from_id_common(uint8_t source_id);

sixel_temporal_stbn_source_backend_common_t const *
sixel_temporal_stbn_source_backend_from_token_common(int strategy_token);

sixel_temporal_stbn_sample_u16_fn
sixel_temporal_stbn_sample_fn_from_source_id_common(uint8_t source_id);

uint16_t
sixel_temporal_stbn_sample_u16_state_common(
    sixel_temporal_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth);

#endif /* LIBSIXEL_DITHER_TEMPORAL_STBN_SOURCE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
