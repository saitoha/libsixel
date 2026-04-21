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
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "dither-positional-8bit.h"
#include "dither-common-pipeline.h"
#include "lookup-common.h"
#include "bluenoise_64x64.h"

#if SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) && \
        !defined(WITH_WINPTHREAD)
#  define SIXEL_POS_8BIT_USE_WIN32_ONCE 1
#  include <windows.h>
static INIT_ONCE g_sixel_pos_strength_once_8bit = INIT_ONCE_STATIC_INIT;
static INIT_ONCE g_sixel_bn_conf_once_8bit = INIT_ONCE_STATIC_INIT;
# else
#  include <pthread.h>
static pthread_once_t g_sixel_pos_strength_once_8bit = PTHREAD_ONCE_INIT;
static pthread_once_t g_sixel_bn_conf_once_8bit = PTHREAD_ONCE_INIT;
# endif
#endif

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
 * Cache SIXEL_DITHER_*_DITHER_STRENGTH for positional arithmetic dithers to
 * avoid getenv() in inner loops. Invalid values fall back to defaults.
 */
static float g_sixel_pos_strength_a_8bit = 0.150f;
static float g_sixel_pos_strength_x_8bit = 0.100f;
#if !SIXEL_ENABLE_THREADS
/*
 * The single-thread fallback uses this flag to emulate one-time init without
 * pthread_once/InitOnceExecuteOnce.
 */
static int g_sixel_pos_inited_8bit = 0;
#endif

/*
 * Use per-file suffixes for static helpers so unity builds do not see
 * duplicate identifiers when combining this file with other dither sources.
 */
typedef struct {
    float strength;
    float gradient_factor;
    int ox;
    int oy;
    int per_channel;
    int size;
} sixel_bluenoise_conf_8bit_t;

static float positional_mask_blue_with_conf_8bit(
    sixel_bluenoise_conf_8bit_t const *conf,
    int x,
    int y,
    int c);

static sixel_bluenoise_conf_8bit_t g_sixel_bn_conf_8bit;
#if !SIXEL_ENABLE_THREADS
/* See g_sixel_pos_inited_8bit above for why this flag is single-thread only. */
static int g_sixel_bn_inited_8bit = 0;
#endif

static void sixel_positional_strength_init_8bit(void);
static void sixel_bluenoise_conf_init_from_env_8bit(void);

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

static void
sixel_positional_strength_init_body_8bit(void)
{
    char const *text;
    float strength_a;
    float strength_x;
    int parsed;

    /*
     * Default strengths are per-dither values. Environment overrides use
     * the same parser for consistency and fall back to defaults on error.
     */
    strength_a = 0.150f;
    text = sixel_compat_getenv("SIXEL_DITHER_A_DITHER_STRENGTH");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_8bit(text, &strength_a);
        if (parsed == 0) {
            strength_a = 0.150f;
        }
    }

    strength_x = 0.100f;
    text = sixel_compat_getenv("SIXEL_DITHER_X_DITHER_STRENGTH");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_8bit(text, &strength_x);
        if (parsed == 0) {
            strength_x = 0.100f;
        }
    }

    g_sixel_pos_strength_a_8bit = strength_a;
    g_sixel_pos_strength_x_8bit = strength_x;
#if !SIXEL_ENABLE_THREADS
    g_sixel_pos_inited_8bit = 1;
#endif
}

#if SIXEL_ENABLE_THREADS && defined(SIXEL_POS_8BIT_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_positional_strength_once_cb_8bit(PINIT_ONCE init_once,
                                       PVOID parameter,
                                       PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;
    sixel_positional_strength_init_body_8bit();
    return TRUE;
}
#endif

static void
sixel_positional_strength_init_8bit(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_POS_8BIT_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(&g_sixel_pos_strength_once_8bit,
                                   sixel_positional_strength_once_cb_8bit,
                                   NULL,
                                   NULL);
    if (executed == FALSE) {
        sixel_positional_strength_init_body_8bit();
    }
# else
    int status;

    status = pthread_once(&g_sixel_pos_strength_once_8bit,
                          sixel_positional_strength_init_body_8bit);
    if (status != 0) {
        sixel_positional_strength_init_body_8bit();
    }
# endif
#else
    if (g_sixel_pos_inited_8bit == 0) {
        sixel_positional_strength_init_body_8bit();
    }
#endif
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
sixel_bluenoise_conf_init_from_env_body_8bit(void)
{
    char const *text;
    float strength;
    float gradient_factor;
    int size;
    int ox;
    int oy;
    int seed;
    int phase_set;
    int parsed;
    int per_channel;
    unsigned int hash;

    strength = 0.055f;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_STRENGTH");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_8bit(text, &strength);
        if (parsed == 0) {
            strength = 0.055f;
        }
    }
    gradient_factor = 0.0f;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_8bit(text, &gradient_factor);
        if (parsed == 0 || gradient_factor <= 0.0f) {
            gradient_factor = 0.0f;
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
    g_sixel_bn_conf_8bit.gradient_factor = gradient_factor;
    g_sixel_bn_conf_8bit.ox = ox;
    g_sixel_bn_conf_8bit.oy = oy;
    g_sixel_bn_conf_8bit.per_channel = per_channel;
    g_sixel_bn_conf_8bit.size = size;
#if !SIXEL_ENABLE_THREADS
    g_sixel_bn_inited_8bit = 1;
#endif
}

#if SIXEL_ENABLE_THREADS && defined(SIXEL_POS_8BIT_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_bluenoise_conf_once_cb_8bit(PINIT_ONCE init_once,
                                  PVOID parameter,
                                  PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;
    sixel_bluenoise_conf_init_from_env_body_8bit();
    return TRUE;
}
#endif

static void
sixel_bluenoise_conf_init_from_env_8bit(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_POS_8BIT_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(&g_sixel_bn_conf_once_8bit,
                                   sixel_bluenoise_conf_once_cb_8bit,
                                   NULL,
                                   NULL);
    if (executed == FALSE) {
        sixel_bluenoise_conf_init_from_env_body_8bit();
    }
# else
    int status;

    status = pthread_once(&g_sixel_bn_conf_once_8bit,
                          sixel_bluenoise_conf_init_from_env_body_8bit);
    if (status != 0) {
        sixel_bluenoise_conf_init_from_env_body_8bit();
    }
# endif
#else
    if (g_sixel_bn_inited_8bit == 0) {
        sixel_bluenoise_conf_init_from_env_body_8bit();
    }
#endif
}

/*
 * Apply per-dither CLI overrides on top of cached environment defaults.
 * The phase override has priority over seed, mirroring env semantics.
 */
static void
sixel_bluenoise_conf_apply_dither_overrides_8bit(
    sixel_bluenoise_conf_8bit_t *conf,
    sixel_dither_t const *dither)
{
    unsigned int hash;

    hash = 0U;
    if (conf == NULL || dither == NULL) {
        return;
    }

    if (dither->bluenoise_strength_override != 0) {
        conf->strength = dither->bluenoise_strength;
    }
    if (dither->bluenoise_gradient_factor_override != 0) {
        conf->gradient_factor = dither->bluenoise_gradient_factor;
    }
    if (conf->gradient_factor <= 0.0f) {
        conf->gradient_factor = 0.0f;
    }
    if (dither->bluenoise_channel_override != 0) {
        conf->per_channel = (dither->bluenoise_channel_rgb != 0) ? 1 : 0;
    }
    if (dither->bluenoise_size_override != 0
            && dither->bluenoise_size == SIXEL_BN_W) {
        conf->size = SIXEL_BN_W;
    }
    if (dither->bluenoise_phase_override != 0) {
        conf->ox = dither->bluenoise_phase_x;
        conf->oy = dither->bluenoise_phase_y;
    } else if (dither->bluenoise_seed_override != 0) {
        hash = sixel_bn_hash32_8bit((unsigned int)dither->bluenoise_seed);
        conf->ox = (int)(hash & 63U);
        conf->oy = (int)((hash >> 8) & 63U);
    }
}

static float
sixel_bluenoise_tri_with_conf_8bit(sixel_bluenoise_conf_8bit_t const *conf,
                                   int x,
                                   int y,
                                   int c)
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

    if (conf == NULL) {
        return 0.0f;
    }

    ox = conf->ox;
    oy = conf->oy;
    per_channel = conf->per_channel;
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
sixel_bluenoise_tri_8bit(int x, int y, int c)
{
    return sixel_bluenoise_tri_with_conf_8bit(&g_sixel_bn_conf_8bit, x, y, c);
}

static float
positional_mask_a_8bit(int x, int y, int c)
{
    return (((((x + c * 67) + y * 236) * 119) & 255) / 128.0f
            - 1.0f) * g_sixel_pos_strength_a_8bit;
}

static float
positional_mask_x_8bit(int x, int y, int c)
{
    return (((((x + c * 29) ^ (y * 149)) * 1234) & 511) / 256.0f
            - 1.0f) * g_sixel_pos_strength_x_8bit;
}

static float
positional_mask_blue_8bit(int x, int y, int c)
{
    return sixel_bluenoise_tri_8bit(x, y, c)
        * g_sixel_bn_conf_8bit.strength;
}

static float
positional_mask_blue_with_conf_8bit(sixel_bluenoise_conf_8bit_t const *conf,
                                    int x,
                                    int y,
                                    int c)
{
    if (conf == NULL) {
        return 0.0f;
    }

    return sixel_bluenoise_tri_with_conf_8bit(conf, x, y, c) * conf->strength;
}

static float
sixel_bluenoise_gradient_weight_8bit(
    sixel_dither_context_t const *context,
    int x,
    int absolute_y,
    float gamma)
{
    size_t offset;
    float normalized;
    float attenuated;

    offset = 0U;
    normalized = 0.0f;
    attenuated = 0.0f;

    if (context == NULL || gamma <= 0.0f) {
        return 1.0f;
    }
    if (context->bluenoise_gradient_map == NULL
            || context->bluenoise_gradient_width <= 0
            || context->bluenoise_gradient_height <= 0
            || absolute_y < 0
            || x < 0) {
        return 1.0f;
    }
    if (x >= context->bluenoise_gradient_width
            || absolute_y >= context->bluenoise_gradient_height) {
        return 1.0f;
    }

    offset = (size_t)absolute_y * (size_t)context->bluenoise_gradient_width
           + (size_t)x;
    if (offset >= context->bluenoise_gradient_map_size) {
        return 1.0f;
    }

    normalized = (float)context->bluenoise_gradient_map[offset] / 255.0f;
    if (normalized <= 0.0f) {
        return 1.0f;
    }
    if (normalized >= 1.0f) {
        return 0.0f;
    }

    attenuated = powf(normalized, gamma);
    if (attenuated < 0.0f) {
        attenuated = 0.0f;
    } else if (attenuated > 1.0f) {
        attenuated = 1.0f;
    }

    return 1.0f - attenuated;
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
    float (*f_mask)(int x, int y, int c);
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;
    sixel_bluenoise_conf_8bit_t bluenoise_conf;
    int use_bluenoise_conf;
    float gradient_factor;
    float gradient_weight;
    float noise;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    bluenoise_conf.strength = 0.055f;
    bluenoise_conf.gradient_factor = 0.0f;
    bluenoise_conf.ox = 0;
    bluenoise_conf.oy = 0;
    bluenoise_conf.per_channel = 0;
    bluenoise_conf.size = SIXEL_BN_W;
    use_bluenoise_conf = 0;
    gradient_factor = 0.0f;
    gradient_weight = 1.0f;
    noise = 0.0f;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->pixels == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL || context->scratch == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_mode < SIXEL_DITHER_LOOKUP_MODE_NORMAL
            || context->lookup_mode > SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (context->method_for_diffuse) {
    case SIXEL_DIFFUSE_A_DITHER:
        sixel_positional_strength_init_8bit();
        f_mask = positional_mask_a_8bit;
        break;
    case SIXEL_DIFFUSE_X_DITHER:
        sixel_positional_strength_init_8bit();
        f_mask = positional_mask_x_8bit;
        break;
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        sixel_bluenoise_conf_init_from_env_8bit();
        bluenoise_conf = g_sixel_bn_conf_8bit;
        sixel_bluenoise_conf_apply_dither_overrides_8bit(&bluenoise_conf,
                                                         dither);
        use_bluenoise_conf = 1;
        f_mask = positional_mask_blue_8bit;
        break;
    default:
        sixel_positional_strength_init_8bit();
        f_mask = positional_mask_x_8bit;
        break;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    use_transparent_fence = 0;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }
    if (use_bluenoise_conf != 0) {
        gradient_factor = bluenoise_conf.gradient_factor;
    }

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
                is_transparent = 0;
                if (use_transparent_fence && absolute_y >= 0) {
                    absolute_index = (size_t)absolute_y
                        * (size_t)context->width
                        + (size_t)x;
                    if (absolute_index < transparent_mask_size
                            && transparent_mask[absolute_index] != 0U) {
                        is_transparent = 1;
                    }
                }
                if (is_transparent) {
                    if (absolute_y >= context->output_start) {
                        context->result[pos]
                            = (sixel_index_t)transparent_keycolor;
                    }
                    continue;
                }
                gradient_weight = 1.0f;
                if (use_bluenoise_conf != 0 && gradient_factor > 0.0f) {
                    gradient_weight = sixel_bluenoise_gradient_weight_8bit(
                        context,
                        x,
                        absolute_y,
                        gradient_factor);
                }
                for (d = 0; d < context->depth; ++d) {
                    int val;

                    if (use_bluenoise_conf != 0) {
                        noise = positional_mask_blue_with_conf_8bit(
                            &bluenoise_conf,
                            x,
                            y,
                            d);
                        noise *= gradient_weight;
                    } else {
                        noise = f_mask(x, y, d);
                    }
                    val = context->pixels[pos * context->depth + d]
                        + (int)(noise * 32.0f);
                    /*
                     * The clamp keeps values within the byte range so the
                     * cast to unsigned char is lossless and silences MSVC
                     * C4244 diagnostics.
                     */
                    context->scratch[d] = (unsigned char)(val < 0 ? 0
                                                      : val > 255 ? 255
                                                                     : val);
                }
                color_index = sixel_dither_lookup_index(context,
                                                        context->scratch);
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
                is_transparent = 0;
                if (use_transparent_fence && absolute_y >= 0) {
                    absolute_index = (size_t)absolute_y
                        * (size_t)context->width
                        + (size_t)x;
                    if (absolute_index < transparent_mask_size
                            && transparent_mask[absolute_index] != 0U) {
                        is_transparent = 1;
                    }
                }
                if (is_transparent) {
                    if (absolute_y >= context->output_start) {
                        context->result[pos]
                            = (sixel_index_t)transparent_keycolor;
                    }
                    continue;
                }
                gradient_weight = 1.0f;
                if (use_bluenoise_conf != 0 && gradient_factor > 0.0f) {
                    gradient_weight = sixel_bluenoise_gradient_weight_8bit(
                        context,
                        x,
                        absolute_y,
                        gradient_factor);
                }
                for (d = 0; d < context->depth; ++d) {
                    int val;

                    if (use_bluenoise_conf != 0) {
                        noise = positional_mask_blue_with_conf_8bit(
                            &bluenoise_conf,
                            x,
                            y,
                            d);
                        noise *= gradient_weight;
                    } else {
                        noise = f_mask(x, y, d);
                    }
                    val = context->pixels[pos * context->depth + d]
                        + (int)(noise * 32.0f);
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
                        sixel_dither_lookup_index(context, context->scratch);
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
