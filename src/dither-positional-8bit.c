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

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "dither-positional-8bit.h"
#include "dither-common-pipeline.h"
#include "lookup-common.h"
#include "bluenoise_64x64.h"

static void
sixel_dither_scanline_params_positional_8bit(int serpentine,
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

static float positional_mask_a_8bit(int x, int y, int c);
static float positional_mask_x_8bit(int x, int y, int c);
static float positional_mask_blue_8bit(int x, int y, int c);

/*
 * Use per-file suffixes for static helpers so unity builds do not see
 * duplicate identifiers when combining this file with other dither sources.
 */
typedef struct {
    float strength;
    int ox;
    int oy;
    int per_channel;
    int size;
} sixel_bluenoise_conf_8bit_t;

static sixel_bluenoise_conf_8bit_t g_sixel_bn_conf_8bit;
static int g_sixel_bn_inited_8bit = 0;

static int
sixel_bn_parse_int_8bit(char const *text, int *out_value)
{
    char *endptr;
    long value;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }
    if (value > INT_MAX || value < INT_MIN) {
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static int
sixel_bn_parse_float_8bit(char const *text, float *out_value)
{
    char *endptr;
    double value;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtod(text, &endptr);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *out_value = (float)value;
    return 1;
}

static int
sixel_bn_parse_phase_8bit(char const *text, int *out_ox, int *out_oy)
{
    char *endptr;
    char const *comma;
    long ox;
    long oy;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    comma = strchr(text, ',');
    if (comma == NULL) {
        return 0;
    }

    ox = strtol(text, &endptr, 10);
    if (endptr == text || endptr != comma) {
        return 0;
    }

    oy = strtol(comma + 1, &endptr, 10);
    if (endptr == comma + 1 || *endptr != '\0') {
        return 0;
    }
    if (ox > INT_MAX || ox < INT_MIN || oy > INT_MAX || oy < INT_MIN) {
        return 0;
    }

    *out_ox = (int)ox;
    *out_oy = (int)oy;
    return 1;
}

static unsigned int
sixel_bn_hash32_8bit(unsigned int value)
{
    value += 0x9e3779b9U;
    value ^= value >> 16;
    value *= 0x85ebca6bU;
    value ^= value >> 13;
    value *= 0xc2b2ae35U;
    value ^= value >> 16;
    return value;
}

static int
sixel_bn_str_equal_nocase_8bit(char const *left, char const *right)
{
    unsigned char lc;
    unsigned char rc;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        lc = (unsigned char)tolower((unsigned char)*left);
        rc = (unsigned char)tolower((unsigned char)*right);
        if (lc != rc) {
            return 0;
        }
        ++left;
        ++right;
    }

    return (*left == '\0' && *right == '\0');
}

/*
 * Cache bluenoise configuration at first use so we do not hit getenv()
 * inside pixel loops. Invalid values fall back to defaults.
 */
static void
sixel_bluenoise_conf_init_from_env_8bit(void)
{
    char const *text;
    float strength;
    int size;
    int ox;
    int oy;
    int seed;
    int phase_set;
    int parsed;
    int per_channel;
    unsigned int hash;

    if (g_sixel_bn_inited_8bit != 0) {
        return;
    }

    strength = 1.0f;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_STRENGTH");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_8bit(text, &strength);
        if (parsed == 0) {
            strength = 1.0f;
        }
    } else {
        text = sixel_compat_getenv("SIXEL_DITHER_STRENGTH");
        if (text != NULL) {
            parsed = sixel_bn_parse_float_8bit(text, &strength);
            if (parsed == 0) {
                strength = 1.0f;
            }
        }
    }

    ox = 0;
    oy = 0;
    phase_set = 0;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_PHASE");
    if (text != NULL) {
        phase_set = 1;
        parsed = sixel_bn_parse_phase_8bit(text, &ox, &oy);
        if (parsed == 0) {
            ox = 0;
            oy = 0;
        }
    }
    if (phase_set == 0) {
        text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SEED");
        if (text != NULL) {
            parsed = sixel_bn_parse_int_8bit(text, &seed);
            if (parsed != 0) {
                hash = sixel_bn_hash32_8bit((unsigned int)seed);
                ox = (int)(hash & 63U);
                oy = (int)((hash >> 8) & 63U);
            }
        }
    }

    per_channel = 0;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_CHANNEL");
    if (text != NULL) {
        if (sixel_bn_str_equal_nocase_8bit(text, "rgb") != 0) {
            per_channel = 1;
        } else if (sixel_bn_str_equal_nocase_8bit(text, "mono") != 0) {
            per_channel = 0;
        }
    }

    size = SIXEL_BN_W;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SIZE");
    if (text != NULL) {
        parsed = sixel_bn_parse_int_8bit(text, &size);
        if (parsed == 0 || size != SIXEL_BN_W) {
            size = SIXEL_BN_W;
        }
    }

    g_sixel_bn_conf_8bit.strength = strength;
    g_sixel_bn_conf_8bit.ox = ox;
    g_sixel_bn_conf_8bit.oy = oy;
    g_sixel_bn_conf_8bit.per_channel = per_channel;
    g_sixel_bn_conf_8bit.size = size;
    g_sixel_bn_inited_8bit = 1;
}

static float
sixel_bluenoise_tri_8bit(int x, int y, int c)
{
    /* Triangular noise blends two samples from the same tile. */
    static int const channel_offset_x[3] = { 17, 34, 51 };
    static int const channel_offset_y[3] = { 31, 62, 93 };
    int ox;
    int oy;
    int per_channel;
    int channel_x;
    int channel_y;
    int ix0;
    int iy0;
    int ix1;
    int iy1;
    float u;
    float v;

    ox = g_sixel_bn_conf_8bit.ox;
    oy = g_sixel_bn_conf_8bit.oy;
    per_channel = g_sixel_bn_conf_8bit.per_channel;
    channel_x = 0;
    channel_y = 0;
    if (per_channel != 0 && c >= 0 && c < 3) {
        channel_x = channel_offset_x[c];
        channel_y = channel_offset_y[c];
    }

    ix0 = x + ox + channel_x;
    iy0 = y + oy + channel_y;
    ix1 = ix0 + 13;
    iy1 = iy0 + 29;
    u = (sixel_bn_mask(ix0, iy0) + 1.0f) * 0.5f;
    v = (sixel_bn_mask(ix1, iy1) + 1.0f) * 0.5f;

    return (u + v) - 1.0f;
}

static float
positional_mask_a_8bit(int x, int y, int c)
{
    return ((((x + c * 67) + y * 236) * 119) & 255) / 128.0f - 1.0f;
}

static float
positional_mask_x_8bit(int x, int y, int c)
{
    return ((((x + c * 29) ^ (y * 149)) * 1234) & 511) / 256.0f - 1.0f;
}

static float
positional_mask_blue_8bit(int x, int y, int c)
{
    sixel_bluenoise_conf_init_from_env_8bit();
    return sixel_bluenoise_tri_8bit(x, y, c)
        * g_sixel_bn_conf_8bit.strength;
}

SIXELSTATUS
sixel_dither_apply_positional_8bit(sixel_dither_t *dither,
                                   sixel_dither_context_t *context)
{
    int serpentine;
    int y;
    int absolute_y;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int float_index;
    sixel_lut_t *fast_lut;
    int use_fast_lut;
    float (*f_mask)(int x, int y, int c);

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->pixels == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL || context->scratch == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (context->method_for_diffuse) {
    case SIXEL_DIFFUSE_A_DITHER:
        f_mask = positional_mask_a_8bit;
        break;
    case SIXEL_DIFFUSE_X_DITHER:
        f_mask = positional_mask_x_8bit;
        break;
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        f_mask = positional_mask_blue_8bit;
        break;
    default:
        f_mask = positional_mask_x_8bit;
        break;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    fast_lut = context->lut;
    use_fast_lut = (fast_lut != NULL);

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

            sixel_dither_scanline_params_positional_8bit(serpentine, absolute_y,
                                         context->width,
                                         &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;
                int color_index;

                pos = y * context->width + x;
                for (d = 0; d < context->depth; ++d) {
                    int val;

                    val = context->pixels[pos * context->depth + d]
                        + (int)(f_mask(x, y, d) * 32.0f);
                    /*
                     * The clamp keeps values within the byte range so the
                     * cast to unsigned char is lossless and silences MSVC
                     * C4244 diagnostics.
                     */
                    context->scratch[d] = (unsigned char)(val < 0 ? 0
                                                      : val > 255 ? 255
                                                                     : val);
                }
                if (use_fast_lut) {
                    color_index = sixel_lut_map_pixel(fast_lut,
                                                     context->scratch);
                } else {
                    color_index = context->lookup(context->scratch,
                                                  context->depth,
                                                  context->palette,
                                                  context->reqcolor,
                                                  context->indextable,
                                                  context->complexion);
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

            sixel_dither_scanline_params_positional_8bit(serpentine, absolute_y,
                                         context->width,
                                         &start, &end, &step, &direction);
            (void)direction;
            for (x = start; x != end; x += step) {
                int pos;
                int d;

                pos = y * context->width + x;
                for (d = 0; d < context->depth; ++d) {
                    int val;

                    val = context->pixels[pos * context->depth + d]
                        + (int)(f_mask(x, y, d) * 32.0f);
                    /*
                     * The clamp keeps values within the byte range so the
                     * cast to unsigned char is lossless and silences MSVC
                     * C4244 diagnostics.
                     */
                    context->scratch[d] = (unsigned char)(val < 0 ? 0
                                                      : val > 255 ? 255
                                                                     : val);
                }
                if (absolute_y >= context->output_start) {
                    /*
                     * Palette indices are limited to SIXEL_PALETTE_MAX, so the
                     * cast to sixel_index_t (unsigned char) is safe here.
                     */
                    context->result[pos] = (sixel_index_t)
                        context->lookup(context->scratch,
                                        context->depth,
                                        context->palette,
                                        context->reqcolor,
                                        context->indextable,
                                        context->complexion);
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
