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

#ifndef LIBSIXEL_PALETTE_SAMPLE_H
#define LIBSIXEL_PALETTE_SAMPLE_H

#include "sample-statistics.h"
#include "palette-plan.h"
#include "timeline-logger.h"

/* Palette adapters request observations only after identifying their actual
 * retained population. The factory/run boundary owns all pixel inspection. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_prepare_sample_statistics(void const *data, size_t length,
    int pixelformat, int colorspace, int alpha_zero_is_transparent,
    sixel_sample_statistics_t **out, sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger);

SIXEL_INTERNAL_API sixel_sample_statistics_t *
sixel_palette_sample_statistics_for(sixel_palette_build_attempt_t *attempt,
    void const *data, size_t length, int pixelformat);

#endif


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
