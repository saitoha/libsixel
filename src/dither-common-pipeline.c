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

#include "dither-common-pipeline.h"
#include "timeline-logger.h"

static void
sixel_dither_pipeline_apply_keycolor_row(sixel_dither_t *dither, int row_index)
{
    size_t row_start;
    size_t row_end;
    size_t cursor;
    size_t width;
    int keycolor;

    if (dither == NULL || row_index < 0) {
        return;
    }

    if (dither->pipeline_transparent_mask == NULL
            || dither->pipeline_index_buffer == NULL) {
        return;
    }

    keycolor = dither->pipeline_transparent_keycolor;
    if (keycolor < 0 || keycolor >= SIXEL_PALETTE_MAX) {
        return;
    }

    width = (size_t)dither->pipeline_image_width;
    if (width == 0U) {
        return;
    }

    row_start = (size_t)row_index * width;
    if (row_start >= dither->pipeline_transparent_mask_size) {
        return;
    }
    row_end = row_start + width;
    if (row_end > dither->pipeline_transparent_mask_size) {
        row_end = dither->pipeline_transparent_mask_size;
    }

    for (cursor = row_start; cursor < row_end; ++cursor) {
        if (dither->pipeline_transparent_mask[cursor] != 0U) {
            dither->pipeline_index_buffer[cursor] = (sixel_index_t)keycolor;
        }
    }
}


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
    sixel_dither_pipeline_apply_keycolor_row(dither, row_index);
    if (dither->pipeline_logger != NULL) {
        int band_height;
        int band_index;

        band_height = dither->pipeline_band_height;
        band_index = -1;
        if (band_height > 0 && row_index >= 0) {
            band_index = row_index / band_height;
        }
        sixel_timeline_logger_logf(dither->pipeline_logger,
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
