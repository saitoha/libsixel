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

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "dither-positional-float32.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "bluenoise_64x64.h"

#if SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) && \
        !defined(WITH_WINPTHREAD)
#  define SIXEL_POS_FLOAT32_USE_WIN32_ONCE 1
#  include <windows.h>
static INIT_ONCE g_sixel_pos_strength_once_float32 = INIT_ONCE_STATIC_INIT;
static INIT_ONCE g_sixel_bn_conf_once_float32 = INIT_ONCE_STATIC_INIT;
# else
#  include <pthread.h>
static pthread_once_t g_sixel_pos_strength_once_float32 = PTHREAD_ONCE_INIT;
static pthread_once_t g_sixel_bn_conf_once_float32 = PTHREAD_ONCE_INIT;
# endif
#endif

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

/*
 * Cache SIXEL_DITHER_*_DITHER_STRENGTH for positional arithmetic dithers to
 * avoid getenv() in inner loops. Invalid values fall back to defaults.
 */
static float g_sixel_pos_strength_a_float32 = 0.150f;
static float g_sixel_pos_strength_x_float32 = 0.100f;
#if !SIXEL_ENABLE_THREADS
/*
 * The single-thread fallback uses this flag to emulate one-time init without
 * pthread_once/InitOnceExecuteOnce.
 */
static int g_sixel_pos_inited_float32 = 0;
#endif

static void sixel_positional_strength_init_float32(void);
static void sixel_bluenoise_conf_init_from_env_float32(void);

static float
positional_mask_a_float32(int x, int y, int c)
{
    return (((((x + c * 67) + y * 236) * 119) & 255) / 128.0f
            - 1.0f) * g_sixel_pos_strength_a_float32;
}

static float
positional_mask_x_float32(int x, int y, int c)
{
    return (((((x + c * 29) ^ (y * 149)) * 1234) & 511) / 256.0f
            - 1.0f) * g_sixel_pos_strength_x_float32;
}

/*
 * Keep per-file suffixes so unity builds do not merge identical static
 * helper symbols from other positional dither sources.
 */
typedef struct {
    float strength;
    float gradient_factor;
    int ox;
    int oy;
    int per_channel;
    int size;
} sixel_bluenoise_conf_float32_t;

static float positional_mask_blue_with_conf_float32(
    sixel_bluenoise_conf_float32_t const *conf,
    int x,
    int y,
    int c);

static sixel_bluenoise_conf_float32_t g_sixel_bn_conf_float32;
#if !SIXEL_ENABLE_THREADS
/*
 * Keep the fallback init flag out of threaded builds so -Wunused-but-set-global
 * does not fire when pthread_once/InitOnceExecuteOnce is active.
 */
static int g_sixel_bn_inited_float32 = 0;
#endif

static int
sixel_bn_parse_int_float32(char const *text, int *out_value)
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
sixel_bn_parse_float_float32(char const *text, float *out_value)
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
sixel_bn_parse_phase_float32(char const *text, int *out_ox, int *out_oy)
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
sixel_positional_strength_init_body_float32(void)
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
        parsed = sixel_bn_parse_float_float32(text, &strength_a);
        if (parsed == 0) {
            strength_a = 0.150f;
        }
    }

    strength_x = 0.100f;
    text = sixel_compat_getenv("SIXEL_DITHER_X_DITHER_STRENGTH");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_float32(text, &strength_x);
        if (parsed == 0) {
            strength_x = 0.100f;
        }
    }

    g_sixel_pos_strength_a_float32 = strength_a;
    g_sixel_pos_strength_x_float32 = strength_x;
#if !SIXEL_ENABLE_THREADS
    g_sixel_pos_inited_float32 = 1;
#endif
}

#if SIXEL_ENABLE_THREADS && defined(SIXEL_POS_FLOAT32_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_positional_strength_once_cb_float32(PINIT_ONCE init_once,
                                          PVOID parameter,
                                          PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;
    sixel_positional_strength_init_body_float32();
    return TRUE;
}
#endif

static void
sixel_positional_strength_init_float32(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_POS_FLOAT32_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(&g_sixel_pos_strength_once_float32,
                                   sixel_positional_strength_once_cb_float32,
                                   NULL,
                                   NULL);
    if (executed == FALSE) {
        sixel_positional_strength_init_body_float32();
    }
# else
    int status;

    status = pthread_once(&g_sixel_pos_strength_once_float32,
                          sixel_positional_strength_init_body_float32);
    if (status != 0) {
        sixel_positional_strength_init_body_float32();
    }
# endif
#else
    if (g_sixel_pos_inited_float32 == 0) {
        sixel_positional_strength_init_body_float32();
    }
#endif
}

static unsigned int
sixel_bn_hash32_float32(unsigned int value)
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
sixel_bn_str_equal_nocase_float32(char const *left, char const *right)
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
sixel_bluenoise_conf_init_from_env_body_float32(void)
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
        parsed = sixel_bn_parse_float_float32(text, &strength);
        if (parsed == 0) {
            strength = 0.055f;
        }
    }
    gradient_factor = 0.0f;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR");
    if (text != NULL) {
        parsed = sixel_bn_parse_float_float32(text, &gradient_factor);
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
        parsed = sixel_bn_parse_phase_float32(text, &ox, &oy);
        if (parsed == 0) {
            ox = 0;
            oy = 0;
        }
    }
    if (phase_set == 0) {
        text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SEED");
        if (text != NULL) {
            parsed = sixel_bn_parse_int_float32(text, &seed);
            if (parsed != 0) {
                hash = sixel_bn_hash32_float32((unsigned int)seed);
                ox = (int)(hash & 63U);
                oy = (int)((hash >> 8) & 63U);
            }
        }
    }

    per_channel = 0;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_CHANNEL");
    if (text != NULL) {
        if (sixel_bn_str_equal_nocase_float32(text, "rgb") != 0) {
            per_channel = 1;
        } else if (sixel_bn_str_equal_nocase_float32(text, "mono") != 0) {
            per_channel = 0;
        }
    }

    size = SIXEL_BN_W;
    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SIZE");
    if (text != NULL) {
        parsed = sixel_bn_parse_int_float32(text, &size);
        if (parsed == 0 || size != SIXEL_BN_W) {
            size = SIXEL_BN_W;
        }
    }

    g_sixel_bn_conf_float32.strength = strength;
    g_sixel_bn_conf_float32.gradient_factor = gradient_factor;
    g_sixel_bn_conf_float32.ox = ox;
    g_sixel_bn_conf_float32.oy = oy;
    g_sixel_bn_conf_float32.per_channel = per_channel;
    g_sixel_bn_conf_float32.size = size;
#if !SIXEL_ENABLE_THREADS
    g_sixel_bn_inited_float32 = 1;
#endif
}

#if SIXEL_ENABLE_THREADS && defined(SIXEL_POS_FLOAT32_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_bluenoise_conf_once_cb_float32(PINIT_ONCE init_once,
                                     PVOID parameter,
                                     PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;
    sixel_bluenoise_conf_init_from_env_body_float32();
    return TRUE;
}
#endif

static void
sixel_bluenoise_conf_init_from_env_float32(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_POS_FLOAT32_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(&g_sixel_bn_conf_once_float32,
                                   sixel_bluenoise_conf_once_cb_float32,
                                   NULL,
                                   NULL);
    if (executed == FALSE) {
        sixel_bluenoise_conf_init_from_env_body_float32();
    }
# else
    int status;

    status = pthread_once(&g_sixel_bn_conf_once_float32,
                          sixel_bluenoise_conf_init_from_env_body_float32);
    if (status != 0) {
        sixel_bluenoise_conf_init_from_env_body_float32();
    }
# endif
#else
    if (g_sixel_bn_inited_float32 == 0) {
        sixel_bluenoise_conf_init_from_env_body_float32();
    }
#endif
}

/*
 * Apply per-dither CLI overrides on top of cached environment defaults.
 * The phase override has priority over seed, mirroring env semantics.
 */
static void
sixel_bluenoise_conf_apply_dither_overrides_float32(
    sixel_bluenoise_conf_float32_t *conf,
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
        hash = sixel_bn_hash32_float32((unsigned int)dither->bluenoise_seed);
        conf->ox = (int)(hash & 63U);
        conf->oy = (int)((hash >> 8) & 63U);
    }
}

static float
sixel_bluenoise_tri_with_conf_float32(
    sixel_bluenoise_conf_float32_t const *conf,
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
positional_mask_blue_with_conf_float32(
    sixel_bluenoise_conf_float32_t const *conf,
    int x,
    int y,
    int c)
{
    if (conf == NULL) {
        return 0.0f;
    }

    return sixel_bluenoise_tri_with_conf_float32(conf, x, y, c)
        * conf->strength;
}

static float
sixel_bluenoise_gradient_weight_float32(
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

static SIXELSTATUS
sixel_dither_apply_positional_float32_with_mode(
    sixel_dither_t *dither,
    sixel_dither_context_t *context,
    int mask_mode)
{
    int serpentine;
    int y;
    int absolute_y;
    float jitter_scale;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int float_index;
    unsigned char *quantized;
    float lookup_pixel_float[SIXEL_MAX_CHANNELS];
    unsigned char const *lookup_pixel;
    int lookup_wants_float;
    int use_palette_float_lookup;
    int need_float_pixel;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;
    sixel_bluenoise_conf_float32_t bluenoise_conf;
    float gradient_factor;
    float gradient_weight;
    float noise;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    quantized = NULL;
    lookup_wants_float = 0;
    bluenoise_conf.strength = 0.055f;
    bluenoise_conf.gradient_factor = 0.0f;
    bluenoise_conf.ox = 0;
    bluenoise_conf.oy = 0;
    bluenoise_conf.per_channel = 0;
    bluenoise_conf.size = SIXEL_BN_W;
    gradient_factor = 0.0f;
    gradient_weight = 1.0f;
    noise = 0.0f;
    memset(lookup_pixel_float, 0, sizeof(lookup_pixel_float));

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->pixels_float == NULL || context->scratch == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->palette == NULL || context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->depth <= 0 || context->depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (mask_mode == SIXEL_DIFFUSE_A_DITHER
            || mask_mode == SIXEL_DIFFUSE_X_DITHER) {
        sixel_positional_strength_init_float32();
    } else if (mask_mode == SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        sixel_bluenoise_conf_init_from_env_float32();
        bluenoise_conf = g_sixel_bn_conf_float32;
        sixel_bluenoise_conf_apply_dither_overrides_float32(&bluenoise_conf,
                                                            dither);
    } else {
        return SIXEL_BAD_ARGUMENT;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    jitter_scale = 32.0f / 255.0f;
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    quantized = context->scratch;
    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    use_transparent_fence = 0;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }
    if (mask_mode == SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        gradient_factor = bluenoise_conf.gradient_factor;
    }
    lookup_wants_float = (context->lookup_source_is_float != 0);
    use_palette_float_lookup = 0;
    if (context->prefer_palette_float_lookup != 0
            && palette_float != NULL
            && float_depth >= context->depth) {
        use_palette_float_lookup = 1;
    }
    need_float_pixel = lookup_wants_float || use_palette_float_lookup;

#define SIXEL_DITHER_APPLY_POSITIONAL_FLOAT32(NOISE_EXPR)               \
    do {                                                                 \
        if (context->optimize_palette) {                                 \
            int x;                                                        \
                                                                        \
            *context->ncolors = 0;                                       \
            memset(context->new_palette, 0x00,                           \
                   (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);  \
            if (new_palette_float != NULL && float_depth > 0) {          \
                memset(new_palette_float, 0x00,                          \
                       (size_t)SIXEL_PALETTE_MAX                         \
                           * (size_t)float_depth * sizeof(float));       \
            }                                                            \
            memset(context->migration_map, 0x00,                         \
                   sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);  \
            for (y = 0; y < context->height; ++y) {                      \
                int start;                                                \
                int end;                                                  \
                int step;                                                 \
                int direction;                                            \
                                                                        \
                absolute_y = context->band_origin + y;                   \
                sixel_dither_scanline_params_positional_float32(         \
                    serpentine, absolute_y,                              \
                    context->width,                                      \
                    &start, &end, &step, &direction);                    \
                (void)direction;                                         \
                for (x = start; x != end; x += step) {                   \
                    int pos;                                              \
                    int d;                                                \
                    int color_index;                                      \
                                                                        \
                    pos = y * context->width + x;                        \
                    is_transparent = 0;                                  \
                    if (use_transparent_fence && absolute_y >= 0) {      \
                        absolute_index = (size_t)absolute_y              \
                            * (size_t)context->width                     \
                            + (size_t)x;                                 \
                        if (absolute_index < transparent_mask_size        \
                                && transparent_mask[absolute_index]       \
                                    != 0U) {                             \
                            is_transparent = 1;                          \
                        }                                                \
                    }                                                    \
                    if (is_transparent) {                                \
                        if (absolute_y >= context->output_start) {       \
                            context->result[pos]                         \
                                = (sixel_index_t)transparent_keycolor;   \
                        }                                                \
                        continue;                                        \
                    }                                                    \
                    gradient_weight = 1.0f;                              \
                    if (gradient_factor > 0.0f) {                        \
                        gradient_weight                                  \
                            = sixel_bluenoise_gradient_weight_float32(   \
                                context,                                 \
                                x,                                       \
                                absolute_y,                              \
                                gradient_factor);                        \
                    }                                                    \
                    for (d = 0; d < context->depth; ++d) {               \
                        float val;                                        \
                                                                        \
                        noise = (NOISE_EXPR);                            \
                        val = context->pixels_float[pos * context->depth \
                                                   + d]                  \
                            + noise * jitter_scale;                      \
                        val = sixel_pixelformat_float_channel_clamp(     \
                            context->pixelformat,                        \
                            d,                                           \
                            val);                                        \
                        if (need_float_pixel) {                          \
                            lookup_pixel_float[d] = val;                 \
                        }                                                \
                        if (!lookup_wants_float                          \
                                && !use_palette_float_lookup) {          \
                            quantized[d]                                 \
                                = sixel_pixelformat_float_channel_to_byte\
                                    (context->pixelformat, d, val);      \
                        }                                                \
                    }                                                    \
                    if (lookup_wants_float) {                            \
                        lookup_pixel = (unsigned char const *)(void const*)\
                            lookup_pixel_float;                          \
                        color_index = context->lookup_map(               \
                            context->lookup_policy,                      \
                            lookup_pixel);                               \
                    } else if (use_palette_float_lookup) {               \
                        color_index = sixel_dither_lookup_palette_float32(\
                            lookup_pixel_float,                          \
                            context->depth,                              \
                            palette_float,                               \
                            context->reqcolor);                          \
                    } else {                                             \
                        lookup_pixel = quantized;                        \
                        color_index = context->lookup_map(               \
                            context->lookup_policy,                      \
                            lookup_pixel);                               \
                    }                                                    \
                    if (context->migration_map[color_index] == 0) {      \
                        if (absolute_y >= context->output_start) {       \
                            /*                                            \
                             * Palette indices never exceed              \
                             * SIXEL_PALETTE_MAX, so the cast to         \
                             * sixel_index_t (unsigned char) is safe.    \
                             */                                           \
                            context->result[pos] = (sixel_index_t)       \
                                (*context->ncolors);                     \
                        }                                                \
                        for (d = 0; d < context->depth; ++d) {           \
                            context->new_palette[*context->ncolors       \
                                                 * context->depth + d]   \
                                = context->palette[color_index           \
                                                   * context->depth      \
                                                   + d];                 \
                        }                                                \
                        if (palette_float != NULL                        \
                                && new_palette_float != NULL             \
                                && float_depth > 0) {                    \
                            for (float_index = 0;                        \
                                    float_index < float_depth;           \
                                    ++float_index) {                     \
                                new_palette_float[*context->ncolors      \
                                                   * float_depth         \
                                                   + float_index]        \
                                    = palette_float[color_index          \
                                                    * float_depth        \
                                                    + float_index];      \
                            }                                            \
                        }                                                \
                        ++*context->ncolors;                             \
                        /*                                                \
                         * Migration map entries are limited to the      \
                         * palette size (<= 256), so storing them as     \
                         * unsigned short is safe.                       \
                         */                                               \
                        context->migration_map[color_index]              \
                            = (unsigned short)(*context->ncolors);       \
                    } else {                                             \
                        if (absolute_y >= context->output_start) {       \
                            context->result[pos] = (sixel_index_t)(      \
                                context->migration_map[color_index] - 1);\
                        }                                                \
                    }                                                    \
                }                                                        \
                if (absolute_y >= context->output_start) {               \
                    sixel_dither_pipeline_row_notify(dither, absolute_y);\
                }                                                        \
            }                                                            \
            memcpy(context->palette, context->new_palette,               \
                   (size_t)(*context->ncolors * context->depth));        \
            if (palette_float != NULL                                    \
                    && new_palette_float != NULL                         \
                    && float_depth > 0) {                                \
                memcpy(palette_float,                                    \
                       new_palette_float,                                \
                       (size_t)(*context->ncolors * float_depth)         \
                           * sizeof(float));                             \
            }                                                            \
        } else {                                                         \
            int x;                                                        \
                                                                        \
            for (y = 0; y < context->height; ++y) {                      \
                int start;                                                \
                int end;                                                  \
                int step;                                                 \
                int direction;                                            \
                                                                        \
                absolute_y = context->band_origin + y;                   \
                sixel_dither_scanline_params_positional_float32(         \
                    serpentine, absolute_y,                              \
                    context->width,                                      \
                    &start, &end, &step, &direction);                    \
                (void)direction;                                         \
                for (x = start; x != end; x += step) {                   \
                    int pos;                                              \
                    int d;                                                \
                                                                        \
                    pos = y * context->width + x;                        \
                    is_transparent = 0;                                  \
                    if (use_transparent_fence && absolute_y >= 0) {      \
                        absolute_index = (size_t)absolute_y              \
                            * (size_t)context->width                     \
                            + (size_t)x;                                 \
                        if (absolute_index < transparent_mask_size        \
                                && transparent_mask[absolute_index]       \
                                    != 0U) {                             \
                            is_transparent = 1;                          \
                        }                                                \
                    }                                                    \
                    if (is_transparent) {                                \
                        if (absolute_y >= context->output_start) {       \
                            context->result[pos]                         \
                                = (sixel_index_t)transparent_keycolor;   \
                        }                                                \
                        continue;                                        \
                    }                                                    \
                    gradient_weight = 1.0f;                              \
                    if (gradient_factor > 0.0f) {                        \
                        gradient_weight                                  \
                            = sixel_bluenoise_gradient_weight_float32(   \
                                context,                                 \
                                x,                                       \
                                absolute_y,                              \
                                gradient_factor);                        \
                    }                                                    \
                    for (d = 0; d < context->depth; ++d) {               \
                        float val;                                        \
                                                                        \
                        noise = (NOISE_EXPR);                            \
                        val = context->pixels_float[pos * context->depth \
                                                   + d]                  \
                            + noise * jitter_scale;                      \
                        val = sixel_pixelformat_float_channel_clamp(     \
                            context->pixelformat,                        \
                            d,                                           \
                            val);                                        \
                        if (need_float_pixel) {                          \
                            lookup_pixel_float[d] = val;                 \
                        }                                                \
                        if (!lookup_wants_float                          \
                                && !use_palette_float_lookup) {          \
                            quantized[d]                                 \
                                = sixel_pixelformat_float_channel_to_byte\
                                    (context->pixelformat, d, val);      \
                        }                                                \
                    }                                                    \
                    if (absolute_y >= context->output_start) {           \
                        /*                                                \
                         * Palette indices never exceed                  \
                         * SIXEL_PALETTE_MAX, so narrowing to            \
                         * sixel_index_t (unsigned char) is safe.        \
                         */                                               \
                        if (lookup_wants_float) {                        \
                            lookup_pixel =                               \
                                (unsigned char const *)(void const*)     \
                                    lookup_pixel_float;                  \
                            context->result[pos] = (sixel_index_t)       \
                                context->lookup_map(context->lookup_policy,\
                                                    lookup_pixel);       \
                        } else if (use_palette_float_lookup) {           \
                            context->result[pos] = (sixel_index_t)       \
                                sixel_dither_lookup_palette_float32(     \
                                    lookup_pixel_float,                  \
                                    context->depth,                      \
                                    palette_float,                       \
                                    context->reqcolor);                  \
                        } else {                                         \
                            lookup_pixel = quantized;                    \
                            context->result[pos] = (sixel_index_t)       \
                                context->lookup_map(context->lookup_policy,\
                                                    lookup_pixel);       \
                        }                                                \
                    }                                                    \
                }                                                        \
                if (absolute_y >= context->output_start) {               \
                    sixel_dither_pipeline_row_notify(dither, absolute_y);\
                }                                                        \
            }                                                            \
            *context->ncolors = context->reqcolor;                       \
        }                                                                \
    } while (0)

    switch (mask_mode) {
    case SIXEL_DIFFUSE_A_DITHER:
        SIXEL_DITHER_APPLY_POSITIONAL_FLOAT32(
            positional_mask_a_float32(x, y, d));
        break;
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        SIXEL_DITHER_APPLY_POSITIONAL_FLOAT32(
            positional_mask_blue_with_conf_float32(&bluenoise_conf, x, y, d)
                * gradient_weight);
        break;
    case SIXEL_DIFFUSE_X_DITHER:
    default:
        SIXEL_DITHER_APPLY_POSITIONAL_FLOAT32(
            positional_mask_x_float32(x, y, d));
        break;
    }
#undef SIXEL_DITHER_APPLY_POSITIONAL_FLOAT32

    return SIXEL_OK;
}

SIXELSTATUS
sixel_dither_apply_a_dither_float32(sixel_dither_t *dither,
                                    sixel_dither_context_t *context)
{
    return sixel_dither_apply_positional_float32_with_mode(
        dither, context, SIXEL_DIFFUSE_A_DITHER);
}

SIXELSTATUS
sixel_dither_apply_x_dither_float32(sixel_dither_t *dither,
                                    sixel_dither_context_t *context)
{
    return sixel_dither_apply_positional_float32_with_mode(
        dither, context, SIXEL_DIFFUSE_X_DITHER);
}

SIXELSTATUS
sixel_dither_apply_bluenoise_float32(sixel_dither_t *dither,
                                     sixel_dither_context_t *context)
{
    return sixel_dither_apply_positional_float32_with_mode(
        dither, context, SIXEL_DIFFUSE_BLUENOISE_DITHER);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
