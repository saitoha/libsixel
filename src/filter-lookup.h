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

#ifndef LIBSIXEL_FILTER_LOOKUP_H
#define LIBSIXEL_FILTER_LOOKUP_H

#include <sixel.h>

#include "filter.h"
#include "lookup-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configuration for the lookup filter. The filter configures a LUT using the
 * provided palette and weighting rules. A pre-existing LUT can be reused when
 * `reuse_lut` is set.
 */
typedef struct sixel_filter_lookup_config {
    unsigned char const *palette;
    /* Optional float palette already in the dither working colorspace. */
    float const *palette_float;
    int depth;
    /* Bytes per float palette entry when palette_float is present. */
    int float_depth;
    int ncolors;
    int complexion;
    int method_for_largest;
    int lut_policy;
    int pixelformat;
    sixel_lut_t *reuse_lut;
} sixel_filter_lookup_config_t;

/*
 * Result bundle describing the configured LUT and whether the filter owns its
 * lifetime. Callers should unref the LUT only when `owned` is non-zero.
 */
typedef struct sixel_filter_lookup_result {
    sixel_lut_t *lut;
    int owned;
} sixel_filter_lookup_result_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_lookup_build(
    const sixel_filter_lookup_config_t *config,
    sixel_allocator_t *allocator,
    sixel_logger_t *logger,
    sixel_filter_lookup_result_t *result_out);

SIXELSTATUS
sixel_filter_lookup_init(sixel_filter_t *filter,
                         const sixel_filter_lookup_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FILTER_LOOKUP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
