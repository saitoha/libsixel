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
    int method_for_scan;
    struct sixel_lookup_policy_interface *lookup_policy;
    sixel_dither_lookup_map_fn lookup_map;
    int pixelformat;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
} sixel_dither_policy_x_dither_context_t;

/*
 * Parse a float environment override. Invalid text is rejected so callers can
 * keep their default values.
 */
static int
sixel_dither_x_parse_float_env(char const *text, float *out_value)
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
            && sixel_dither_x_parse_float_env(text, &value) == 0) {
        value = default_strength;
    }

    return value;
}

static void
sixel_dither_x_scanline_params_8bit(int serpentine,
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
sixel_dither_x_scanline_params_float32(int serpentine,
                                     int index,
                                     int limit,
                                     int *start,
                                     int *end,
                                     int *step,
                                     int *direction)
{
    sixel_dither_x_scanline_params_8bit(serpentine,
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
sixel_dither_x_is_transparent_pixel(sixel_dither_policy_x_dither_context_t const *context,
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
                                 sixel_dither_policy_x_dither_context_t *context
                                 )
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
    float strength;
    unsigned char quantized[SIXEL_MAX_CHANNELS];
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
    strength = 0.150f;
    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = -1;
    use_transparent_fence = 0;
    is_transparent = 0;

    if (dither == NULL || context == NULL
            || context->pixels == NULL
            || context->result == NULL
            || context->lookup_policy == NULL
            || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    strength = sixel_dither_get_x_strength(0.100f);

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);

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
        sixel_dither_x_scanline_params_8bit(serpentine,
                                          absolute_y,
                                          context->width,
                                          &start,
                                          &end,
                                          &step,
                                          &direction);
        (void)direction;

        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            is_transparent = sixel_dither_x_is_transparent_pixel(
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
                quantized[d] = (unsigned char)(
                    val < 0 ? 0 : val > 255 ? 255 : val);
            }

            color_index = context->lookup_map(context->lookup_policy,
                                              quantized);

            if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)color_index;
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_apply_x_dither_float32(sixel_dither_t *dither,
                                    sixel_dither_policy_x_dither_context_t *context
                                    )
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
    float strength;
    float jitter_scale;
    float noise;
    float val;
    float lookup_pixel_float[SIXEL_MAX_CHANNELS];
    unsigned char const *lookup_pixel;
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
    strength = 0.150f;
    jitter_scale = 32.0f / 255.0f;
    noise = 0.0f;
    val = 0.0f;
    memset(lookup_pixel_float, 0, sizeof(lookup_pixel_float));
    lookup_pixel = NULL;
    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = -1;
    use_transparent_fence = 0;
    is_transparent = 0;

    if (dither == NULL || context == NULL
            || context->pixels_float == NULL
            || context->result == NULL
            || context->lookup_policy == NULL
            || context->lookup_map == NULL
                        || context->depth <= 0
            || context->depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }

    strength = sixel_dither_get_x_strength(0.100f);

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);

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
        sixel_dither_x_scanline_params_float32(serpentine,
                                             absolute_y,
                                             context->width,
                                             &start,
                                             &end,
                                             &step,
                                             &direction);
        (void)direction;

        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            is_transparent = sixel_dither_x_is_transparent_pixel(
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
                lookup_pixel_float[d] = val;
            }

            lookup_pixel = (unsigned char const *)(void const *)
                lookup_pixel_float;
            color_index = context->lookup_map(context->lookup_policy,
                                              lookup_pixel);

            if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)color_index;
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
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
    sixel_allocator_t *allocator;
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
    sixel_dither_policy_x_dither_context_t *context)
{
    sixel_dither_lookup_map_fn lookup_map;
    sixel_dither_t *dither;

    lookup_map = NULL;
    dither = NULL;

    if (request == NULL || context == NULL || request->lookup_policy == NULL
            || request->lookup_policy->vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(context, 0, sizeof(*context));
    context->result = request->result;
    context->width = request->width;
    context->height = request->height;
    context->band_origin = request->band_origin;
    context->output_start = request->output_start;
    context->depth = request->depth;
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;

    lookup_map = request->lookup_policy->vtbl->map_pixel;
    context->lookup_map = lookup_map;

    if (lookup_map == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: lookup policy is not prepared.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        context->pixels_float = (float *)(void *)request->data;
    }

    dither = request->dither;
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
sixel_dither_policy_x_dither_apply_8bit(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_x_dither_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_x_dither_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_x_dither_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_apply_x_dither_8bit(
        effective.dither,
        &context);
}

static SIXELSTATUS
sixel_dither_policy_x_dither_apply_float32(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_x_dither_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_x_dither_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_x_dither_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_apply_x_dither_float32(
        effective.dither,
        &context);
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
    sixel_dither_policy_x_dither_apply_8bit,
    sixel_dither_policy_x_dither_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_x_dither_new(
    sixel_allocator_t *allocator,
    sixel_dither_policy_interface_t **policy)
{
    sixel_dither_policy_x_dither_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_x_dither_object_t *))sixel_allocator_malloc(allocator, sizeof(*object));
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

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_x_dither_8bit_vtbl = {
    sixel_dither_policy_x_dither_ref,
    sixel_dither_policy_x_dither_unref,
    sixel_dither_policy_x_dither_prepare,
    sixel_dither_policy_x_dither_apply_8bit,
    sixel_dither_policy_x_dither_supports_parallel_bands
};

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_x_dither_float32_vtbl = {
    sixel_dither_policy_x_dither_ref,
    sixel_dither_policy_x_dither_unref,
    sixel_dither_policy_x_dither_prepare,
    sixel_dither_policy_x_dither_apply_float32,
    sixel_dither_policy_x_dither_supports_parallel_bands
};

SIXELSTATUS
sixel_dither_policy_x_dither_8bit_new(
    sixel_allocator_t *allocator,
    sixel_dither_policy_interface_t **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_x_dither_new(allocator, policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        (*policy)->vtbl = &g_sixel_dither_policy_x_dither_8bit_vtbl;
    }

    return status;
}

SIXELSTATUS
sixel_dither_policy_x_dither_float32_new(
    sixel_allocator_t *allocator,
    sixel_dither_policy_interface_t **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_x_dither_new(allocator, policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        (*policy)->vtbl = &g_sixel_dither_policy_x_dither_float32_vtbl;
    }

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
