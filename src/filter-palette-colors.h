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

#ifndef LIBSIXEL_FILTER_PALETTE_COLORS_H
#define LIBSIXEL_FILTER_PALETTE_COLORS_H

#include "filter.h"

typedef struct sixel_filter_palette_colors_config {
    /* Zero performs an identity transform. The planner owns this decision. */
    int convert;
    int target_pixelformat;
    int target_colorspace;
    int collect_statistics;
    /* Only raw double point input needs this storage-domain annotation. */
    int byte_coordinates;
    /* A caller that will mutate the output also needs an isolated payload. */
    int copy_payload;
} sixel_filter_palette_colors_config_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_palette_colors_init(sixel_filter_t *filter,
    sixel_filter_palette_colors_config_t const *config);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
