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
#include "dither-interframe-method.h"

static int
sixel_interframe_is_supported_spatial_diffuse(int method_for_diffuse)
{
    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_NONE:
    case SIXEL_DIFFUSE_FS:
    case SIXEL_DIFFUSE_ATKINSON:
    case SIXEL_DIFFUSE_JAJUNI:
    case SIXEL_DIFFUSE_STUCKI:
    case SIXEL_DIFFUSE_BURKES:
    case SIXEL_DIFFUSE_SIERRA1:
    case SIXEL_DIFFUSE_SIERRA2:
    case SIXEL_DIFFUSE_SIERRA3:
        return 1;
    default:
        break;
    }

    return 0;
}

static int
sixel_interframe_spatial_diffuse_from_env_named(char const *envvar,
                                                 int fallback)
{
    char const *value;
    int resolved;

    value = NULL;
    resolved = SIXEL_INTERFRAME_SPATIAL_DIFFUSE_UNSET;

    if (envvar == NULL) {
        return fallback;
    }

    value = sixel_compat_getenv(envvar);
    resolved = sixel_interframe_spatial_diffuse_from_string(value);
    if (!sixel_interframe_is_supported_spatial_diffuse(resolved)) {
        resolved = fallback;
    }
    return resolved;
}

int
sixel_interframe_strategy_token_from_string(char const *value)
{
    if (value == NULL) {
        return SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    }
    if (strcmp(value, "diffusion") == 0) {
        return SIXEL_INTERFRAME_STRATEGY_TOKEN_DIFFUSION;
    }
    if (strcmp(value, "stbn") == 0) {
        return SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH;
    }
    if (strcmp(value, "stbn-hash") == 0) {
        return SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH;
    }
    if (strcmp(value, "stbn-mask") == 0) {
        return SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK;
    }
    if (strcmp(value, "pmj") == 0) {
        return SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ;
    }

    return SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
}

int
sixel_interframe_strategy_method_from_token(int strategy_token)
{
    switch (strategy_token) {
    case SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH:
    case SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK:
    case SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ:
        return SIXEL_INTERFRAME_METHOD_STBN;
    case SIXEL_INTERFRAME_STRATEGY_TOKEN_DIFFUSION:
        return SIXEL_INTERFRAME_METHOD_DIFFUSION;
    default:
        break;
    }

    return SIXEL_INTERFRAME_METHOD_NONE;
}

int
sixel_interframe_spatial_diffuse_from_string(char const *value)
{
    if (value == NULL) {
        return SIXEL_INTERFRAME_SPATIAL_DIFFUSE_UNSET;
    }
    if (strcmp(value, "auto") == 0) {
        return SIXEL_DIFFUSE_FS;
    }
    if (strcmp(value, "none") == 0) {
        return SIXEL_DIFFUSE_NONE;
    }
    if (strcmp(value, "fs") == 0) {
        return SIXEL_DIFFUSE_FS;
    }
    if (strcmp(value, "atkinson") == 0) {
        return SIXEL_DIFFUSE_ATKINSON;
    }
    if (strcmp(value, "jajuni") == 0) {
        return SIXEL_DIFFUSE_JAJUNI;
    }
    if (strcmp(value, "stucki") == 0) {
        return SIXEL_DIFFUSE_STUCKI;
    }
    if (strcmp(value, "burkes") == 0) {
        return SIXEL_DIFFUSE_BURKES;
    }
    if (strcmp(value, "sierra1") == 0) {
        return SIXEL_DIFFUSE_SIERRA1;
    }
    if (strcmp(value, "sierra2") == 0) {
        return SIXEL_DIFFUSE_SIERRA2;
    }
    if (strcmp(value, "sierra3") == 0) {
        return SIXEL_DIFFUSE_SIERRA3;
    }

    return SIXEL_INTERFRAME_SPATIAL_DIFFUSE_UNSET;
}

int
sixel_interframe_spatial_diffuse_from_env_common(void)
{
    return sixel_interframe_spatial_diffuse_from_env_named(
        SIXEL_DITHER_INTERFRAME_DIFFUSION_ENVVAR,
        SIXEL_DIFFUSE_FS);
}

int
sixel_interframe_spatial_diffuse_from_dither_or_env_common(
    sixel_dither_t const *dither)
{
    int resolved;
    int strategy_token;
    int strategy_method;

    if (dither != NULL && dither->interframe_spatial_diffuse_override != 0) {
        resolved = dither->interframe_spatial_diffuse;
        if (sixel_interframe_is_supported_spatial_diffuse(resolved)) {
            return resolved;
        }
    }

    strategy_token = sixel_interframe_strategy_token_from_dither_or_env_common(
        dither);
    strategy_method = sixel_interframe_strategy_method_from_token(
        strategy_token);
    if (strategy_method == SIXEL_INTERFRAME_METHOD_STBN) {
        return sixel_interframe_spatial_diffuse_from_env_named(
            SIXEL_DITHER_STBN_DIFFUSION_ENVVAR,
            SIXEL_DIFFUSE_NONE);
    }

    return sixel_interframe_spatial_diffuse_from_env_named(
        SIXEL_DITHER_INTERFRAME_DIFFUSION_ENVVAR,
        SIXEL_DIFFUSE_FS);
}

int
sixel_interframe_strategy_token_from_env_common(void)
{
    char const *value;

    value = sixel_compat_getenv(SIXEL_DITHER_STBN_SOURCE_ENVVAR);
    return sixel_interframe_strategy_token_from_string(value);
}

int
sixel_interframe_strategy_token_from_dither_or_env_common(
    sixel_dither_t const *dither)
{
    if (dither != NULL && dither->interframe_strategy_override != 0) {
        return dither->interframe_strategy_token;
    }

    return sixel_interframe_strategy_token_from_env_common();
}

uint8_t
sixel_interframe_stbn_source_id_from_token(int strategy_token)
{
    if (strategy_token == SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK) {
        return SIXEL_INTERFRAME_STBN_SOURCE_MASK;
    }
    if (strategy_token == SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ) {
        return SIXEL_INTERFRAME_STBN_SOURCE_PMJ;
    }

    return SIXEL_INTERFRAME_STBN_SOURCE_HASH;
}

int
sixel_interframe_method_from_diffuse_and_token(int method_for_diffuse,
                                             int strategy_token)
{
    int method;

    method = SIXEL_INTERFRAME_METHOD_NONE;
    if (method_for_diffuse != SIXEL_DIFFUSE_INTERFRAME) {
        return method;
    }

    method = sixel_interframe_strategy_method_from_token(strategy_token);
    if (method == SIXEL_INTERFRAME_METHOD_STBN) {
        return method;
    }

    return SIXEL_INTERFRAME_METHOD_DIFFUSION;
}

void
sixel_interframe_release_shared_frame(sixel_dither_t *dither)
{
    sixel_allocator_t *allocator;

    if (dither == NULL) {
        return;
    }

    allocator = dither->allocator;
    if (dither->interframe_state.error_frame != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, dither->interframe_state.error_frame);
    }
    if (dither->interframe_state.method_private != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, dither->interframe_state.method_private);
    }

    dither->interframe_state.error_frame = NULL;
    dither->interframe_state.error_frame_size = 0U;
    dither->interframe_state.width = 0;
    dither->interframe_state.height = 0;
    dither->interframe_state.depth = 0;
    dither->interframe_state.method_id = SIXEL_INTERFRAME_METHOD_NONE;
    dither->interframe_state.method_private = NULL;
    dither->interframe_state.method_private_size = 0U;
}

SIXELSTATUS
sixel_interframe_prepare_shared_frame(sixel_dither_t *dither,
                                    int width,
                                    int height,
                                    int depth,
                                    int can_update,
                                    int owner_method,
                                    int *enabled,
                                    int32_t **frame)
{
    SIXELSTATUS status;
    size_t interframe_len;
    size_t interframe_bytes;
    int needs_reset;
    int32_t *new_frame;

    status = SIXEL_OK;
    interframe_len = 0U;
    interframe_bytes = 0U;
    needs_reset = 0;
    new_frame = NULL;

    if (dither == NULL || dither->allocator == NULL
            || enabled == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (owner_method != SIXEL_INTERFRAME_METHOD_DIFFUSION
            && owner_method != SIXEL_INTERFRAME_METHOD_STBN) {
        return SIXEL_BAD_ARGUMENT;
    }

    *frame = NULL;
    if (*enabled == 0) {
        return status;
    }

    interframe_len = (size_t)width * (size_t)height * (size_t)depth;
    if (interframe_len == 0U) {
        *enabled = 0;
        return status;
    }

    if (dither->interframe_state.error_frame != NULL) {
        if (dither->interframe_state.method_id != owner_method) {
            needs_reset = 1;
        } else if (dither->interframe_state.width != width
                || dither->interframe_state.height != height
                || dither->interframe_state.depth != depth) {
            needs_reset = 1;
        }
    }

    if (needs_reset) {
        if (can_update) {
            sixel_dither_note_interframe_reset_reason(
                dither,
                SIXEL_DITHER_INTERFRAME_RESET_REASON_SIZE_CHANGE);
            sixel_interframe_release_shared_frame(dither);
        } else {
            *enabled = 0;
            return status;
        }
    }

    if (dither->interframe_state.error_frame == NULL) {
        if (can_update == 0) {
            *enabled = 0;
            return status;
        }

        if (interframe_len > SIZE_MAX / sizeof(int32_t)) {
            return SIXEL_BAD_ALLOCATION;
        }
        interframe_bytes = interframe_len * sizeof(int32_t);
        new_frame = (int32_t *)sixel_allocator_malloc(dither->allocator,
                                                      interframe_bytes);
        if (new_frame == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }

        memset(new_frame, 0x00, interframe_bytes);
        dither->interframe_state.error_frame = new_frame;
        dither->interframe_state.error_frame_size = interframe_bytes;
        dither->interframe_state.width = width;
        dither->interframe_state.height = height;
        dither->interframe_state.depth = depth;
    }

    dither->interframe_state.method_id = owner_method;
    *frame = (int32_t *)dither->interframe_state.error_frame;
    return status;
}

SIXELSTATUS
sixel_interframe_prepare_method_private(sixel_dither_t *dither,
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

    if (dither->interframe_state.method_private != NULL
            && (dither->interframe_state.method_id != owner_method
                || dither->interframe_state.method_private_size != state_size)) {
        sixel_allocator_free(allocator, dither->interframe_state.method_private);
        dither->interframe_state.method_private = NULL;
        dither->interframe_state.method_private_size = 0U;
    }

    if (dither->interframe_state.method_private == NULL) {
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
        dither->interframe_state.method_private = new_state;
        dither->interframe_state.method_private_size = state_size;
    }

    dither->interframe_state.method_id = owner_method;
    *state = dither->interframe_state.method_private;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_interframe_prepare_stbn_state_common(sixel_dither_t *dither,
                                         int can_update,
                                         int strategy_token,
                                         size_t state_size,
                                         void **state)
{
    SIXELSTATUS status;
    sixel_interframe_stbn_state_common_t *typed_state;
    sixel_interframe_stbn_source_backend_common_t const *backend;

    status = SIXEL_OK;
    typed_state = NULL;
    backend = NULL;

    if (state == NULL || state_size < sizeof(*typed_state)) {
        return SIXEL_BAD_ARGUMENT;
    }

    *state = NULL;
    status = sixel_interframe_prepare_method_private(
        dither,
        SIXEL_INTERFRAME_METHOD_STBN,
        can_update,
        state_size,
        state);
    if (status != SIXEL_OK || *state == NULL) {
        return status;
    }

    typed_state = (sixel_interframe_stbn_state_common_t *)
        sixel_interframe_get_method_private(
            dither,
            SIXEL_INTERFRAME_METHOD_STBN,
            state_size);
    if (typed_state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    backend = sixel_interframe_stbn_source_backend_from_token_common(
        strategy_token);
    if (backend != NULL) {
        typed_state->sample_source_id = backend->source_id;
        typed_state->sample_u16 = backend->sample_u16;
    }
    if (typed_state->sample_u16 == NULL) {
        typed_state->sample_source_id = SIXEL_INTERFRAME_STBN_SOURCE_HASH;
        typed_state->sample_u16 = sixel_interframe_stbn_sample_hash_u16_common;
    }

    if (backend != NULL && backend->prepare_state != NULL) {
        status = backend->prepare_state(dither, typed_state, can_update);
    } else {
        status = sixel_interframe_stbn_prepare_state_default_common(
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
