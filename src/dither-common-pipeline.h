/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_DITHER_COMMON_PIPELINE_H
#define LIBSIXEL_DITHER_COMMON_PIPELINE_H

#include <math.h>

#include "dither.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve only the automatic amplitude. Numeric CLI/environment overrides
 * remain absolute amplitudes and are applied by the consuming policy.
 * Preserve the 256-color defaults and increase smoothly as paintable colors
 * become sparse. This inexpensive heuristic deliberately does not analyze
 * image content or claim perceptual normalization across working spaces. */
static inline float
sixel_dither_positional_auto_strength(int method, int ncolors)
{
    double base;
    double growth;

    if (ncolors <= 1) {
        return 0.0f;
    }
    if (ncolors > 256) {
        ncolors = 256;
    }
    switch (method) {
    case SIXEL_DIFFUSE_A_DITHER:
        base = 0.150;
        growth = 0.100;
        break;
    case SIXEL_DIFFUSE_X_DITHER:
        base = 0.100;
        growth = 0.050;
        break;
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        base = 0.055;
        growth = 0.080;
        break;
    default:
        return 0.0f;
    }
    return (float)(base + growth * (sqrt(256.0 / ncolors) - 1.0));
}

void
sixel_dither_pipeline_row_notify(sixel_dither_t *dither, int row_index);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_DITHER_COMMON_PIPELINE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
