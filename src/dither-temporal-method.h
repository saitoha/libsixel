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

#endif /* LIBSIXEL_DITHER_TEMPORAL_METHOD_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
