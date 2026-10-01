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

#ifdef __cplusplus
extern "C" {
#endif

/* Private encoder normalization, with no concrete frame storage exposed. */
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
