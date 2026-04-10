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

#include <stddef.h>
#include <stdint.h>
#include "dither-temporal-method.h"

static uint32_t
sixel_temporal_stbn_hash_u32_common(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

uint16_t
sixel_temporal_stbn_sample_hash_u16_common(uint32_t sequence_index,
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
    key = sixel_temporal_stbn_hash_u32_common(key);

    return (uint16_t)(key >> 16);
}

uint16_t
sixel_temporal_stbn_sample_mask_u16_common(uint32_t sequence_index,
                                           int x,
                                           int y,
                                           int channel,
                                           int depth)
{
    /*
     * Placeholder mask source keeps output stable until dedicated STBN mask
     * tables are connected.
     */
    return sixel_temporal_stbn_sample_hash_u16_common(sequence_index,
                                                      x,
                                                      y,
                                                      channel,
                                                      depth);
}

SIXELSTATUS
sixel_temporal_stbn_prepare_state_default_common(
    sixel_dither_t const *dither,
    sixel_temporal_stbn_state_common_t *stbn_state,
    int can_update)
{
    (void)can_update;

    if (stbn_state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (dither != NULL && dither->frame_context.valid) {
        stbn_state->sequence_index = (uint32_t)dither->frame_context.frame_no;
    }

    return SIXEL_OK;
}

static sixel_temporal_stbn_source_backend_common_t const
sixel_temporal_stbn_source_backends_common[] = {
    /*
     * Index 0 is the fallback backend. Keep it as hash until dedicated
     * source tables are introduced and validated.
     */
    {
        SIXEL_TEMPORAL_STBN_SOURCE_HASH,
        sixel_temporal_stbn_sample_hash_u16_common,
        sixel_temporal_stbn_prepare_state_default_common
    },
    /*
     * Mask currently reuses hash samples as a placeholder. Table-driven
     * dispatch keeps future PMJ/STBN mask integration local to this list.
     */
    {
        SIXEL_TEMPORAL_STBN_SOURCE_MASK,
        sixel_temporal_stbn_sample_mask_u16_common,
        sixel_temporal_stbn_prepare_state_default_common
    }
};

sixel_temporal_stbn_source_backend_common_t const *
sixel_temporal_stbn_source_backend_from_id_common(uint8_t source_id)
{
    size_t i;
    size_t count;

    i = 0U;
    count = sizeof(sixel_temporal_stbn_source_backends_common)
        / sizeof(sixel_temporal_stbn_source_backends_common[0]);

    for (i = 0U; i < count; ++i) {
        if (sixel_temporal_stbn_source_backends_common[i].source_id
                == source_id) {
            return &sixel_temporal_stbn_source_backends_common[i];
        }
    }

    return &sixel_temporal_stbn_source_backends_common[0];
}

sixel_temporal_stbn_source_backend_common_t const *
sixel_temporal_stbn_source_backend_from_token_common(int strategy_token)
{
    uint8_t source_id;

    source_id = sixel_temporal_stbn_source_id_from_token(strategy_token);
    return sixel_temporal_stbn_source_backend_from_id_common(source_id);
}

sixel_temporal_stbn_sample_u16_fn
sixel_temporal_stbn_sample_fn_from_source_id_common(uint8_t source_id)
{
    sixel_temporal_stbn_source_backend_common_t const *backend;

    backend = sixel_temporal_stbn_source_backend_from_id_common(source_id);
    if (backend != NULL && backend->sample_u16 != NULL) {
        return backend->sample_u16;
    }

    return sixel_temporal_stbn_sample_hash_u16_common;
}

static uint32_t
sixel_temporal_stbn_sequence_index_common(
    sixel_temporal_stbn_state_common_t const *stbn_state)
{
    if (stbn_state == NULL) {
        return 0U;
    }

    return stbn_state->sequence_index;
}

static uint8_t
sixel_temporal_stbn_sample_source_id_common(
    sixel_temporal_stbn_state_common_t const *stbn_state)
{
    if (stbn_state == NULL) {
        return SIXEL_TEMPORAL_STBN_SOURCE_HASH;
    }

    return stbn_state->sample_source_id;
}

uint16_t
sixel_temporal_stbn_sample_u16_state_common(
    sixel_temporal_stbn_state_common_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth)
{
    uint32_t sequence_index;
    uint8_t source_id;
    sixel_temporal_stbn_sample_u16_fn sample_u16;
    sixel_temporal_stbn_source_backend_common_t const *backend;

    sequence_index = sixel_temporal_stbn_sequence_index_common(stbn_state);
    source_id = sixel_temporal_stbn_sample_source_id_common(stbn_state);
    sample_u16 = NULL;
    backend = NULL;
    if (stbn_state != NULL) {
        sample_u16 = stbn_state->sample_u16;
    }
    if (sample_u16 == NULL) {
        backend = sixel_temporal_stbn_source_backend_from_id_common(source_id);
        if (backend != NULL) {
            sample_u16 = backend->sample_u16;
        }
    }
    if (sample_u16 == NULL) {
        sample_u16 = sixel_temporal_stbn_sample_hash_u16_common;
    }

    return sample_u16(sequence_index, x, y, channel, depth);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
