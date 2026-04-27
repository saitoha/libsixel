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

#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "dither-policy-x-dither.h"
#include "dither.h"
#include "dither-common-pipeline.h"
#include "dither-internal.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * Private dither context for this policy implementation.
 * Keep only members used by this translation unit.
 */
typedef struct sixel_dither_policy_x_dither_context {
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
    int optimize_palette;
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
} sixel_dither_policy_x_dither_context_t;

/*
 * Parse a float environment override. Invalid text is rejected so callers can
 * keep their default values.
 */
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

/*
 * Resolve the effective A-dither strength from environment.
 */
static float
sixel_dither_get_x_strength(float default_strength)
{
    char const *text;
    float value;

    text = NULL;
    value = default_strength;

    text = sixel_compat_getenv("SIXEL_DITHER_X_DITHER_STRENGTH");
    if (text != NULL
            && sixel_dither_parse_float_env(text, &value) == 0) {
        value = default_strength;
    }

    return value;
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

static float
sixel_dither_x_noise(int x, int y, int c, float strength)
{
    return (((((x + c * 29) ^ (y * 149)) * 1234) & 511) / 256.0f
            - 1.0f) * strength;
}

/*
 * Return non-zero when the current pixel must be forced to the transparent
 * keycolor.
 */
static int
sixel_dither_is_transparent_pixel(sixel_dither_policy_x_dither_context_t const *context,
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
sixel_dither_apply_x_dither_8bit(sixel_dither_t *dither,
                                 sixel_dither_policy_x_dither_context_t *context)
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
    float strength;
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
    strength = 0.150f;
    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
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
            || context->migration_map == NULL
            || context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    strength = sixel_dither_get_x_strength(0.100f);

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

    if (context->optimize_palette != 0) {
        *context->ncolors = 0;
        memset(context->new_palette,
               0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);
        memset(context->migration_map,
               0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float,
                   0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth
                       * sizeof(float));
        }
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

            for (d = 0; d < context->depth; ++d) {
                val = context->pixels[pos * context->depth + d]
                    + (int)(sixel_dither_x_noise(x, y, d, strength) * 32.0f);
                context->scratch[d] = (unsigned char)(
                    val < 0 ? 0 : val > 255 ? 255 : val);
            }

            color_index = context->lookup_map(context->lookup_policy,
                                              context->scratch);

            if (context->optimize_palette != 0) {
                if (context->migration_map[color_index] == 0) {
                    if (absolute_y >= context->output_start) {
                        context->result[pos] =
                            (sixel_index_t)(*context->ncolors);
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
                                               * float_depth + float_index]
                                = palette_float[color_index
                                                * float_depth + float_index];
                        }
                    }

                    ++*context->ncolors;
                    context->migration_map[color_index]
                        = (unsigned short)(*context->ncolors);
                } else if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)(
                        context->migration_map[color_index] - 1);
                }
            } else if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)color_index;
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    if (context->optimize_palette != 0) {
        memcpy(context->palette,
               context->new_palette,
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
        *context->ncolors = context->reqcolor;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_apply_x_dither_float32(sixel_dither_t *dither,
                                    sixel_dither_policy_x_dither_context_t *context)
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
    float strength;
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
    strength = 0.150f;
    jitter_scale = 32.0f / 255.0f;
    noise = 0.0f;
    val = 0.0f;
    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
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
            || context->migration_map == NULL
            || context->ncolors == NULL
            || context->depth <= 0
            || context->depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }

    strength = sixel_dither_get_x_strength(0.100f);

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

    if (context->optimize_palette != 0) {
        *context->ncolors = 0;
        memset(context->new_palette,
               0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);
        memset(context->migration_map,
               0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float,
                   0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth
                       * sizeof(float));
        }
    }

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

            for (d = 0; d < context->depth; ++d) {
                noise = sixel_dither_x_noise(x, y, d, strength);
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

            if (context->optimize_palette != 0) {
                if (context->migration_map[color_index] == 0) {
                    if (absolute_y >= context->output_start) {
                        context->result[pos] =
                            (sixel_index_t)(*context->ncolors);
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
                                               * float_depth + float_index]
                                = palette_float[color_index
                                                * float_depth + float_index];
                        }
                    }

                    ++*context->ncolors;
                    context->migration_map[color_index]
                        = (unsigned short)(*context->ncolors);
                } else if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)(
                        context->migration_map[color_index] - 1);
                }
            } else if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)color_index;
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    if (context->optimize_palette != 0) {
        memcpy(context->palette,
               context->new_palette,
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
        *context->ncolors = context->reqcolor;
    }

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
typedef struct sixel_dither_policy_x_dither_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_x_dither_object_t;

static sixel_dither_policy_x_dither_object_t *
sixel_dither_policy_x_dither_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_x_dither_object_t *)(void *)policy;
}

static sixel_dither_policy_x_dither_object_t const *
sixel_dither_policy_x_dither_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_x_dither_object_t const *)(void const *)policy;
}

static void
sixel_dither_policy_x_dither_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_x_dither_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_x_dither_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_x_dither_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_x_dither_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_x_dither_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_dither_policy_x_dither_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_x_dither_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_x_dither_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_x_dither_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective)
{
    sixel_dither_policy_x_dither_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_x_dither_from_base_const(policy);
    *effective = *request;
    effective->method_for_scan = object->method_for_scan;
    effective->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_x_dither_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_x_dither_context_t *context,
    unsigned char scratch[SIXEL_MAX_CHANNELS],
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4],
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS],
    unsigned short migration_map[SIXEL_PALETTE_MAX])
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
    context->new_palette = new_palette;
    context->migration_map = migration_map;
    context->ncolors = request->ncolors;
    context->scratch = scratch;
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;
    context->optimize_palette = request->foptimize_palette;

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
                context->new_palette_float = new_palette_float;
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


    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_x_dither_apply(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_x_dither_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned short migration_map[SIXEL_PALETTE_MAX];

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));
    memset(scratch, 0, sizeof(scratch));
    memset(new_palette, 0, sizeof(new_palette));
    memset(new_palette_float, 0, sizeof(new_palette_float));
    memset(migration_map, 0, sizeof(migration_map));

    status = sixel_dither_policy_x_dither_make_effective_request(policy,
                                                           request,
                                                           &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_x_dither_build_context(&effective,
                                                  &context,
                                                  scratch,
                                                  new_palette,
                                                  new_palette_float,
                                                  migration_map);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(context.pixelformat)
            && context.pixels_float != NULL
            && effective.dither != NULL
            && effective.dither->prefer_float32 != 0) {
        status = sixel_dither_apply_x_dither_float32(
            effective.dither,
            &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_x_dither_8bit(
            effective.dither,
            &context);
        }
    } else {
        status = sixel_dither_apply_x_dither_8bit(
            effective.dither,
            &context);
    }

    return status;
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_x_dither_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_x_dither_vtbl = {
    sixel_dither_policy_x_dither_ref,
    sixel_dither_policy_x_dither_unref,
    sixel_dither_policy_x_dither_prepare,
    sixel_dither_policy_x_dither_apply,
    sixel_dither_policy_x_dither_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_create_x_dither(
    sixel_dither_policy_interface_t **policy)
{
    sixel_dither_policy_x_dither_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_x_dither_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_x_dither_vtbl;
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
