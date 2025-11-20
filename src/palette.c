/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Palette orchestration layer shared by every quantizer.  The code below keeps
 * the environment readers, reversible palette helpers, and final merge logic in
 * one place so the algorithm-specific sources only need to request services.
 * The diagram summarises the steady-state flow:
 *
 *   load env -> dispatch -> build clusters -> final merge -> publish entries
 *        ^                 (median-cut / k-means)                 |
 *        '--------------------------------------------------------'
 *
 * The shared helpers are grouped by phase to make the interactions discoverable
 * when adjusting palette-heckbert.c or palette-kmeans.c.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_FLOAT_H
# include <errno.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif

#include "lookup-common.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-heckbert.h"
#include "palette-kmeans.h"
#include "palette.h"
#include "allocator.h"
#include "status.h"
#include "compat_stub.h"

#if HAVE_TESTS
static int g_palette_tests_last_engine_requires_float32 = 0;
static int g_palette_tests_last_engine_model = 0;

void
sixel_palette_tests_reset_last_engine(void)
{
    g_palette_tests_last_engine_requires_float32 = 0;
    g_palette_tests_last_engine_model = 0;
}

int
sixel_palette_tests_last_engine_requires_float32(void)
{
    return g_palette_tests_last_engine_requires_float32;
}

int
sixel_palette_tests_last_engine_model(void)
{
    return g_palette_tests_last_engine_model;
}
#endif

static int palette_default_lut_policy = SIXEL_LUT_POLICY_AUTO;
static int palette_method_for_largest = SIXEL_LARGE_NORM;

/*
 * Quantizer engines wrap each palette solver behind a uniform interface so
 * palette.c can switch between the legacy 8bit implementations and future
 * RGBFLOAT32 variants.  The registry remains private to this translation unit;
 * callers simply ask for a model and optionally request float32 support.
 */
typedef SIXELSTATUS (*sixel_palette_quant_dispatch_fn)(
    sixel_palette_t *palette,
    unsigned char const *data,
    unsigned int length,
    int pixelformat,
    sixel_allocator_t *allocator);

typedef struct sixel_palette_quant_engine {
    char const *name;
    int quantize_model;
    int requires_rgbfloat32;
    sixel_palette_quant_dispatch_fn build_fn;
} sixel_palette_quant_engine_t;

static sixel_palette_quant_engine_t const g_palette_quant_engines[] = {
    {
        "kmeans-rgbfloat32",
        SIXEL_QUANTIZE_MODEL_KMEANS,
        1,
        sixel_palette_build_kmeans_float32,
    },
    {
        "kmeans-legacy",
        SIXEL_QUANTIZE_MODEL_KMEANS,
        0,
        sixel_palette_build_kmeans,
    },
    {
        "heckbert-rgbfloat32",
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        1,
        sixel_palette_build_heckbert_float32,
    },
    {
        "heckbert-legacy",
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        0,
        sixel_palette_build_heckbert,
    },
};

static size_t
sixel_palette_quant_engine_total(void)
{
    size_t total;

    total = sizeof(g_palette_quant_engines)
          / sizeof(g_palette_quant_engines[0]);
    return total;
}

/* Locate a quantizer engine that satisfies the model/format requirements. */
static sixel_palette_quant_engine_t const *
sixel_palette_quant_engine_lookup(int quantize_model,
                                  int needs_rgbfloat32)
{
    sixel_palette_quant_engine_t const *engine;
    size_t index;
    size_t total;

    engine = NULL;
    total = sixel_palette_quant_engine_total();
    for (index = 0U; index < total; ++index) {
        engine = &g_palette_quant_engines[index];
        if (engine->quantize_model != quantize_model) {
            continue;
        }
        if (needs_rgbfloat32 && !engine->requires_rgbfloat32) {
            continue;
        }
        if (!needs_rgbfloat32 && engine->requires_rgbfloat32) {
            continue;
        }
        return engine;
    }

    return NULL;
}

static SIXELSTATUS
sixel_palette_quant_engine_run(sixel_palette_quant_engine_t const *engine,
                               sixel_palette_t *palette,
                               unsigned char const *data,
                               unsigned int length,
                               int pixelformat,
                               sixel_allocator_t *allocator)
{
    if (engine == NULL || engine->build_fn == NULL) {
        return SIXEL_LOGIC_ERROR;
    }

#if HAVE_TESTS
    g_palette_tests_last_engine_requires_float32 =
        engine->requires_rgbfloat32;
    g_palette_tests_last_engine_model = engine->quantize_model;
#endif

    return engine->build_fn(palette,
                            data,
                            length,
                            pixelformat,
                            allocator);
}

/* Try the preferred float32 K-means engine before falling back to legacy. */
static SIXELSTATUS
sixel_palette_apply_kmeans_engines(sixel_palette_t *palette,
                                   unsigned char const *data,
                                   unsigned int length,
                                   int pixelformat,
                                   sixel_allocator_t *allocator,
                                   int prefer_rgbfloat32)
{
    sixel_palette_quant_engine_t const *engine;
    SIXELSTATUS status;
    int saved_lut_policy;

    engine = NULL;
    status = SIXEL_LOGIC_ERROR;
    saved_lut_policy = palette->lut_policy;

    if (prefer_rgbfloat32) {
        engine = sixel_palette_quant_engine_lookup(
            SIXEL_QUANTIZE_MODEL_KMEANS,
            1);
        if (engine != NULL) {
            palette->lut_policy = SIXEL_LUT_POLICY_NONE;
            status = sixel_palette_quant_engine_run(engine,
                                                    palette,
                                                    data,
                                                    length,
                                                    pixelformat,
                                                    allocator);
            if (SIXEL_SUCCEEDED(status)) {
                return status;
            }
            palette->lut_policy = saved_lut_policy;
        }
    }

    engine = sixel_palette_quant_engine_lookup(
        SIXEL_QUANTIZE_MODEL_KMEANS,
        0);
    if (engine != NULL) {
        status = sixel_palette_quant_engine_run(engine,
                                                palette,
                                                data,
                                                length,
                                                pixelformat,
                                                allocator);
    }

    return status;
}

/* Thin wrapper that executes the median-cut/Heckbert engine. */
static SIXELSTATUS
sixel_palette_apply_mediancut_engine(sixel_palette_t *palette,
                                     unsigned char const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     int prefer_rgbfloat32)
{
    sixel_palette_quant_engine_t const *engine;
    SIXELSTATUS status;

    status = SIXEL_LOGIC_ERROR;
    if (prefer_rgbfloat32 && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        engine = sixel_palette_quant_engine_lookup(
            SIXEL_QUANTIZE_MODEL_MEDIANCUT,
            1);
        if (engine != NULL) {
            status = sixel_palette_quant_engine_run(engine,
                                                    palette,
                                                    data,
                                                    length,
                                                    pixelformat,
                                                    allocator);
            if (SIXEL_SUCCEEDED(status)) {
                return status;
            }
        }
    }

    engine = sixel_palette_quant_engine_lookup(
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        0);
    if (engine == NULL) {
        return SIXEL_LOGIC_ERROR;
    }

    return sixel_palette_quant_engine_run(engine,
                                          palette,
                                          data,
                                          length,
                                          pixelformat,
                                          allocator);
}

/* Palette orchestration delegates algorithm specifics to dedicated modules. */

void
sixel_palette_set_lut_policy(int lut_policy)
{
    int normalized;

    normalized = SIXEL_LUT_POLICY_AUTO;
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
        || lut_policy == SIXEL_LUT_POLICY_6BIT
        || lut_policy == SIXEL_LUT_POLICY_CERTLUT
        || lut_policy == SIXEL_LUT_POLICY_NONE) {
        normalized = lut_policy;
    }

    palette_default_lut_policy = normalized;
}

void
sixel_palette_set_method_for_largest(int method)
{
    int normalized;

    normalized = SIXEL_LARGE_NORM;
    if (method == SIXEL_LARGE_NORM || method == SIXEL_LARGE_LUM) {
        normalized = method;
    } else if (method == SIXEL_LARGE_AUTO) {
        normalized = SIXEL_LARGE_NORM;
    }

    palette_method_for_largest = normalized;
}

/*
 * Resize the palette entry buffer.
 *
 * The helper keeps the allocation logic in a single place so both k-means and
 * median-cut paths can rely on the same growth strategy.  When the caller
 * requests a size of zero the buffer is released entirely.
 */
static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator);

static SIXELSTATUS
sixel_palette_resize_entries_float32(sixel_palette_t *palette,
                                     unsigned int colors,
                                     unsigned int depth,
                                     sixel_allocator_t *allocator);

SIXELAPI SIXELSTATUS
sixel_palette_new(sixel_palette_t **palette, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *object;

    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_new: palette pointer is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            *palette = NULL;
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    object = (sixel_palette_t *)sixel_allocator_malloc(
        allocator, sizeof(*object));
    if (object == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_palette_new: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        *palette = NULL;
        goto end;
    }

    object->ref = 1U;
    object->allocator = allocator;
    object->entries = NULL;
    object->entries_size = 0U;
    object->entries_float32 = NULL;
    object->entries_float32_size = 0U;
    object->entry_count = 0U;
    object->requested_colors = 0U;
    object->original_colors = 0U;
    object->depth = 0;
    object->float_depth = 0;
    object->method_for_largest = SIXEL_LARGE_AUTO;
    object->method_for_rep = SIXEL_REP_AUTO;
    object->quality_mode = SIXEL_QUALITY_AUTO;
    object->force_palette = 0;
    object->use_reversible = 0;
    object->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    object->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    object->complexion = 1;
    object->lut_policy = SIXEL_LUT_POLICY_AUTO;
    object->sixel_reversible = 0;
    object->final_merge = 0;
    object->lut = NULL;

    *palette = object;
    status = SIXEL_OK;

end:
    return status;
}

SIXELAPI sixel_palette_t *
sixel_palette_ref(sixel_palette_t *palette)
{
    if (palette != NULL) {
        ++palette->ref;
    }

    return palette;
}

/* Count unique RGB triples until the limit is exceeded. */
static void
sixel_palette_dispose(sixel_palette_t *palette)
{
    sixel_allocator_t *allocator;

    if (palette == NULL) {
        return;
    }

    allocator = palette->allocator;
    if (palette->entries != NULL) {
        sixel_allocator_free(allocator, palette->entries);
        palette->entries = NULL;
    }
    if (palette->entries_float32 != NULL) {
        sixel_allocator_free(allocator, palette->entries_float32);
        palette->entries_float32 = NULL;
    }

    if (palette->lut != NULL) {
        sixel_lut_unref(palette->lut);
        palette->lut = NULL;
    }

    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI void
sixel_palette_unref(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return;
    }

    if (palette->ref > 1U) {
        --palette->ref;
        return;
    }

    sixel_palette_dispose(palette);
    sixel_allocator_free(palette->allocator, palette);
}

static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t required;
    unsigned char *resized;

    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    required = (size_t)colors * (size_t)depth;
    if (required == 0U) {
        if (palette->entries != NULL) {
            sixel_allocator_free(allocator, palette->entries);
            palette->entries = NULL;
        }
        palette->entries_size = 0U;
        return SIXEL_OK;
    }

    if (palette->entries != NULL && palette->entries_size >= required) {
        return SIXEL_OK;
    }

    if (palette->entries == NULL) {
        resized = (unsigned char *)sixel_allocator_malloc(allocator, required);
    } else {
        resized = (unsigned char *)sixel_allocator_realloc(allocator,
                                                           palette->entries,
                                                           required);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    palette->entries = resized;
    palette->entries_size = required;

    status = SIXEL_OK;

    return status;
}

static SIXELSTATUS
sixel_palette_resize_entries_float32(sixel_palette_t *palette,
                                     unsigned int colors,
                                     unsigned int depth,
                                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t required_bytes;
    float *resized;

    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    required_bytes = (size_t)colors * (size_t)depth;
    if (required_bytes == 0U) {
        if (palette->entries_float32 != NULL) {
            sixel_allocator_free(allocator, palette->entries_float32);
            palette->entries_float32 = NULL;
        }
        palette->entries_float32_size = 0U;
        return SIXEL_OK;
    }

    if (palette->entries_float32 != NULL
            && palette->entries_float32_size >= required_bytes) {
        return SIXEL_OK;
    }

    if (palette->entries_float32 == NULL) {
        resized = (float *)sixel_allocator_malloc(allocator,
                                                  required_bytes);
    } else {
        resized = (float *)sixel_allocator_realloc(allocator,
                                                   palette->entries_float32,
                                                   required_bytes);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries_float32: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    palette->entries_float32 = resized;
    palette->entries_float32_size = required_bytes;

    status = SIXEL_OK;

    return status;
}

SIXELAPI SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize_entries(palette,
                                          colors,
                                          (unsigned int)depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette->entry_count = colors;
    palette->depth = depth;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize(palette, colors, depth, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    payload_size = (size_t)colors * (size_t)depth;
    if (entries != NULL && palette->entries != NULL && payload_size > 0U) {
        memcpy(palette->entries, entries, payload_size);
    }

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_palette_set_entries_float32(sixel_palette_t *palette,
                                  float const *entries,
                                  unsigned int colors,
                                  int depth,
                                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (colors == 0U || depth == 0 || entries == NULL) {
        if (palette->entries_float32 != NULL) {
            sixel_allocator_free(work_allocator,
                                 palette->entries_float32);
            palette->entries_float32 = NULL;
        }
        palette->entries_float32_size = 0U;
        palette->float_depth = 0;
        return SIXEL_OK;
    }

    status = sixel_palette_resize_entries_float32(palette,
                                                  colors,
                                                  (unsigned int)depth,
                                                  work_allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    payload_size = (size_t)colors * (size_t)depth;
    if (payload_size > 0U && palette->entries_float32 != NULL) {
        memcpy(palette->entries_float32, entries, payload_size);
    }
    palette->entries_float32_size = payload_size;
    palette->float_depth = depth;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_palette_copy_entries_8bit(sixel_palette_t *palette,
                                unsigned char **ppentries,
                                size_t *count,
                                int pixelformat,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;
    unsigned char *copy;
    size_t payload_size;
    size_t entry_count;
    int depth;

    if (palette == NULL || ppentries == NULL || count == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppentries = NULL;
    *count = 0U;

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_copy_entries_8bit: invalid pixelformat.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        sixel_helper_set_additional_message(
            "sixel_palette_copy_entries_8bit: only RGB888 is supported.");
        return SIXEL_FEATURE_ERROR;
    }

    if (palette->entries == NULL || palette->entry_count == 0U) {
        return SIXEL_OK;
    }

    entry_count = palette->entry_count;
    payload_size = entry_count * (size_t)depth;
    if (palette->depth != depth || palette->entries_size < payload_size) {
        sixel_helper_set_additional_message(
            "sixel_palette_copy_entries_8bit: palette depth mismatch.");
        return SIXEL_LOGIC_ERROR;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    copy = NULL;
    if (payload_size > 0U) {
        copy = (unsigned char *)sixel_allocator_malloc(work_allocator,
                                                       payload_size);
        if (copy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_palette_copy_entries_8bit: malloc failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(copy, palette->entries, payload_size);
    }

    *ppentries = copy;
    *count = entry_count;
    status = SIXEL_OK;

    return status;
}

SIXELAPI SIXELSTATUS
sixel_palette_copy_entries_float32(sixel_palette_t *palette,
                                   float **ppentries,
                                   size_t *count,
                                   int pixelformat,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;
    float *copy;
    size_t payload_size;
    size_t entry_count;
    int depth;

    if (palette == NULL || ppentries == NULL || count == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppentries = NULL;
    *count = 0U;

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_copy_entries_float32: invalid pixelformat.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_helper_set_additional_message(
            "sixel_palette_copy_entries_float32: only float32 layouts are supported.");
        return SIXEL_FEATURE_ERROR;
    }

    if (palette->entries_float32 == NULL || palette->entry_count == 0U) {
        return SIXEL_OK;
    }

    entry_count = palette->entry_count;
    payload_size = entry_count * (size_t)depth;
    if (palette->float_depth != depth
            || palette->entries_float32_size < payload_size) {
        sixel_helper_set_additional_message(
            "sixel_palette_copy_entries_float32: depth mismatch.");
        return SIXEL_LOGIC_ERROR;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    copy = NULL;
    if (payload_size > 0U) {
        copy = (float *)sixel_allocator_malloc(work_allocator,
                                               payload_size);
        if (copy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_palette_copy_entries_float32: malloc failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(copy, palette->entries_float32, payload_size);
    }

    *ppentries = copy;
    *count = entry_count;
    status = SIXEL_OK;

    return status;
}

/*
 * sixel_palette_generate builds the palette entries inside the provided
 * sixel_palette_t instance.  The dispatcher consults the quantizer engine
 * registry to pick either the legacy 8bit solvers or (in later phases) the
 * RGBFLOAT32-enabled variants.  The high-level flow is:
 *
 *   1. Honour explicit K-means requests by running the preferred engine
 *      (float32 when available, otherwise the current 8bit solver).  Failed
 *      runs fall back to Heckbert to preserve historical behaviour.
 *   2. Invoke the median-cut/Heckbert engine for every other case, including
 *      the AUTO mode.
 *
 * Both branches share helper routines for cache management and post-processing
 * (for example reversible palette transformation).  The palette object tracks
 * the generated metadata so the caller can publish it without recomputing.
 */
SIXELSTATUS
sixel_palette_generate(sixel_palette_t *palette,
                       unsigned char const *data,
                       unsigned int length,
                       int pixelformat,
                       int prefer_rgbfloat32,
                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    sixel_allocator_t *work_allocator;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result_depth = sixel_helper_compute_depth(pixelformat);
    if (result_depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = (unsigned int)result_depth;

    status = SIXEL_FALSE;

    if (palette->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = sixel_palette_apply_kmeans_engines(palette,
                                                    data,
                                                    length,
                                                    pixelformat,
                                                    work_allocator,
                                                    prefer_rgbfloat32);
        if (SIXEL_SUCCEEDED(status)) {
            ncolors = palette->entry_count;
            origcolors = palette->original_colors;
            depth = (unsigned int)palette->depth;
            goto success;
        }
    } else if (palette->quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        status = sixel_palette_apply_mediancut_engine(palette,
                                                      data,
                                                      length,
                                                      pixelformat,
                                                      work_allocator,
                                                      prefer_rgbfloat32);
        goto after_quantizer;
    }

    status = sixel_palette_apply_mediancut_engine(palette,
                                                  data,
                                                  length,
                                                  pixelformat,
                                                  work_allocator,
                                                  prefer_rgbfloat32);

after_quantizer:
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = palette->entry_count;
    origcolors = palette->original_colors;
    depth = (unsigned int)palette->depth;
    if (palette->use_reversible && palette->entries != NULL) {
        sixel_palette_reversible_palette(palette->entries,
                                         ncolors,
                                         depth);
    }
    status = SIXEL_OK;

success:
    palette->entry_count = ncolors;
    palette->original_colors = origcolors;
    palette->depth = (int)depth;

end:
    return status;
}

SIXELSTATUS
sixel_palette_make_palette(unsigned char **result,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           unsigned int reqcolors,
                           unsigned int *ncolors,
                           unsigned int *origcolors,
                           int methodForLargest,
                           int methodForRep,
                           int qualityMode,
                           int force_palette,
                           int use_reversible,
                           int quantize_model,
                           int final_merge_mode,
                           int prefer_rgbfloat32,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *palette = NULL;
    sixel_allocator_t *work_allocator;
    size_t payload_size;
    unsigned int depth;

    if (result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;

    status = sixel_palette_new(&palette, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (methodForLargest == SIXEL_LARGE_AUTO) {
        methodForLargest = palette_method_for_largest;
    }

    palette->requested_colors = reqcolors;
    palette->method_for_largest = methodForLargest;
    palette->method_for_rep = methodForRep;
    palette->quality_mode = qualityMode;
    palette->force_palette = force_palette;
    palette->use_reversible = use_reversible;
    palette->quantize_model = quantize_model;
    palette->final_merge_mode = final_merge_mode;
    palette->lut_policy = palette_default_lut_policy;

    status = sixel_palette_generate(palette,
                                    data,
                                    length,
                                    pixelformat,
                                    prefer_rgbfloat32,
                                    allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (ncolors != NULL) {
        *ncolors = palette->entry_count;
    }
    if (origcolors != NULL) {
        *origcolors = palette->original_colors;
    }

    if (palette->depth <= 0 || palette->entry_count == 0U) {
        status = SIXEL_OK;
        goto end;
    }

    depth = (unsigned int)palette->depth;
    payload_size = (size_t)palette->entry_count * (size_t)depth;
    work_allocator = (allocator != NULL) ? allocator : palette->allocator;
    if (work_allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    *result = (unsigned char *)sixel_allocator_malloc(work_allocator,
                                                      payload_size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_make_palette: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(*result, palette->entries, payload_size);

    status = SIXEL_OK;

end:
    if (palette != NULL) {
        sixel_palette_unref(palette);
    }
    return status;
}

void
sixel_palette_free_palette(unsigned char *data,
                           sixel_allocator_t *allocator)
{
    sixel_allocator_free(allocator, data);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
