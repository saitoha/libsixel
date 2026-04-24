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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_FLOAT_H
# include <float.h>
#endif

#include "compat_stub.h"
#include "lookup-8bit.h"
#include "lookup-common.h"
#include "lookup-float32.h"
#include "lookup-policy-private.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

#define SIXEL_LUT_DENSE_EMPTY (-1)

#define SIXEL_LOOKUP_PACK_LINEAR 0
#define SIXEL_LOOKUP_PACK_MORTON 1
#define SIXEL_LOOKUP_PACK_HILBERT 2

/*
 * IDL (internal contract)
 *
 * class Lookup6Bit : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 */

typedef struct sixel_lookup_policy_bit6_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_lut_t *lut;
    int owns_lut;
    int lookup_source_is_float;
} sixel_lookup_policy_bit6_object_t;

static sixel_lookup_policy_bit6_object_t *
sixel_lookup_policy_bit6_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_bit6_object_t *)(void *)policy;
}

static sixel_lookup_policy_bit6_object_t const *
sixel_lookup_policy_bit6_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_bit6_object_t const *)(void const *)policy;
}

static sixel_lookup_8bit_quantization_t
sixel_lookup_policy_bit6_quant_make(unsigned int depth)
{
    sixel_lookup_8bit_quantization_t quant;
    unsigned int shift;

    shift = 2U;
    if (depth > 3U) {
        shift = 3U;
    }

    quant.channel_shift = shift;
    quant.channel_bits = 8U - shift;
    quant.channel_mask = (1U << quant.channel_bits) - 1U;

    return quant;
}

static int
sixel_lookup_policy_bit6_env_packing(void)
{
    char const *env;

    env = sixel_compat_getenv("SIXEL_LOOKUP_PACKING");
    if (env == NULL || env[0] == '\0') {
        return SIXEL_LOOKUP_PACK_LINEAR;
    }

    if (sixel_compat_strcasecmp(env, "linear") == 0) {
        return SIXEL_LOOKUP_PACK_LINEAR;
    }
    if (sixel_compat_strcasecmp(env, "morton") == 0) {
        return SIXEL_LOOKUP_PACK_MORTON;
    }
    if (sixel_compat_strcasecmp(env, "hilbert") == 0) {
        return SIXEL_LOOKUP_PACK_HILBERT;
    }

    return SIXEL_LOOKUP_PACK_LINEAR;
}

static size_t
sixel_lookup_policy_bit6_dense_size(
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant)
{
    size_t size;
    unsigned int exponent;
    unsigned int i;

    size = 1U;
    exponent = depth * quant->channel_bits;
    for (i = 0U; i < exponent; ++i) {
        if (size > SIZE_MAX / 2U) {
            size = SIZE_MAX;
            break;
        }
        size <<= 1U;
    }

    return size;
}

static unsigned int
sixel_lookup_policy_bit6_pack_linear(
    unsigned char const *pixel,
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant)
{
    unsigned int packed;
    unsigned int bits;
    unsigned int shift;
    unsigned int plane;
    unsigned int component;
    unsigned int rounded;
    unsigned int mask;

    packed = 0U;
    bits = quant->channel_bits;
    shift = quant->channel_shift;
    mask = quant->channel_mask;
    for (plane = 0U; plane < depth; ++plane) {
        component = (unsigned int)pixel[depth - 1U - plane];
        if (shift > 0U) {
            rounded = (component + (1U << (shift - 1U))) >> shift;
            if (rounded > mask) {
                rounded = mask;
            }
        } else {
            rounded = component & mask;
        }
        packed |= rounded << (plane * bits);
    }

    return packed;
}

static unsigned int
sixel_lookup_policy_bit6_pack_morton(
    unsigned char const *pixel,
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant)
{
    unsigned int packed;
    unsigned int bits;
    unsigned int shift;
    unsigned int mask;
    unsigned int component;
    unsigned int rounded;
    unsigned int plane;
    unsigned int bit;
    unsigned int bit_index;
    unsigned int values[4];

    packed = 0U;
    bits = quant->channel_bits;
    shift = quant->channel_shift;
    mask = quant->channel_mask;
    if (depth == 0U) {
        return 0U;
    }
    if (depth > 4U || bits == 0U || bits * depth > 32U) {
        return sixel_lookup_policy_bit6_pack_linear(pixel, depth, quant);
    }

    for (plane = 0U; plane < depth; ++plane) {
        component = (unsigned int)pixel[depth - 1U - plane];
        if (shift > 0U) {
            rounded = (component + (1U << (shift - 1U))) >> shift;
            if (rounded > mask) {
                rounded = mask;
            }
        } else {
            rounded = component & mask;
        }
        values[plane] = rounded;
    }

    for (bit = 0U; bit < bits; ++bit) {
        for (plane = 0U; plane < depth; ++plane) {
            bit_index = bit * depth + plane;
            packed |= ((values[plane] >> bit) & 1U) << bit_index;
        }
    }

    return packed;
}

static unsigned int
sixel_lookup_policy_bit6_pack_hilbert(
    unsigned char const *pixel,
    unsigned int depth,
    sixel_lookup_8bit_quantization_t const *quant)
{
    unsigned int packed;
    unsigned int bits;
    unsigned int shift;
    unsigned int mask;
    unsigned int component;
    unsigned int rounded;
    unsigned int plane;
    unsigned int bit;
    unsigned int bit_index;
    unsigned int q;
    unsigned int p;
    unsigned int t;
    unsigned int coords[3];

    packed = 0U;
    bits = quant->channel_bits;
    shift = quant->channel_shift;
    mask = quant->channel_mask;
    if (depth != 3U || bits == 0U || bits * depth > 30U) {
        return sixel_lookup_policy_bit6_pack_linear(pixel, depth, quant);
    }

    for (plane = 0U; plane < depth; ++plane) {
        component = (unsigned int)pixel[depth - 1U - plane];
        if (shift > 0U) {
            rounded = (component + (1U << (shift - 1U))) >> shift;
            if (rounded > mask) {
                rounded = mask;
            }
        } else {
            rounded = component & mask;
        }
        coords[plane] = rounded;
    }

    q = 1U << (bits - 1U);
    while (q > 1U) {
        p = q - 1U;
        for (plane = 0U; plane < depth; ++plane) {
            if ((coords[plane] & q) != 0U) {
                coords[0] ^= p;
            } else {
                t = (coords[0] ^ coords[plane]) & p;
                coords[0] ^= t;
                coords[plane] ^= t;
            }
        }
        q >>= 1U;
    }

    for (plane = 1U; plane < depth; ++plane) {
        coords[plane] ^= coords[plane - 1U];
    }

    t = 0U;
    q = 1U << (bits - 1U);
    while (q > 1U) {
        if ((coords[depth - 1U] & q) != 0U) {
            t ^= q - 1U;
        }
        q >>= 1U;
    }
    for (plane = 0U; plane < depth; ++plane) {
        coords[plane] ^= t;
    }

    for (bit = 0U; bit < bits; ++bit) {
        for (plane = 0U; plane < depth; ++plane) {
            bit_index = bit * depth + plane;
            packed |= ((coords[plane] >> bit) & 1U) << bit_index;
        }
    }

    return packed;
}

static unsigned int
sixel_lookup_policy_bit6_pack(
    sixel_lookup_8bit_t const *lut,
    unsigned char const *pixel)
{
    if (lut->packing == SIXEL_LOOKUP_PACK_MORTON) {
        return sixel_lookup_policy_bit6_pack_morton(
            pixel,
            (unsigned int)lut->depth,
            &lut->quant);
    }
    if (lut->packing == SIXEL_LOOKUP_PACK_HILBERT) {
        return sixel_lookup_policy_bit6_pack_hilbert(
            pixel,
            (unsigned int)lut->depth,
            &lut->quant);
    }

    return sixel_lookup_policy_bit6_pack_linear(
        pixel,
        (unsigned int)lut->depth,
        &lut->quant);
}

static void
sixel_lookup_policy_bit6_release_cache(sixel_lookup_8bit_t *lut)
{
    if (lut == NULL || lut->dense == NULL) {
        return;
    }

    sixel_allocator_free(lut->allocator, lut->dense);
    lut->dense = NULL;
    lut->dense_size = 0U;
    lut->dense_ready = 0;
}

static SIXELSTATUS
sixel_lookup_policy_bit6_prepare_cache(sixel_lookup_8bit_t *lut)
{
    size_t expected;
    size_t bytes;
    size_t index;

    expected = 0U;
    bytes = 0U;
    index = 0U;
    if (lut == NULL || lut->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    expected = sixel_lookup_policy_bit6_dense_size(
        (unsigned int)lut->depth,
        &lut->quant);
    if (expected == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (expected > SIZE_MAX / sizeof(int32_t)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_6bit: dense cache too large.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (lut->dense != NULL && lut->dense_size != expected) {
        sixel_lookup_policy_bit6_release_cache(lut);
    }
    if (lut->dense == NULL) {
        bytes = expected * sizeof(int32_t);
        lut->dense = (int32_t *)sixel_allocator_malloc(lut->allocator, bytes);
        if (lut->dense == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_6bit: cache allocation failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    for (index = 0U; index < expected; ++index) {
        lut->dense[index] = SIXEL_LUT_DENSE_EMPTY;
    }
    lut->dense_size = expected;
    lut->dense_ready = 1;

    return SIXEL_OK;
}

static float
sixel_lookup_policy_bit6_float_component(float const *palette,
                                         int depth,
                                         int index,
                                         int axis)
{
    int clamped_axis;

    clamped_axis = axis;
    if (clamped_axis < 0) {
        clamped_axis = 0;
    } else if (clamped_axis >= SIXEL_LOOKUP_FLOAT_COMPONENTS) {
        clamped_axis = SIXEL_LOOKUP_FLOAT_COMPONENTS - 1;
    }

    return palette[index * depth + clamped_axis];
}

static float
sixel_lookup_policy_bit6_float_distance(sixel_lookup_float32_t const *lut,
                                        float const *sample,
                                        int palette_index)
{
    float diff;
    float distance;
    int component;

    diff = 0.0f;
    distance = 0.0f;
    component = 0;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        diff = sample[component]
             - sixel_lookup_policy_bit6_float_component(
                   lut->palette,
                   lut->depth,
                   palette_index,
                   component);
        diff *= diff;
        diff *= lut->weights[component];
        distance += diff;
    }

    return distance;
}

static int
sixel_lookup_policy_bit6_float_linear_search(
    sixel_lookup_float32_t const *lut,
    float const *sample)
{
    int index;
    int best_index;
    float distance;
    float best_distance;

    index = 0;
    best_index = 0;
    distance = 0.0f;
    best_distance = FLT_MAX;
    if (lut == NULL || sample == NULL || lut->ncolors <= 0) {
        return 0;
    }

    for (index = 0; index < lut->ncolors; ++index) {
        distance = sixel_lookup_policy_bit6_float_distance(lut, sample, index);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = index;
        }
    }

    return best_index;
}

static SIXELSTATUS
sixel_lookup_policy_bit6_prepare_float_palette(
    sixel_lookup_float32_t *lut,
    unsigned char const *palette,
    float const *palette_float,
    int float_depth,
    int pixelformat)
{
    size_t total;
    size_t float_payload;
    int index;
    int component;
    float *cursor;
    float const *float_cursor;
    int expected_float_depth;

    total = 0U;
    float_payload = 0U;
    index = 0;
    component = 0;
    cursor = NULL;
    float_cursor = NULL;
    expected_float_depth = 0;

    if (lut == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }

    total = (size_t)lut->ncolors * (size_t)lut->depth;
    lut->palette = (float *)sixel_allocator_malloc(lut->allocator,
                                                   total * sizeof(float));
    if (lut->palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_6bit: float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_6bit: float palette depth mismatch.");
            sixel_allocator_free(lut->allocator, lut->palette);
            lut->palette = NULL;
            return SIXEL_BAD_ARGUMENT;
        }
        float_payload = (size_t)lut->ncolors * (size_t)expected_float_depth;
        if (float_payload > 0U) {
            memcpy(cursor, float_cursor, float_payload);
            return SIXEL_OK;
        }
    }

    for (index = 0; index < lut->ncolors; ++index) {
        for (component = 0; component < lut->depth; ++component) {
            *cursor = sixel_pixelformat_byte_to_float(
                pixelformat,
                component,
                palette[index * lut->depth + component]);
            ++cursor;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_bit6_configure_8bit(
    sixel_lookup_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_6BIT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;
    lut->palette = request->palette;
    lut->packing = sixel_lookup_policy_bit6_env_packing();
    lut->quant = sixel_lookup_policy_bit6_quant_make((unsigned int)lut->depth);

    status = sixel_lookup_policy_bit6_prepare_cache(lut);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_bit6_release_cache(lut);
        return status;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_bit6_map_8bit(sixel_lookup_8bit_t *lut,
                                  unsigned char const *pixel)
{
    int result;
    int diff;
    int i;
    int distant;
    unsigned char const *entry;
    unsigned char const *end;
    int pixel0;
    int pixel1;
    int pixel2;
    int delta;
    unsigned int bucket;
    int32_t cached;

    result = 0;
    diff = INT_MAX;
    i = 0;
    distant = 0;
    entry = NULL;
    end = NULL;
    pixel0 = 0;
    pixel1 = 0;
    pixel2 = 0;
    delta = 0;
    bucket = 0U;
    cached = SIXEL_LUT_DENSE_EMPTY;
    if (lut == NULL || pixel == NULL) {
        return 0;
    }

    if (lut->palette == NULL || lut->ncolors <= 0) {
        return 0;
    }

    if (lut->dense_ready != 0 && lut->dense != NULL) {
        bucket = sixel_lookup_policy_bit6_pack(lut, pixel);
        if ((size_t)bucket < lut->dense_size) {
            cached = lut->dense[bucket];
            if (cached >= 0) {
                return cached;
            }
        }
    }

    entry = lut->palette;
    end = lut->palette + (size_t)lut->ncolors * (size_t)lut->depth;
    pixel0 = (int)pixel[0];
    pixel1 = (int)pixel[1];
    pixel2 = (int)pixel[2];
    result = -1;
    for (i = 0; entry < end; ++i, entry += lut->depth) {
        delta = pixel0 - (int)entry[0];
        distant = delta * delta * lut->complexion;
        delta = pixel1 - (int)entry[1];
        distant += delta * delta;
        delta = pixel2 - (int)entry[2];
        distant += delta * delta;
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    if (lut->dense_ready != 0 && lut->dense != NULL && result >= 0
            && sixel_lookup_parallel_dither_active() == 0
            && (size_t)bucket < lut->dense_size) {
        lut->dense[bucket] = result;
    }

    if (result < 0) {
        result = 0;
    }

    return result;
}

static SIXELSTATUS
sixel_lookup_policy_bit6_configure_float32(
    sixel_lookup_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    float range;
    float scale;
    int component;

    status = SIXEL_FALSE;
    base_weights[0] = 0.0f;
    base_weights[1] = 0.0f;
    base_weights[2] = 0.0f;
    range = 1.0f;
    scale = 1.0f;
    component = 0;

    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_6BIT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_policy_bit6_prepare_float_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_bit6_map_float32(sixel_lookup_float32_t const *lut,
                                     unsigned char const *pixel)
{
    float const *sample;

    sample = NULL;
    if (lut == NULL || pixel == NULL || lut->palette == NULL
            || lut->ncolors <= 0) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    return sixel_lookup_policy_bit6_float_linear_search(lut, sample);
}

static void
sixel_lookup_policy_bit6_reset_state(
    sixel_lookup_policy_bit6_object_t *object)
{
    if (object == NULL) {
        return;
    }

    if (object->owns_lut != 0 && object->lut != NULL) {
        sixel_lut_unref(object->lut);
    }

    object->lut = NULL;
    object->owns_lut = 0;
    object->lookup_source_is_float = 0;
}

static void
sixel_lookup_policy_bit6_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_bit6_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_bit6_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_bit6_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_bit6_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_bit6_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_bit6_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_bit6_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_bit6_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_bit6_object_t *reuse_object;
    int normalized_lut_policy;
    int shared_lut;

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;
    shared_lut = 1;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        status = sixel_lookup_policy_validate_complexion_limit(
            request->depth,
            request->complexion);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    object = sixel_lookup_policy_bit6_from_base(policy);
    sixel_lookup_policy_bit6_reset_state(object);

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        SIXEL_LUT_POLICY_6BIT);
    shared_lut = sixel_lookup_policy_shared_cache_enabled(
        normalized_lut_policy);

    reuse_policy = request->reuse_policy;
    if (sixel_lookup_parallel_dither_active() != 0
            /* Reuse slot NULL means shared cache cannot migrate ownership. */
            && shared_lut == 0
            && request->reuse_policy_slot == NULL) {
        reuse_policy = NULL;
    }

    object->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);

    if (reuse_policy != NULL
            && reuse_policy->vtbl == policy->vtbl) {
        reuse_object = sixel_lookup_policy_bit6_from_base(reuse_policy);
        if (reuse_object->lut != NULL) {
            object->lut = reuse_object->lut;
            object->owns_lut = reuse_object->owns_lut;
            object->lookup_source_is_float =
                reuse_object->lookup_source_is_float;
            reuse_object->lut = NULL;
            reuse_object->owns_lut = 0;
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    status = sixel_lut_new(&object->lut,
                           normalized_lut_policy,
                           request->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    object->owns_lut = 1;

    if (object->lookup_source_is_float != 0) {
        status = sixel_lookup_policy_bit6_configure_float32(
            sixel_lut_backend_float32(object->lut),
            request);
    } else {
        status = sixel_lookup_policy_bit6_configure_8bit(
            sixel_lut_backend_8bit(object->lut),
            request);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_bit6_reset_state(object);
        return status;
    }

    if (request->reuse_policy_slot != NULL
            && *request->reuse_policy_slot == NULL) {
        *request->reuse_policy_slot = policy;
        policy->vtbl->ref(policy);
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_bit6_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_bit6_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_bit6_from_base_const(policy);
    if (object->lut == NULL) {
        return 0;
    }

    if (object->lookup_source_is_float != 0) {
        return sixel_lookup_policy_bit6_map_float32(
            sixel_lut_backend_float32(object->lut),
            pixel);
    }

    return sixel_lookup_policy_bit6_map_8bit(
        sixel_lut_backend_8bit(object->lut),
        pixel);
}

static int
sixel_lookup_policy_bit6_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_bit6_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_bit6_from_base_const(policy);
    return object->lookup_source_is_float;
}

static int
sixel_lookup_policy_bit6_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_bit6_vtbl = {
    sixel_lookup_policy_bit6_ref,
    sixel_lookup_policy_bit6_unref,
    sixel_lookup_policy_bit6_prepare,
    sixel_lookup_policy_bit6_map_pixel,
    sixel_lookup_policy_bit6_lookup_source_is_float,
    sixel_lookup_policy_bit6_prefer_palette_float_lookup
};

SIXELSTATUS
sixel_lookup_policy_create_6bit(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_bit6_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_bit6_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_6bit: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_bit6_vtbl;
    object->ref = 1U;

    *policy = &object->base;
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
