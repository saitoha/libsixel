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
#include <string.h>
#include "compat_stub.h"
#include "dither-temporal-method.h"

int
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

int
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

int
sixel_temporal_strategy_token_from_env_common(void)
{
    char const *value;

    value = sixel_compat_getenv("SIXEL_TEMPORAL_STRATEGY");
    return sixel_temporal_strategy_token_from_string(value);
}

uint8_t
sixel_temporal_stbn_source_id_from_token(int strategy_token)
{
    if (strategy_token == SIXEL_TEMPORAL_STRATEGY_TOKEN_STBN_MASK) {
        return SIXEL_TEMPORAL_STBN_SOURCE_MASK;
    }

    return SIXEL_TEMPORAL_STBN_SOURCE_HASH;
}

int
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

void
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

SIXELSTATUS
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

SIXELSTATUS
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
    (void)can_update;

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
        sixel_allocator_free(allocator, dither->temporal_state.method_private);
        dither->temporal_state.method_private = NULL;
        dither->temporal_state.method_private_size = 0U;
    }

    if (dither->temporal_state.method_private == NULL) {
        /*
         * Even in capture-only passes (can_update == 0), STBN sampling still
         * needs a deterministic backend selection state. Allocating here keeps
         * mapfile capture output identical to regular encode output.
         */
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

SIXELSTATUS
sixel_temporal_prepare_stbn_state_common(sixel_dither_t *dither,
                                         int can_update,
                                         int strategy_token,
                                         size_t state_size,
                                         void **state)
{
    SIXELSTATUS status;
    sixel_temporal_stbn_state_common_t *typed_state;
    sixel_temporal_stbn_source_backend_common_t const *backend;

    status = SIXEL_OK;
    typed_state = NULL;
    backend = NULL;

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
    if (status != SIXEL_OK || *state == NULL) {
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

    backend = sixel_temporal_stbn_source_backend_from_token_common(
        strategy_token);
    if (backend != NULL) {
        typed_state->sample_source_id = backend->source_id;
        typed_state->sample_u16 = backend->sample_u16;
    }
    if (typed_state->sample_u16 == NULL) {
        typed_state->sample_source_id = SIXEL_TEMPORAL_STBN_SOURCE_HASH;
        typed_state->sample_u16 = sixel_temporal_stbn_sample_hash_u16_common;
    }

    if (backend != NULL && backend->prepare_state != NULL) {
        status = backend->prepare_state(dither, typed_state, can_update);
    } else {
        status = sixel_temporal_stbn_prepare_state_default_common(
            dither,
            typed_state,
            can_update);
    }
    if (status != SIXEL_OK) {
        return status;
    }

    *state = typed_state;
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
