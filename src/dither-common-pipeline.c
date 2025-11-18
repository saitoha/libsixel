/*
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

#include "config.h"

#include <float.h>

#include "dither-common-pipeline.h"

/*
 * Notify the pipeline controller when a scanline completes PaletteApply.
 * The encoder installs a callback so the producer can release each band as
 * soon as all six rows have been finalized.
 */
void
sixel_dither_pipeline_row_notify(sixel_dither_t *dither, int row_index)
{
    if (dither == NULL) {
        return;
    }
    if (dither->pipeline_row_callback == NULL) {
        return;
    }
    dither->pipeline_row_callback(dither->pipeline_row_priv, row_index);
}

/*
 * Compute the nearest palette entry using float32 samples.  The function
 * mirrors lookup_normal() but operates on float buffers so positional and
 * variable-coefficient dithers can benefit from palettes published with
 * RGBFLOAT32 precision.
 */
int
sixel_dither_lookup_palette_float32(float const *pixel,
                                    int depth,
                                    float const *palette,
                                    int reqcolor,
                                    int complexion)
{
    double best_error;
    double error;
    double delta;
    int best_index;
    int color;
    int channel;
    int palette_offset;

    if (pixel == NULL || palette == NULL) {
        return 0;
    }
    if (depth <= 0 || reqcolor <= 0) {
        return 0;
    }

    best_index = 0;
    best_error = DBL_MAX;
    for (color = 0; color < reqcolor; ++color) {
        palette_offset = color * depth;
        delta = (double)pixel[0] - (double)palette[palette_offset + 0];
        error = delta * delta * (double)complexion;
        for (channel = 1; channel < depth; ++channel) {
            delta = (double)pixel[channel]
                  - (double)palette[palette_offset + channel];
            error += delta * delta;
        }
        if (error < best_error) {
            best_error = error;
            best_index = color;
        }
    }

    return best_index;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
