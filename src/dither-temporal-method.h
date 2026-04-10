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
 * Common temporal dithering method definitions shared by fixed pipelines.
 */
#ifndef LIBSIXEL_DITHER_TEMPORAL_METHOD_H
#define LIBSIXEL_DITHER_TEMPORAL_METHOD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "dither.h"

#define SIXEL_TEMPORAL_METHOD_NONE      0
#define SIXEL_TEMPORAL_METHOD_DIFFUSION 1
#define SIXEL_TEMPORAL_METHOD_STBN      2

#define SIXEL_TEMPORAL_STRATEGY_TOKEN_NONE      0
#define SIXEL_TEMPORAL_STRATEGY_TOKEN_DIFFUSION 1
#define SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_HASH 2
#define SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_MASK 3

#define SIXEL_TEMPORAL_STBN_SOURCE_HASH 0
#define SIXEL_TEMPORAL_STBN_SOURCE_MASK 1

#define SIXEL_TEMPORAL_VARERR_SCALE_SHIFT 12
#define SIXEL_TEMPORAL_VARERR_SCALE \
    (1 << SIXEL_TEMPORAL_VARERR_SCALE_SHIFT)
#define SIXEL_TEMPORAL_VARERR_ROUND \
    (1 << (SIXEL_TEMPORAL_VARERR_SCALE_SHIFT - 1))
#define SIXEL_TEMPORAL_VARERR_MAX_VALUE \
    (255 * SIXEL_TEMPORAL_VARERR_SCALE)

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

typedef SIXELSTATUS (*sixel_temporal_prepare_frame_fn)(
    sixel_dither_t *dither,
    int width,
    int height,
    int depth,
    int can_update,
    int *enabled,
    int32_t **frame);

typedef void (*sixel_temporal_load_pixel_fn)(
    sixel_dither_t *dither,
    unsigned char const *data,
    size_t base,
    int x,
    int y,
    int depth,
    int32_t const *frame,
    unsigned char *corrected,
    int32_t *accum_scaled);

typedef void (*sixel_temporal_clear_pixel_fn)(
    int32_t *frame,
    size_t base,
    int depth,
    int can_update);

typedef void (*sixel_temporal_store_error_fn)(
    int32_t *frame,
    size_t base,
    int channel,
    int offset,
    int can_update);

typedef struct sixel_temporal_method_ops {
    int method_id;
    sixel_temporal_prepare_frame_fn prepare_frame;
    sixel_temporal_load_pixel_fn load_pixel;
    sixel_temporal_clear_pixel_fn clear_pixel;
    sixel_temporal_store_error_fn store_error;
} sixel_temporal_method_ops_t;

static inline uint32_t
sixel_temporal_stbn_hash_u32_common(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static inline uint16_t
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

static inline uint16_t
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

static inline sixel_temporal_stbn_sample_u16_fn
sixel_temporal_stbn_sample_fn_from_source_id_common(uint8_t source_id)
{
    switch (source_id) {
    case SIXEL_TEMPORAL_STBN_SOURCE_MASK:
        return sixel_temporal_stbn_sample_mask_u16_common;
    case SIXEL_TEMPORAL_STBN_SOURCE_HASH:
    default:
        break;
    }

    return sixel_temporal_stbn_sample_hash_u16_common;
}

static inline uint32_t
sixel_temporal_stbn_sequence_index_common(
    sixel_temporal_stbn_state_common_t const *stbn_state)
{
    if (stbn_state == NULL) {
        return 0U;
    }

    return stbn_state->sequence_index;
}

static inline uint8_t
sixel_temporal_stbn_sample_source_id_common(
    sixel_temporal_stbn_state_common_t const *stbn_state)
{
    if (stbn_state == NULL) {
        return SIXEL_TEMPORAL_STBN_SOURCE_HASH;
    }

    return stbn_state->sample_source_id;
}

static inline uint16_t
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

    sequence_index = sixel_temporal_stbn_sequence_index_common(stbn_state);
    source_id = sixel_temporal_stbn_sample_source_id_common(stbn_state);
    sample_u16 = NULL;
    if (stbn_state != NULL) {
        sample_u16 = stbn_state->sample_u16;
    }
    if (sample_u16 == NULL) {
        sample_u16 = sixel_temporal_stbn_sample_fn_from_source_id_common(
            source_id);
    }
    if (sample_u16 == NULL) {
        sample_u16 = sixel_temporal_stbn_sample_hash_u16_common;
    }

    return sample_u16(sequence_index, x, y, channel, depth);
}

/*
 * Keep temporal strategy token parsing in one place so 8bit and float32
 * backends resolve overrides with identical fallback rules.
 */
static inline int
sixel_temporal_strategy_token_from_string(char const *value)
{
    if (value == NULL) {
        return SIXEL_TEMPORAL_STRATEGY_TOKEN_NONE;
    }
    if (strcmp(value, "diffusion") == 0) {
        return SIXEL_TEMPORAL_STRATEGY_TOKEN_DIFFUSION;
    }
    if (strcmp(value, "stbn") == 0) {
        return SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_HASH;
    }
    if (strcmp(value, "stbn-hash") == 0) {
        return SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_HASH;
    }
    if (strcmp(value, "stbn-mask") == 0) {
        return SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_MASK;
    }

    return SIXEL_TEMPORAL_STRATEGY_TOKEN_NONE;
}

static inline int
sixel_temporal_strategy_method_from_token(int strategy_token)
{
    switch (strategy_token) {
    case SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_HASH:
    case SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_MASK:
        return SIXEL_TEMPORAL_METHOD_STBN;
    case SIXEL_TEMPORAL_STRATEGY_TOKEN_DIFFUSION:
        return SIXEL_TEMPORAL_METHOD_DIFFUSION;
    default:
        break;
    }

    return SIXEL_TEMPORAL_METHOD_NONE;
}

static inline uint8_t
sixel_temporal_stbn_source_id_from_token(int strategy_token)
{
    if (strategy_token == SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_MASK) {
        return SIXEL_TEMPORAL_STBN_SOURCE_MASK;
    }

    return SIXEL_TEMPORAL_STBN_SOURCE_HASH;
}

static inline int
sixel_temporal_method_from_diffuse_and_token(int method_for_diffuse,
                                             int strategy_token)
{
    int method;

    method = SIXEL_TEMPORAL_METHOD_NONE;
    if (method_for_diffuse != SIXEL_DIFFUSE_TEMPORAL) {
        return method;
    }

    method = sixel_temporal_strategy_method_from_token(strategy_token);
    if (method == SIXEL_TEMPORAL_METHOD_STBN) {
        return method;
    }

    return SIXEL_TEMPORAL_METHOD_DIFFUSION;
}

/*
 * Shared temporal frame ownership helpers used by temporal strategies.
 * The owner method id protects the frame from cross-strategy reuse.
 */
static inline void
sixel_temporal_release_shared_frame(sixel_dither_t *dither)
{
    sixel_allocator_t *allocator;

    if (dither == NULL) {
        return;
    }

    allocator = dither->allocator;
    if (dither->temporal_state.error_frame != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, dither->temporal_state.error_frame);
    }
    if (dither->temporal_state.method_private != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, dither->temporal_state.method_private);
    }

    dither->temporal_state.error_frame = NULL;
    dither->temporal_state.error_frame_size = 0U;
    dither->temporal_state.width = 0;
    dither->temporal_state.height = 0;
    dither->temporal_state.depth = 0;
    dither->temporal_state.method_id = SIXEL_TEMPORAL_METHOD_NONE;
    dither->temporal_state.method_private = NULL;
    dither->temporal_state.method_private_size = 0U;
}

static inline SIXELSTATUS
sixel_temporal_prepare_shared_frame(sixel_dither_t *dither,
                                    int width,
                                    int height,
                                    int depth,
                                    int can_update,
                                    int owner_method,
                                    int *enabled,
                                    int32_t **frame)
{
    SIXELSTATUS status;
    size_t temporal_len;
    size_t temporal_bytes;
    int needs_reset;
    int32_t *new_frame;

    status = SIXEL_OK;
    temporal_len = 0U;
    temporal_bytes = 0U;
    needs_reset = 0;
    new_frame = NULL;

    if (dither == NULL || dither->allocator == NULL
            || enabled == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (owner_method != SIXEL_TEMPORAL_METHOD_DIFFUSION
            && owner_method != SIXEL_TEMPORAL_METHOD_STBN) {
        return SIXEL_BAD_ARGUMENT;
    }

    *frame = NULL;
    if (*enabled == 0) {
        return status;
    }

    temporal_len = (size_t)width * (size_t)height * (size_t)depth;
    if (temporal_len == 0U) {
        *enabled = 0;
        return status;
    }

    if (dither->temporal_state.error_frame != NULL) {
        if (dither->temporal_state.method_id != owner_method) {
            needs_reset = 1;
        } else if (dither->temporal_state.width != width
                || dither->temporal_state.height != height
                || dither->temporal_state.depth != depth) {
            needs_reset = 1;
        }
    }

    if (needs_reset) {
        if (can_update) {
            sixel_temporal_release_shared_frame(dither);
        } else {
            *enabled = 0;
            return status;
        }
    }

    if (dither->temporal_state.error_frame == NULL) {
        if (can_update == 0) {
            *enabled = 0;
            return status;
        }

        if (temporal_len > SIZE_MAX / sizeof(int32_t)) {
            return SIXEL_BAD_ALLOCATION;
        }
        temporal_bytes = temporal_len * sizeof(int32_t);
        new_frame = (int32_t *)sixel_allocator_malloc(dither->allocator,
                                                      temporal_bytes);
        if (new_frame == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }

        memset(new_frame, 0x00, temporal_bytes);
        dither->temporal_state.error_frame = new_frame;
        dither->temporal_state.error_frame_size = temporal_bytes;
        dither->temporal_state.width = width;
        dither->temporal_state.height = height;
        dither->temporal_state.depth = depth;
    }

    dither->temporal_state.method_id = owner_method;
    *frame = (int32_t *)dither->temporal_state.error_frame;
    return status;
}

/*
 * Reserve method-private temporal state for strategy-specific data such as
 * STBN sequence cursors. The state buffer is reused while method and size are
 * unchanged.
 */
static inline SIXELSTATUS
sixel_temporal_prepare_method_private(sixel_dither_t *dither,
                                      int owner_method,
                                      int can_update,
                                      size_t state_size,
                                      void **state)
{
    void *new_state;
    sixel_allocator_t *allocator;

    new_state = NULL;
    allocator = NULL;

    if (dither == NULL || state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *state = NULL;
    if (state_size == 0U) {
        return SIXEL_OK;
    }

    allocator = dither->allocator;
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (dither->temporal_state.method_private != NULL
            && (dither->temporal_state.method_id != owner_method
                || dither->temporal_state.method_private_size != state_size)) {
        if (can_update == 0) {
            return SIXEL_OK;
        }
        sixel_allocator_free(allocator, dither->temporal_state.method_private);
        dither->temporal_state.method_private = NULL;
        dither->temporal_state.method_private_size = 0U;
    }

    if (dither->temporal_state.method_private == NULL) {
        if (can_update == 0) {
            return SIXEL_OK;
        }
        new_state = sixel_allocator_malloc(allocator, state_size);
        if (new_state == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        memset(new_state, 0x00, state_size);
        dither->temporal_state.method_private = new_state;
        dither->temporal_state.method_private_size = state_size;
    }

    dither->temporal_state.method_id = owner_method;
    *state = dither->temporal_state.method_private;
    return SIXEL_OK;
}

/*
 * Fetch strategy-specific temporal private state with ownership and size
 * validation. This avoids open-coded access checks at call sites.
 */
static inline void const *
sixel_temporal_get_method_private_const(sixel_dither_t const *dither,
                                        int owner_method,
                                        size_t state_size)
{
    if (dither == NULL || dither->temporal_state.method_private == NULL) {
        return NULL;
    }
    if (dither->temporal_state.method_id != owner_method) {
        return NULL;
    }
    if (dither->temporal_state.method_private_size < state_size) {
        return NULL;
    }

    return dither->temporal_state.method_private;
}

static inline void *
sixel_temporal_get_method_private(sixel_dither_t *dither,
                                  int owner_method,
                                  size_t state_size)
{
    return (void *)sixel_temporal_get_method_private_const(
        (sixel_dither_t const *)dither,
        owner_method,
        state_size);
}

/*
 * Prepare STBN private state with consistent source resolution and sequence
 * selection so fixed 8bit and float32 backends share the same behavior.
 */
static inline SIXELSTATUS
sixel_temporal_prepare_stbn_state_common(sixel_dither_t *dither,
                                         int can_update,
                                         int strategy_token,
                                         size_t state_size,
                                         void **state)
{
    SIXELSTATUS status;
    sixel_temporal_stbn_state_common_t *typed_state;
    uint8_t source_id;

    status = SIXEL_OK;
    typed_state = NULL;
    source_id = SIXEL_TEMPORAL_STBN_SOURCE_HASH;

    if (state == NULL || state_size < sizeof(*typed_state)) {
        return SIXEL_BAD_ARGUMENT;
    }

    *state = NULL;
    status = sixel_temporal_prepare_method_private(
        dither,
        SIXEL_TEMPORAL_METHOD_STBN,
        can_update,
        state_size,
        state);
    if (status != SIXEL_OK || *state == NULL || can_update == 0) {
        return status;
    }

    typed_state = (sixel_temporal_stbn_state_common_t *)
        sixel_temporal_get_method_private(
            dither,
            SIXEL_TEMPORAL_METHOD_STBN,
            state_size);
    if (typed_state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    source_id = sixel_temporal_stbn_source_id_from_token(strategy_token);
    typed_state->sample_source_id = source_id;
    typed_state->sample_u16 =
        sixel_temporal_stbn_sample_fn_from_source_id_common(source_id);
    if (typed_state->sample_u16 == NULL) {
        typed_state->sample_u16 = sixel_temporal_stbn_sample_hash_u16_common;
    }

    if (dither != NULL && dither->frame_context.valid) {
        typed_state->sequence_index =
            (uint32_t)dither->frame_context.frame_no;
    }

    *state = typed_state;
    return status;
}

#endif /* LIBSIXEL_DITHER_TEMPORAL_METHOD_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
