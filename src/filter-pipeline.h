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

#ifndef LIBSIXEL_FILTER_PIPELINE_H
#define LIBSIXEL_FILTER_PIPELINE_H

#include "filter.h"

/* The plan may mark this edge eligible; allocation and binding checks at
 * execution can still retain the ordinary sequential filter path. */
SIXEL_INTERNAL_API int
sixel_filter_pipeline_eligible(int source, int target, size_t count,
    int threads);

/* Run two independent pointwise filters on a bounded tile queue. SIXEL_FALSE
 * means not admitted: the caller must run the ordinary filters. Runtime
 * failures drain the queue and replay sequentially for the same diagnostic
 * and partial-publication semantics as an ordinary plan. No loop fusion. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_pipeline_run(sixel_filter_t *first, sixel_filter_t *second,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger);

#endif
