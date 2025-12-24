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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#include <string.h>

#include "dither-positional-float32.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "lookup-common.h"

static void
sixel_dither_scanline_params_positional_float32(int serpentine,
                             int index,
                             int limit,
                             int *start,
                             int *end,
                             int *step,
                             int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}

static float
positional_mask_a_float32(int x, int y, int c)
{
    return ((((x + c * 67) + y * 236) * 119) & 255) / 128.0f - 1.0f;
}

static float
positional_mask_x_float32(int x, int y, int c)
{
    return ((((x + c * 29) ^ (y * 149)) * 1234) & 511) / 256.0f - 1.0f;
}

SIXELSTATUS
sixel_dither_apply_positional_float32(sixel_dither_t *dither,
                                      sixel_dither_context_t *context)
{
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    int serpentine;
    int y;
    int absolute_y;
    float (*f_mask)(int x, int y, int c);
    float jitter_scale;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int float_index;
    unsigned char *quantized;
    float lookup_pixel_float[max_channels];
    unsigned char const *lookup_pixel;
    sixel_lut_t *fast_lut;
    int use_fast_lut;
    int lookup_wants_float;
    int use_palette_float_lookup;
    int need_float_pixel;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    quantized = NULL;
    lookup_wants_float = 0;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->pixels_float == NULL || context->scratch == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->palette == NULL || context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (context->method_for_diffuse) {
    case SIXEL_DIFFUSE_A_DITHER:
        f_mask = positional_mask_a_float32;
        break;
    case SIXEL_DIFFUSE_X_DITHER:
    default:
        f_mask = positional_mask_x_float32;
        break;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    jitter_scale = 32.0f / 255.0f;
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    quantized = context->scratch;
    fast_lut = context->lut;
    use_fast_lut = (fast_lut != NULL);
    lookup_wants_float = (context->lookup_source_is_float != 0);
    use_palette_float_lookup = 0;
    if (context->prefer_palette_float_lookup != 0
            && palette_float != NULL
            && float_depth >= context->depth) {
        use_palette_float_lookup = 1;
    }
    need_float_pixel = lookup_wants_float || use_palette_float_lookup;

    if (context->optimize_palette) {
        int x;

        *context->ncolors = 0;
        memset(context->new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float, 0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth * sizeof(float));
        }
        memset(context->migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
        for (y = 0; y < context->height; ++y) {
            absolute_y = context->band_origin + y;
            int start;
            int end;
            int step;
            int direction;

            sixel_dither_scanline_params_positional_float32(serpentine, absolute_y,
                                         context->width,
                                         &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;
                int color_index;

                pos = y * context->width + x;
                for (d = 0; d < context->depth; ++d) {
                    float val;

                    val = context->pixels_float[pos * context->depth + d]
                        + f_mask(x, y, d) * jitter_scale;
                    val = sixel_pixelformat_float_channel_clamp(
                        context->pixelformat,
                        d,
                        val);
                    if (need_float_pixel) {
                        lookup_pixel_float[d] = val;
                    }
                    if (!lookup_wants_float && !use_palette_float_lookup) {
                        quantized[d]
                            = sixel_pixelformat_float_channel_to_byte(
                                  context->pixelformat,
                                  d,
                                  val);
                    }
                }
                if (lookup_wants_float) {
                    lookup_pixel = (unsigned char const *)(void const *)
                        lookup_pixel_float;
                    if (use_fast_lut) {
                        color_index = sixel_lut_map_pixel(fast_lut,
                                                         lookup_pixel);
                    } else {
                        color_index = context->lookup(lookup_pixel,
                                                      context->depth,
                                                      context->palette,
                                                      context->reqcolor,
                                                      context->indextable,
                                                      context->complexion);
                    }
                } else if (use_palette_float_lookup) {
                    color_index = sixel_dither_lookup_palette_float32(
                        lookup_pixel_float,
                        context->depth,
                        palette_float,
                        context->reqcolor,
                        context->complexion);
                } else {
                    lookup_pixel = quantized;
                    if (use_fast_lut) {
                        color_index = sixel_lut_map_pixel(fast_lut,
                                                         lookup_pixel);
                    } else {
                        color_index = context->lookup(lookup_pixel,
                                                      context->depth,
                                                      context->palette,
                                                      context->reqcolor,
                                                      context->indextable,
                                                      context->complexion);
                    }
                }
                if (context->migration_map[color_index] == 0) {
                    if (absolute_y >= context->output_start) {
                        /*
                         * Palette indices never exceed SIXEL_PALETTE_MAX, so
                         * the cast to sixel_index_t (unsigned char) is safe.
                         */
                        context->result[pos]
                            = (sixel_index_t)(*context->ncolors);
                    }
                    for (d = 0; d < context->depth; ++d) {
                        context->new_palette[*context->ncolors
                                             * context->depth + d]
                            = context->palette[color_index
                                               * context->depth + d];
                    }
                    if (palette_float != NULL
                            && new_palette_float != NULL
                            && float_depth > 0) {
                        for (float_index = 0;
                                float_index < float_depth;
                                ++float_index) {
                            new_palette_float[*context->ncolors
                                               * float_depth
                                               + float_index]
                                = palette_float[color_index * float_depth
                                                + float_index];
                        }
                    }
                    ++*context->ncolors;
                    /*
                     * Migration map entries are limited to the palette size
                     * (<= 256), so storing them as unsigned short is safe.
                     */
                    context->migration_map[color_index]
                        = (unsigned short)(*context->ncolors);
                } else {
                    if (absolute_y >= context->output_start) {
                        context->result[pos]
                            = (sixel_index_t)(context->migration_map[
                                  color_index] - 1);
                    }
                }
            }
            if (absolute_y >= context->output_start) {
                sixel_dither_pipeline_row_notify(dither, absolute_y);
            }
        }
        memcpy(context->palette, context->new_palette,
               (size_t)(*context->ncolors * context->depth));
        if (palette_float != NULL
                && new_palette_float != NULL
                && float_depth > 0) {
            memcpy(palette_float,
                   new_palette_float,
                   (size_t)(*context->ncolors * float_depth)
                       * sizeof(float));
        }
    } else {
        int x;

        for (y = 0; y < context->height; ++y) {
            absolute_y = context->band_origin + y;
            int start;
            int end;
            int step;
            int direction;

            sixel_dither_scanline_params_positional_float32(serpentine, absolute_y,
                                         context->width,
                                         &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;

                pos = y * context->width + x;
                for (d = 0; d < context->depth; ++d) {
                    float val;

                    val = context->pixels_float[pos * context->depth + d]
                        + f_mask(x, y, d) * jitter_scale;
                    val = sixel_pixelformat_float_channel_clamp(
                        context->pixelformat,
                        d,
                        val);
                    if (need_float_pixel) {
                        lookup_pixel_float[d] = val;
                    }
                    if (!lookup_wants_float && !use_palette_float_lookup) {
                        quantized[d]
                            = sixel_pixelformat_float_channel_to_byte(
                                  context->pixelformat,
                                  d,
                                  val);
                    }
                }
                if (absolute_y >= context->output_start) {
                    /*
                     * Palette indices never exceed SIXEL_PALETTE_MAX, so
                     * narrowing to sixel_index_t (unsigned char) is safe.
                     */
                    if (lookup_wants_float) {
                        lookup_pixel = (unsigned char const *)(void const *)
                            lookup_pixel_float;
                        context->result[pos] = (sixel_index_t)
                            context->lookup(
                                lookup_pixel,
                                context->depth,
                                context->palette,
                                context->reqcolor,
                                context->indextable,
                                context->complexion);
                    } else if (use_palette_float_lookup) {
                        context->result[pos] = (sixel_index_t)
                            sixel_dither_lookup_palette_float32(
                                lookup_pixel_float,
                                context->depth,
                                palette_float,
                                context->reqcolor,
                                context->complexion);
                    } else {
                        lookup_pixel = quantized;
                        context->result[pos] = (sixel_index_t)
                            context->lookup(
                                lookup_pixel,
                                context->depth,
                                context->palette,
                                context->reqcolor,
                                context->indextable,
                                context->complexion);
                    }
                }
            }
            if (absolute_y >= context->output_start) {
                sixel_dither_pipeline_row_notify(dither, absolute_y);
            }
        }
        *context->ncolors = context->reqcolor;
    }

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
