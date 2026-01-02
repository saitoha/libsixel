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
#if defined(__AVX2__)
#include <immintrin.h>
#endif
#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif

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
    int color_simd_end;
    int channel;
    int palette_offset;
#if defined(__AVX2__)
    __m256 sample_l_ps;
    __m256 sample_r_ps;
    __m256 sample_g_ps;
    __m256 complexion_ps;
    __m256 palette_l_ps;
    __m256 palette_r_ps;
    __m256 palette_g_ps;
    __m256 diff_l_ps;
    __m256 diff_r_ps;
    __m256 diff_g_ps;
    __m256 error_ps;
    __m256 best_error_ps;
    __m256 cmp_ps;
    __m128 error_lo_ps;
    __m128 error_hi_ps;
    int cmp_mask;
#endif
#if defined(__wasm_simd128__)
    v128_t sample_l_ps;
    v128_t sample_r_ps;
    v128_t sample_g_ps;
    v128_t complexion_ps;
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
#if defined(__AVX2__)
    /*
     * AVX2 fast path for depth==3 without L_r remap.  Eight palette entries
     * are compared at once, keeping the scalar tail to handle any remainder
     * or non-RGB layouts.
     */
    if (depth == 3 && use_l_r_distance == 0) {
        sample_l_ps = _mm256_set1_ps(pixel[0]);
        sample_r_ps = _mm256_set1_ps(pixel[1]);
        sample_g_ps = _mm256_set1_ps(pixel[2]);
        complexion_ps = _mm256_set1_ps((float)complexion);
        color_simd_end = reqcolor & ~7;
        for (color = 0; color < color_simd_end; color += 8) {
            palette_l_ps = _mm256_set_ps(palette[(color + 7) * 3],
                                         palette[(color + 6) * 3],
                                         palette[(color + 5) * 3],
                                         palette[(color + 4) * 3],
                                         palette[(color + 3) * 3],
                                         palette[(color + 2) * 3],
                                         palette[(color + 1) * 3],
                                         palette[color * 3]);
            palette_r_ps = _mm256_set_ps(palette[(color + 7) * 3 + 1],
                                         palette[(color + 6) * 3 + 1],
                                         palette[(color + 5) * 3 + 1],
                                         palette[(color + 4) * 3 + 1],
                                         palette[(color + 3) * 3 + 1],
                                         palette[(color + 2) * 3 + 1],
                                         palette[(color + 1) * 3 + 1],
                                         palette[color * 3 + 1]);
            palette_g_ps = _mm256_set_ps(palette[(color + 7) * 3 + 2],
                                         palette[(color + 6) * 3 + 2],
                                         palette[(color + 5) * 3 + 2],
                                         palette[(color + 4) * 3 + 2],
                                         palette[(color + 3) * 3 + 2],
                                         palette[(color + 2) * 3 + 2],
                                         palette[(color + 1) * 3 + 2],
                                         palette[color * 3 + 2]);
            diff_l_ps = _mm256_sub_ps(sample_l_ps, palette_l_ps);
            error_ps = _mm256_mul_ps(diff_l_ps, diff_l_ps);
            error_ps = _mm256_mul_ps(error_ps, complexion_ps);
            diff_r_ps = _mm256_sub_ps(sample_r_ps, palette_r_ps);
            error_ps = _mm256_fmadd_ps(diff_r_ps, diff_r_ps, error_ps);
            diff_g_ps = _mm256_sub_ps(sample_g_ps, palette_g_ps);
            error_ps = _mm256_fmadd_ps(diff_g_ps, diff_g_ps, error_ps);
            best_error_ps = _mm256_set1_ps((float)best_error);
            cmp_ps = _mm256_cmp_ps(error_ps, best_error_ps,
                                   _CMP_LT_OQ);
            cmp_mask = _mm256_movemask_ps(cmp_ps);
            if (cmp_mask != 0) {
                /*
                 * Update best_index directly from the active lanes to avoid
                 * spilling the vector to a temporary buffer.
                 */
                error_lo_ps = _mm256_castps256_ps128(error_ps);
                if ((cmp_mask & 0x01) != 0
                        && _mm_cvtss_f32(error_lo_ps) < best_error) {
                    best_error = (double)_mm_cvtss_f32(error_lo_ps);
                    best_index = color;
                }
                if ((cmp_mask & 0x02) != 0
                        && _mm_cvtss_f32(_mm_shuffle_ps(error_lo_ps,
                                                        error_lo_ps,
                                                        _MM_SHUFFLE(1,
                                                                    1,
                                                                    1,
                                                                    1)))
                            < best_error) {
                    best_error = (double)_mm_cvtss_f32(
                            _mm_shuffle_ps(error_lo_ps,
                                           error_lo_ps,
                                           _MM_SHUFFLE(1, 1, 1, 1)));
                    best_index = color + 1;
                }
                if ((cmp_mask & 0x04) != 0
                        && _mm_cvtss_f32(_mm_shuffle_ps(error_lo_ps,
                                                        error_lo_ps,
                                                        _MM_SHUFFLE(2,
                                                                    2,
                                                                    2,
                                                                    2)))
                            < best_error) {
                    best_error = (double)_mm_cvtss_f32(
                            _mm_shuffle_ps(error_lo_ps,
                                           error_lo_ps,
                                           _MM_SHUFFLE(2, 2, 2, 2)));
                    best_index = color + 2;
                }
                if ((cmp_mask & 0x08) != 0
                        && _mm_cvtss_f32(_mm_shuffle_ps(error_lo_ps,
                                                        error_lo_ps,
                                                        _MM_SHUFFLE(3,
                                                                    3,
                                                                    3,
                                                                    3)))
                            < best_error) {
                    best_error = (double)_mm_cvtss_f32(
                            _mm_shuffle_ps(error_lo_ps,
                                           error_lo_ps,
                                           _MM_SHUFFLE(3, 3, 3, 3)));
                    best_index = color + 3;
                }
                error_hi_ps = _mm256_extractf128_ps(error_ps, 1);
                if ((cmp_mask & 0x10) != 0
                        && _mm_cvtss_f32(error_hi_ps) < best_error) {
                    best_error = (double)_mm_cvtss_f32(error_hi_ps);
                    best_index = color + 4;
                }
                if ((cmp_mask & 0x20) != 0
                        && _mm_cvtss_f32(_mm_shuffle_ps(error_hi_ps,
                                                        error_hi_ps,
                                                        _MM_SHUFFLE(1,
                                                                    1,
                                                                    1,
                                                                    1)))
                            < best_error) {
                    best_error = (double)_mm_cvtss_f32(
                            _mm_shuffle_ps(error_hi_ps,
                                           error_hi_ps,
                                           _MM_SHUFFLE(1, 1, 1, 1)));
                    best_index = color + 5;
                }
                if ((cmp_mask & 0x40) != 0
                        && _mm_cvtss_f32(_mm_shuffle_ps(error_hi_ps,
                                                        error_hi_ps,
                                                        _MM_SHUFFLE(2,
                                                                    2,
                                                                    2,
                                                                    2)))
                            < best_error) {
                    best_error = (double)_mm_cvtss_f32(
                            _mm_shuffle_ps(error_hi_ps,
                                           error_hi_ps,
                                           _MM_SHUFFLE(2, 2, 2, 2)));
                    best_index = color + 6;
                }
                if ((cmp_mask & 0x80) != 0
                        && _mm_cvtss_f32(_mm_shuffle_ps(error_hi_ps,
                                                        error_hi_ps,
                                                        _MM_SHUFFLE(3,
                                                                    3,
                                                                    3,
                                                                    3)))
                            < best_error) {
                    best_error = (double)_mm_cvtss_f32(
                            _mm_shuffle_ps(error_hi_ps,
                                           error_hi_ps,
                                           _MM_SHUFFLE(3, 3, 3, 3)));
                    best_index = color + 7;
                }
            }
        }
    }
#endif
#if defined(__wasm_simd128__)
    /*
     * WASM SIMD fast path for depth==3 without L_r remap.  Palette entries
     * are processed in groups of four to balance gather cost and register
     * pressure; any remainder is handled by the scalar loop below.
     */
    if (color_simd_end == 0 && depth == 3 && use_l_r_distance == 0) {
        sample_l_ps = wasm_f32x4_splat(pixel[0]);
        sample_r_ps = wasm_f32x4_splat(pixel[1]);
        sample_g_ps = wasm_f32x4_splat(pixel[2]);
        complexion_ps = wasm_f32x4_splat((float)complexion);
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
            error_ps = wasm_f32x4_mul(error_ps, complexion_ps);
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
