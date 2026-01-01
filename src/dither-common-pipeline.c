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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>

#include "dither-common-pipeline.h"
#include "logger.h"

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
    if (dither->pipeline_logger != NULL) {
        int band_height;
        int band_index;

        band_height = dither->pipeline_band_height;
        band_index = -1;
        if (band_height > 0 && row_index >= 0) {
            band_index = row_index / band_height;
        }
        sixel_logger_logf(dither->pipeline_logger,
                          "producer",
                          "dither",
                          "row_ready",
                          band_index);
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
                                    int complexion,
                                    int use_l_r_distance)
{
    double best_error;
    double error;
    double sample_l;
    double palette_l;
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
    sample_l = (double)pixel[0];
    if (use_l_r_distance != 0) {
        /*
         * Optional OKLab L_r remap keeps shadow distances stable without
         * mutating the stored sample buffers.
         */
        const double k1 = 0.206;
        const double k2 = 0.03;
        const double k3 = (1.0 + k1) / (1.0 + k2);

        sample_l = 0.5 * (((k3 * sample_l) / (sample_l + k1))
                          + (sample_l / ((k2 * sample_l) + 1.0)));
    }
    for (color = 0; color < reqcolor; ++color) {
        palette_offset = color * depth;
        palette_l = (double)palette[palette_offset];
        if (use_l_r_distance != 0) {
            const double k1 = 0.206;
            const double k2 = 0.03;
            const double k3 = (1.0 + k1) / (1.0 + k2);

            palette_l = 0.5 * (((k3 * palette_l) / (palette_l + k1))
                                + (palette_l / ((k2 * palette_l) + 1.0)));
        }
        delta = sample_l - palette_l;
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
