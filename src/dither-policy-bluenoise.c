/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
#include <stdlib.h>
#include <string.h>
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#include "compat_stub.h"
#include "dither-policy-bluenoise.h"
#include "dither.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "sixel_atomic.h"
#include "bluenoise_64x64.h"

/*
 * Private dither context for this policy implementation.
 * Keep only members used by this translation unit.
 */
typedef struct sixel_dither_policy_bluenoise_context {
    sixel_index_t *result;
    unsigned char *pixels;
    float *pixels_float;
    int width;
    int height;
    int band_origin;
    int output_start;
    int depth;
    unsigned char *palette;
    float *palette_float;
    int reqcolor;
    int method_for_scan;
    struct sixel_lookup_policy_interface *lookup_policy;
    sixel_dither_lookup_map_fn lookup_map;
    unsigned char *scratch;
    unsigned char *new_palette;
    float *new_palette_float;
    unsigned short *migration_map;
    int *ncolors;
    int pixelformat;
    int float_depth;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    unsigned char const *bluenoise_gradient_map;
    size_t bluenoise_gradient_map_size;
    int bluenoise_gradient_width;
    int bluenoise_gradient_height;
} sixel_dither_policy_bluenoise_context_t;

/*
 * The bluenoise policy resolves all runtime options once per apply call so
 * the inner loops do not need to re-read environment or CLI override state.
 */
typedef struct sixel_bluenoise_conf {
    float strength;
    float gradient_factor;
    int phase_x;
    int phase_y;
    int channel_rgb;
    int size;
} sixel_bluenoise_conf_t;

static int
sixel_dither_parse_float_env(char const *text, float *out_value)
{
    char *endptr;
    double value;

    endptr = NULL;
    value = 0.0;
    if (text == NULL || out_value == NULL || text[0] == '\0') {
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
sixel_dither_parse_int_env(char const *text, int *out_value)
{
    char *endptr;
    long value;

    endptr = NULL;
    value = 0L;
    if (text == NULL || out_value == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static int
sixel_dither_parse_phase_env(char const *text, int *out_x, int *out_y)
{
    char *endptr;
    int x;
    int y;

    endptr = NULL;
    x = 0;
    y = 0;
    if (text == NULL || out_x == NULL || out_y == NULL || text[0] == '\0') {
        return 0;
    }

    x = (int)strtol(text, &endptr, 10);
    if (endptr == text || endptr == NULL || *endptr != ',') {
        return 0;
    }

    text = endptr + 1;
    y = (int)strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *out_x = x;
    *out_y = y;
    return 1;
}

static int
sixel_dither_channel_is_rgb(char const *text)
{
    char value0;
    char value1;
    char value2;
    char value3;

    value0 = '\0';
    value1 = '\0';
    value2 = '\0';
    value3 = '\0';
    if (text == NULL) {
        return 0;
    }

    value0 = (char)tolower((unsigned char)text[0]);
    value1 = (char)tolower((unsigned char)text[1]);
    value2 = (char)tolower((unsigned char)text[2]);
    value3 = text[3];
    return value0 == 'r' && value1 == 'g' && value2 == 'b' && value3 == '\0';
}

static unsigned int
sixel_bluenoise_hash32(unsigned int value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static void
sixel_bluenoise_conf_init(sixel_bluenoise_conf_t *conf)
{
    char const *text;
    int value;
    unsigned int hash;

    text = NULL;
    value = 0;
    hash = 0U;
    if (conf == NULL) {
        return;
    }

    conf->strength = 0.055f;
    conf->gradient_factor = 0.0f;
    conf->phase_x = 0;
    conf->phase_y = 0;
    conf->channel_rgb = 0;
    conf->size = SIXEL_BN_W;

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_STRENGTH");
    if (text != NULL) {
        (void)sixel_dither_parse_float_env(text, &conf->strength);
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR");
    if (text != NULL) {
        (void)sixel_dither_parse_float_env(text, &conf->gradient_factor);
        if (conf->gradient_factor < 0.0f) {
            conf->gradient_factor = 0.0f;
        }
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_CHANNEL");
    if (text != NULL) {
        conf->channel_rgb = sixel_dither_channel_is_rgb(text);
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SIZE");
    if (text != NULL
            && sixel_dither_parse_int_env(text, &value) != 0
            && value == SIXEL_BN_W) {
        conf->size = value;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_PHASE");
    if (text != NULL
            && sixel_dither_parse_phase_env(text,
                                            &conf->phase_x,
                                            &conf->phase_y) != 0) {
        return;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SEED");
    if (text != NULL && sixel_dither_parse_int_env(text, &value) != 0) {
        hash = sixel_bluenoise_hash32((unsigned int)value);
        conf->phase_x = (int)(hash & 63U);
        conf->phase_y = (int)((hash >> 8) & 63U);
    }
}

static void
sixel_bluenoise_conf_apply_dither_overrides(sixel_bluenoise_conf_t *conf,
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
        if (conf->gradient_factor < 0.0f) {
            conf->gradient_factor = 0.0f;
        }
    }
    if (dither->bluenoise_channel_override != 0) {
        conf->channel_rgb = (dither->bluenoise_channel_rgb != 0) ? 1 : 0;
    }
    if (dither->bluenoise_size_override != 0
            && dither->bluenoise_size == SIXEL_BN_W) {
        conf->size = dither->bluenoise_size;
    }

    if (dither->bluenoise_phase_override != 0) {
        conf->phase_x = dither->bluenoise_phase_x;
        conf->phase_y = dither->bluenoise_phase_y;
    } else if (dither->bluenoise_seed_override != 0) {
        hash = sixel_bluenoise_hash32((unsigned int)dither->bluenoise_seed);
        conf->phase_x = (int)(hash & 63U);
        conf->phase_y = (int)((hash >> 8) & 63U);
    }
}

static float
sixel_bluenoise_tri_noise(sixel_bluenoise_conf_t const *conf,
                          int x,
                          int y,
                          int c)
{
    static int const channel_offset_x[3] = { 17, 34, 51 };
    static int const channel_offset_y[3] = { 31, 62, 93 };
    int channel_x;
    int channel_y;
    int ix0;
    int iy0;
    int ix1;
    int iy1;
    float sample_u;
    float sample_v;

    channel_x = 0;
    channel_y = 0;
    ix0 = 0;
    iy0 = 0;
    ix1 = 0;
    iy1 = 0;
    sample_u = 0.0f;
    sample_v = 0.0f;
    if (conf == NULL || conf->size != SIXEL_BN_W) {
        return 0.0f;
    }

    if (conf->channel_rgb != 0 && c >= 0 && c < 3) {
        channel_x = channel_offset_x[c];
        channel_y = channel_offset_y[c];
    }

    ix0 = x + conf->phase_x + channel_x;
    iy0 = y + conf->phase_y + channel_y;
    ix1 = ix0 + 13;
    iy1 = iy0 + 29;
    sample_u = (sixel_bn_mask(ix0, iy0) + 1.0f) * 0.5f;
    sample_v = (sixel_bn_mask(ix1, iy1) + 1.0f) * 0.5f;
    return (sample_u + sample_v) - 1.0f;
}

static float
sixel_dither_bluenoise_noise(sixel_bluenoise_conf_t const *conf,
                             int x,
                             int y,
                             int c)
{
    if (conf == NULL) {
        return 0.0f;
    }
    return sixel_bluenoise_tri_noise(conf, x, y, c) * conf->strength;
}

static float
sixel_bluenoise_gradient_weight(sixel_dither_policy_bluenoise_context_t const *context,
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
            || x < 0
            || absolute_y < 0
            || x >= context->bluenoise_gradient_width
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

static void
sixel_dither_scanline_params_8bit(int serpentine,
                                  int index,
                                  int limit,
                                  int *start,
                                  int *end,
                                  int *step,
                                  int *direction)
{
    if (serpentine != 0 && (index & 1) != 0) {
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

static void
sixel_dither_scanline_params_float32(int serpentine,
                                     int index,
                                     int limit,
                                     int *start,
                                     int *end,
                                     int *step,
                                     int *direction)
{
    sixel_dither_scanline_params_8bit(serpentine,
                                      index,
                                      limit,
                                      start,
                                      end,
                                      step,
                                      direction);
}

/*
 * Return non-zero when the current pixel must be forced to the transparent
 * keycolor.
 */
static int
sixel_dither_is_transparent_pixel(sixel_dither_policy_bluenoise_context_t const *context,
                                  unsigned char const *transparent_mask,
                                  size_t transparent_mask_size,
                                  int use_transparent_fence,
                                  int x,
                                  int absolute_y)
{
    size_t absolute_index;

    absolute_index = 0U;
    if (context == NULL
            || transparent_mask == NULL
            || use_transparent_fence == 0
            || absolute_y < 0
            || x < 0) {
        return 0;
    }

    absolute_index = (size_t)absolute_y * (size_t)context->width + (size_t)x;
    if (absolute_index >= transparent_mask_size) {
        return 0;
    }

    return transparent_mask[absolute_index] != 0U;
}

static SIXELSTATUS
sixel_dither_apply_bluenoise_8bit(sixel_dither_t *dither,
                                 sixel_dither_policy_bluenoise_context_t *context)
{
    int serpentine;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    int d;
    int val;
    int color_index;
    int float_index;
    sixel_bluenoise_conf_t bluenoise_conf;
    float gradient_weight;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;

    serpentine = 0;
    y = 0;
    absolute_y = 0;
    start = 0;
    end = 0;
    step = 0;
    direction = 0;
    x = 0;
    pos = 0;
    d = 0;
    val = 0;
    color_index = 0;
    float_index = 0;
    memset(&bluenoise_conf, 0, sizeof(bluenoise_conf));
    gradient_weight = 1.0f;
    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    (void)float_index;
    (void)palette_float;
    (void)new_palette_float;
    (void)float_depth;
    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = -1;
    use_transparent_fence = 0;
    is_transparent = 0;

    if (dither == NULL || context == NULL
            || context->pixels == NULL
            || context->palette == NULL
            || context->result == NULL
            || context->scratch == NULL
            || context->lookup_policy == NULL
            || context->lookup_map == NULL
            || context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_bluenoise_conf_init(&bluenoise_conf);
    sixel_bluenoise_conf_apply_dither_overrides(&bluenoise_conf, dither);

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;

    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }


    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        sixel_dither_scanline_params_8bit(serpentine,
                                          absolute_y,
                                          context->width,
                                          &start,
                                          &end,
                                          &step,
                                          &direction);
        (void)direction;

        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            is_transparent = sixel_dither_is_transparent_pixel(
                context,
                transparent_mask,
                transparent_mask_size,
                use_transparent_fence,
                x,
                absolute_y);
            if (is_transparent != 0) {
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)transparent_keycolor;
                }
                continue;
            }

            gradient_weight = 1.0f;
            if (bluenoise_conf.gradient_factor > 0.0f) {
                gradient_weight = sixel_bluenoise_gradient_weight(
                    context,
                    x,
                    absolute_y,
                    bluenoise_conf.gradient_factor);
            }

            for (d = 0; d < context->depth; ++d) {
                val = context->pixels[pos * context->depth + d]
                    + (int)(sixel_dither_bluenoise_noise(
                                &bluenoise_conf,
                                x,
                                y,
                                d) * gradient_weight * 32.0f);
                context->scratch[d] = (unsigned char)(
                    val < 0 ? 0 : val > 255 ? 255 : val);
            }

            color_index = context->lookup_map(context->lookup_policy,
                                              context->scratch);

            if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)color_index;
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

        *context->ncolors = context->reqcolor;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_apply_bluenoise_float32(sixel_dither_t *dither,
                                    sixel_dither_policy_bluenoise_context_t *context)
{
    int serpentine;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    int d;
    int color_index;
    int float_index;
    sixel_bluenoise_conf_t bluenoise_conf;
    float gradient_weight;
    float jitter_scale;
    float noise;
    float val;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
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

    serpentine = 0;
    y = 0;
    absolute_y = 0;
    start = 0;
    end = 0;
    step = 0;
    direction = 0;
    x = 0;
    pos = 0;
    d = 0;
    color_index = 0;
    float_index = 0;
    memset(&bluenoise_conf, 0, sizeof(bluenoise_conf));
    gradient_weight = 1.0f;
    jitter_scale = 32.0f / 255.0f;
    noise = 0.0f;
    val = 0.0f;
    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    (void)float_index;
    (void)new_palette_float;
    quantized = NULL;
    memset(lookup_pixel_float, 0, sizeof(lookup_pixel_float));
    lookup_pixel = NULL;
    lookup_wants_float = 0;
    use_palette_float_lookup = 0;
    need_float_pixel = 0;
    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = -1;
    use_transparent_fence = 0;
    is_transparent = 0;

    if (dither == NULL || context == NULL
            || context->pixels_float == NULL
            || context->scratch == NULL
            || context->palette == NULL
            || context->result == NULL
            || context->lookup_policy == NULL
            || context->lookup_map == NULL
            || context->ncolors == NULL
            || context->depth <= 0
            || context->depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_bluenoise_conf_init(&bluenoise_conf);
    sixel_bluenoise_conf_apply_dither_overrides(&bluenoise_conf, dither);

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    quantized = context->scratch;

    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }

    lookup_wants_float = (context->lookup_source_is_float != 0);
    if (context->prefer_palette_float_lookup != 0
            && palette_float != NULL
            && float_depth >= context->depth) {
        use_palette_float_lookup = 1;
    }
    need_float_pixel = lookup_wants_float || use_palette_float_lookup;


    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        sixel_dither_scanline_params_float32(serpentine,
                                             absolute_y,
                                             context->width,
                                             &start,
                                             &end,
                                             &step,
                                             &direction);
        (void)direction;

        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            is_transparent = sixel_dither_is_transparent_pixel(
                context,
                transparent_mask,
                transparent_mask_size,
                use_transparent_fence,
                x,
                absolute_y);
            if (is_transparent != 0) {
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)transparent_keycolor;
                }
                continue;
            }

            gradient_weight = 1.0f;
            if (bluenoise_conf.gradient_factor > 0.0f) {
                gradient_weight = sixel_bluenoise_gradient_weight(
                    context,
                    x,
                    absolute_y,
                    bluenoise_conf.gradient_factor);
            }

            for (d = 0; d < context->depth; ++d) {
                noise = sixel_dither_bluenoise_noise(&bluenoise_conf, x, y, d)
                    * gradient_weight;
                val = context->pixels_float[pos * context->depth + d]
                    + noise * jitter_scale;
                val = sixel_pixelformat_float_channel_clamp(
                    context->pixelformat,
                    d,
                    val);
                if (need_float_pixel) {
                    lookup_pixel_float[d] = val;
                }
                if (!lookup_wants_float && !use_palette_float_lookup) {
                    quantized[d] = sixel_pixelformat_float_channel_to_byte(
                        context->pixelformat,
                        d,
                        val);
                }
            }

            if (lookup_wants_float) {
                lookup_pixel = (unsigned char const *)(void const *)
                    lookup_pixel_float;
                color_index = context->lookup_map(context->lookup_policy,
                                                  lookup_pixel);
            } else if (use_palette_float_lookup) {
                color_index = sixel_dither_lookup_palette_float32(
                    lookup_pixel_float,
                    context->depth,
                    palette_float,
                    context->reqcolor);
            } else {
                lookup_pixel = quantized;
                color_index = context->lookup_map(context->lookup_policy,
                                                  lookup_pixel);
            }

            if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)color_index;
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

        *context->ncolors = context->reqcolor;

    return SIXEL_OK;
}

/*
 * IDL (internal contract)
 *
 * class DitherPolicyXDither : IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 */
/*
 * IDL (internal contract)
 *
 * class DitherPolicyBluenoise : IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 */
typedef struct sixel_dither_policy_bluenoise_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_bluenoise_object_t;

static sixel_dither_policy_bluenoise_object_t *
sixel_dither_policy_bluenoise_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_bluenoise_object_t *)(void *)policy;
}

static sixel_dither_policy_bluenoise_object_t const *
sixel_dither_policy_bluenoise_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_bluenoise_object_t const *)(void const *)policy;
}

static void
sixel_dither_policy_bluenoise_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_bluenoise_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_bluenoise_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_bluenoise_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_bluenoise_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_bluenoise_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_dither_policy_bluenoise_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_bluenoise_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_bluenoise_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_bluenoise_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective)
{
    sixel_dither_policy_bluenoise_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_bluenoise_from_base_const(policy);
    *effective = *request;
    effective->method_for_scan = object->method_for_scan;
    effective->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_bluenoise_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_bluenoise_context_t *context,
    unsigned char scratch[SIXEL_MAX_CHANNELS])
{
    sixel_dither_lookup_map_fn lookup_map;
    sixel_dither_t *dither;

    lookup_map = NULL;
    dither = NULL;

    if (request == NULL || context == NULL || request->lookup_policy == NULL
            || request->lookup_policy->vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (request->reqcolor < 1) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: "
            "a bad argument is detected, reqcolor < 0.");
        return SIXEL_BAD_ARGUMENT;
    }

    memset(context, 0, sizeof(*context));
    context->result = request->result;
    context->width = request->width;
    context->height = request->height;
    context->band_origin = request->band_origin;
    context->output_start = request->output_start;
    context->depth = request->depth;
    context->palette = request->palette;
    context->reqcolor = request->reqcolor;
    context->ncolors = request->ncolors;
    context->scratch = scratch;
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;

    lookup_map = request->lookup_policy->vtbl->map_pixel;
    context->lookup_map = lookup_map;
    context->lookup_source_is_float =
        request->lookup_policy->vtbl->lookup_source_is_float(
            request->lookup_policy);
    context->prefer_palette_float_lookup =
        request->lookup_policy->vtbl->prefer_palette_float_lookup(
            request->lookup_policy);

    if (lookup_map == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: lookup policy is not prepared.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        context->pixels_float = (float *)(void *)request->data;
    }

    dither = request->dither;
    if (dither != NULL && dither->palette != NULL) {
        sixel_palette_t *palette_object;
        int float_components;

        palette_object = dither->palette;
        if (palette_object->entries_float32 != NULL
                && palette_object->float_depth > 0) {
            float_components = palette_object->float_depth
                / (int)sizeof(float);
            if (float_components > 0
                    && (size_t)float_components <= SIXEL_MAX_CHANNELS) {
                context->palette_float = palette_object->entries_float32;
                context->float_depth = float_components;
            }
        }
    }

    if (dither != NULL
            && dither->pipeline_transparent_mask != NULL
            && dither->pipeline_transparent_keycolor >= 0
            && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX) {
        context->transparent_mask = dither->pipeline_transparent_mask;
        context->transparent_mask_size = dither->pipeline_transparent_mask_size;
        context->transparent_keycolor = dither->pipeline_transparent_keycolor;
    }

    if (dither != NULL && dither->bluenoise_gradient_map != NULL) {
        context->bluenoise_gradient_map = dither->bluenoise_gradient_map;
        context->bluenoise_gradient_map_size =
            dither->bluenoise_gradient_map_size;
        context->bluenoise_gradient_width = dither->bluenoise_gradient_width;
        context->bluenoise_gradient_height = dither->bluenoise_gradient_height;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_bluenoise_apply(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_bluenoise_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));
    memset(scratch, 0, sizeof(scratch));

    status = sixel_dither_policy_bluenoise_make_effective_request(policy,
                                                           request,
                                                           &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_bluenoise_build_context(&effective,
                                                  &context,
                                                  scratch);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(context.pixelformat)
            && context.pixels_float != NULL
            && effective.dither != NULL
            && effective.dither->prefer_float32 != 0) {
        status = sixel_dither_apply_bluenoise_float32(
            effective.dither,
            &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_bluenoise_8bit(
            effective.dither,
            &context);
        }
    } else {
        status = sixel_dither_apply_bluenoise_8bit(
            effective.dither,
            &context);
    }

    return status;
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_bluenoise_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_bluenoise_vtbl = {
    sixel_dither_policy_bluenoise_ref,
    sixel_dither_policy_bluenoise_unref,
    sixel_dither_policy_bluenoise_prepare,
    sixel_dither_policy_bluenoise_apply,
    sixel_dither_policy_bluenoise_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_create_bluenoise(
    sixel_dither_policy_interface_t **policy)
{
    sixel_dither_policy_bluenoise_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_bluenoise_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_bluenoise_vtbl;
    object->ref = 1U;
    object->method_for_scan = SIXEL_SCAN_AUTO;
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
