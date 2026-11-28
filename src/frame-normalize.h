/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FRAME_NORMALIZE_H
#define LIBSIXEL_FRAME_NORMALIZE_H

#include <6cells.h>
#include "sixel_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Private encoder normalization, with no concrete frame storage exposed. */
/* Float triplets use RGBFLOAT32 layout for all supported coordinates. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_convert_pixels(sixel_frame_t *frame, int layout,
                           int target_colorspace);

/* Resolve an internal output/work format to an explicit conversion target. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_convert_workformat(sixel_frame_t *frame, int format);

/* Optional per-source observation for the packed-frame regression tests.
 * Install before handing the frame to an encoder and keep the counters
 * alive until all operations have joined. Read the counts after joining. */
typedef struct sixel_ycbcr422_counts {
    sixel_atomic_u32_t packed_copies;
    sixel_atomic_u32_t rgb_conversions;
    sixel_atomic_u32_t rgb_pixels;
    sixel_atomic_u32_t rgb_copies;
} sixel_ycbcr422_counts_t;

SIXEL_INTERNAL_API void
sixel_frame_observe_ycbcr422(sixel_frame_t *frame,
                            sixel_ycbcr422_counts_t *counts);

/* Produce a private RGB frame directly from packed rows. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_decode_ycbcr422_copy(sixel_frame_t const *frame,
                                sixel_allocator_t *allocator,
                                sixel_frame_t **frame_out);

/* A view owns its metadata and retains its immutable profile RGB source.
 * Consumers must replace storage before writing to the pixel plane. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_view_profile_rgb(sixel_frame_t *frame,
    sixel_allocator_t *allocator, sixel_frame_t **result);

/* Decode the original raster coordinates selected by a sampling grid.
 * A full decode can reuse the same source's previously decoded grid. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_sample_ycbcr422(sixel_frame_t const *frame,
    sixel_allocator_t *allocator, int step, sixel_frame_t const *grid,
    int grid_step, sixel_frame_t **frame_out);

/* Match storage layout, decoding policy and RGB definition. */
SIXEL_INTERNAL_API int
sixel_frame_ycbcr422_matches(sixel_frame_t const *frame,
                            sixel_frame_t const *source);

/* Normalize packed gamma RGB/gray alpha for the native float encoder.
 * Metadata still decides whether alpha is interpreted. Fractional blending
 * retains the legacy gamma-domain/background-gray convention, without byte
 * rounding. NULL background means black, as on the legacy packed path.
 * With no fractional blend and REQUIRE_FLOAT false, publish exact RGB888.
 * Failure leaves pixels, masks and metadata unchanged. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_unpack_alpha(sixel_frame_t *frame,
                          unsigned char const *background,
                          int require_float);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FRAME_NORMALIZE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
