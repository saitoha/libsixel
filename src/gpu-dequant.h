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

#ifndef LIBSIXEL_GPU_DEQUANT_H
#define LIBSIXEL_GPU_DEQUANT_H

#include <stddef.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIXEL_GPU_DEQUANT_AUTO_THRESHOLD_DEFAULT 262144U

/*
 * GPU fast4 dequantization request.
 *
 * The decoder-side accelerator starts after the SIXEL parser has already
 * produced direct RGBA.  Alpha carries the painted/unpainted distinction, so
 * the shader does not need the indexed paint mask.  The palette remains part
 * of the request because k_undither decides whether a neighbour is useful by
 * comparing the midpoint against the other palette colors.
 */
typedef struct sixel_gpu_dequant_request {
    int policy;
    size_t auto_threshold;
    unsigned char *dest;
    unsigned char const *rgba;
    size_t pixel_count;
    int width;
    int height;
    int pixelformat;
    unsigned char const *palette;
    size_t palette_size;
    int palette_depth;
    int ncolors;
    int similarity_bias;
} sixel_gpu_dequant_request_t;

/*
 * Resolve the AUTO cutoff independently of backend availability.  Keeping
 * this seam observable lets registry migrations preserve the historical
 * strtoul() contract before GPU dispatch policy is changed.
 */
SIXEL_INTERNAL_API size_t
sixel_gpu_dequant_auto_threshold(void);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_dequant_fast4_rgba(sixel_gpu_dequant_request_t const *request);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_GPU_DEQUANT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
