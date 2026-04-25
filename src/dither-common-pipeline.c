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
#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif

#include "dither-common-pipeline.h"
#include "logger.h"

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
                                    int reqcolor)
{
    double best_error;
    double error;
    double sample_l;
    double palette_l;
    double delta;
    int best_index;
    int color;
    int color_simd_end;
    int channel;
    int palette_offset;
#if defined(__wasm_simd128__)
    v128_t sample_l_ps;
    v128_t sample_r_ps;
    v128_t sample_g_ps;
    v128_t palette_l_ps;
    v128_t palette_r_ps;
    v128_t palette_g_ps;
    v128_t diff_l_ps;
    v128_t diff_r_ps;
    v128_t diff_g_ps;
    v128_t error_ps;
    float error_lane0;
    float error_lane1;
    float error_lane2;
    float error_lane3;
#endif

    if (pixel == NULL || palette == NULL) {
        return 0;
    }
    if (depth <= 0 || reqcolor <= 0) {
        return 0;
    }

    best_index = 0;
    best_error = DBL_MAX;
    color_simd_end = 0;
    sample_l = (double)pixel[0];
#if defined(__wasm_simd128__)
    /*
     * WASM SIMD fast path for depth==3.  Palette entries are processed in
     * groups of four to balance gather cost and register pressure; any
     * remainder is handled by the scalar loop below.
     */
    if (depth == 3) {
        sample_l_ps = wasm_f32x4_splat(pixel[0]);
        sample_r_ps = wasm_f32x4_splat(pixel[1]);
        sample_g_ps = wasm_f32x4_splat(pixel[2]);
        color_simd_end = reqcolor & ~3;
        for (color = 0; color < color_simd_end; color += 4) {
            palette_l_ps = wasm_f32x4_make(palette[color * 3],
                                           palette[(color + 1) * 3],
                                           palette[(color + 2) * 3],
                                           palette[(color + 3) * 3]);
            palette_r_ps = wasm_f32x4_make(palette[color * 3 + 1],
                                           palette[(color + 1) * 3 + 1],
                                           palette[(color + 2) * 3 + 1],
                                           palette[(color + 3) * 3 + 1]);
            palette_g_ps = wasm_f32x4_make(palette[color * 3 + 2],
                                           palette[(color + 1) * 3 + 2],
                                           palette[(color + 2) * 3 + 2],
                                           palette[(color + 3) * 3 + 2]);
            diff_l_ps = wasm_f32x4_sub(sample_l_ps, palette_l_ps);
            error_ps = wasm_f32x4_mul(diff_l_ps, diff_l_ps);
            diff_r_ps = wasm_f32x4_sub(sample_r_ps, palette_r_ps);
            error_ps = wasm_f32x4_add(error_ps,
                    wasm_f32x4_mul(diff_r_ps, diff_r_ps));
            diff_g_ps = wasm_f32x4_sub(sample_g_ps, palette_g_ps);
            error_ps = wasm_f32x4_add(error_ps,
                    wasm_f32x4_mul(diff_g_ps, diff_g_ps));
            error_lane0 = wasm_f32x4_extract_lane(error_ps, 0);
            error_lane1 = wasm_f32x4_extract_lane(error_ps, 1);
            error_lane2 = wasm_f32x4_extract_lane(error_ps, 2);
            error_lane3 = wasm_f32x4_extract_lane(error_ps, 3);
            if (error_lane0 < best_error) {
                best_error = (double)error_lane0;
                best_index = color;
            }
            if (error_lane1 < best_error) {
                best_error = (double)error_lane1;
                best_index = color + 1;
            }
            if (error_lane2 < best_error) {
                best_error = (double)error_lane2;
                best_index = color + 2;
            }
            if (error_lane3 < best_error) {
                best_error = (double)error_lane3;
                best_index = color + 3;
            }
        }
    }
#endif
    /*
     * color_simd_end carries the handoff point from the SIMD fast paths.
     * Any remaining palette entries (including non-RGB layouts) are handled
     * by the scalar loop to keep behavior identical to the baseline path.
     */
    for (color = color_simd_end; color < reqcolor; ++color) {
        palette_offset = color * depth;
        palette_l = (double)palette[palette_offset];
        delta = sample_l - palette_l;
        error = delta * delta;
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
