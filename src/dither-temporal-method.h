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

typedef SIXELSTATUS (*sixel_temporal_prepare_frame_fn)(
    sixel_dither_t *dither,
    int width,
    int height,
    int depth,
    int can_update,
    int *enabled,
    int32_t **frame);

typedef void (*sixel_temporal_load_pixel_fn)(
    unsigned char const *data,
    size_t base,
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

#endif /* LIBSIXEL_DITHER_TEMPORAL_METHOD_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
