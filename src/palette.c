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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <math.h>
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
#if HAVE_STDIO_H
# include <stdio.h>
#endif

#include "palette-common-cover.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-heckbert.h"
#include "palette-kcenter.h"
#include "palette-kmeans.h"
#include "palette-kmedoids.h"
#include "palette-private.h"
#include "allocator.h"
#include "options.h"
#include "status.h"
#include "compat_stub.h"
#include "timeline-logger.h"
#include "cms.h"
#include "colorspace.h"
#include "pixelformat.h"

/*
 * IDL usage in this unit
 *
 * IPalette.ref()
 * IPalette.unref()
 * IPalette.init_entries(request)
 * IPalette.init_entries_float32(request)
 * IPalette.generate(request)
 * IPalette.get_entries(view)
 * IPalette.get_entries_float32(view)
 * IPalette.get_metadata(metadata)
 */

static void
sixel_palette_vtbl_ref(sixel_palette_t *palette);
static void
sixel_palette_vtbl_unref(sixel_palette_t *palette);
static SIXELSTATUS
sixel_palette_vtbl_init_entries(
    sixel_palette_t *palette,
    sixel_palette_entries_request_t const *request);
static SIXELSTATUS
sixel_palette_vtbl_init_entries_float32(
    sixel_palette_t *palette,
    sixel_palette_float32_entries_request_t const *request);
static SIXELSTATUS
sixel_palette_vtbl_generate(
    sixel_palette_t *palette,
    sixel_palette_generate_request_t const *request);
static SIXELSTATUS
sixel_palette_vtbl_get_entries(
    sixel_palette_t const *palette,
    sixel_palette_entries_view_t *view);
static SIXELSTATUS
sixel_palette_vtbl_get_entries_float32(
    sixel_palette_t const *palette,
    sixel_palette_float32_entries_view_t *view);
static SIXELSTATUS
sixel_palette_vtbl_get_metadata(
    sixel_palette_t const *palette,
    sixel_palette_metadata_t *metadata);

static sixel_palette_vtbl_t const g_sixel_palette_vtbl = {
    sixel_palette_vtbl_ref,
    sixel_palette_vtbl_unref,
    sixel_palette_vtbl_init_entries,
    sixel_palette_vtbl_init_entries_float32,
    sixel_palette_vtbl_generate,
    sixel_palette_vtbl_get_entries,
    sixel_palette_vtbl_get_entries_float32,
    sixel_palette_vtbl_get_metadata
};

static sixel_atomic_u32_t g_sixel_palette_timeline_job_seq;

SIXEL_INTERNAL_API int
sixel_palette_timeline_next_job_id(void)
{
    unsigned int job_id;

    /*
     * Palette timeline rows share one process-wide id space.  Top-level build
     * spans and nested quantizer phases both allocate from this counter so
     * concurrent palette runs cannot reuse rows.
     */
    job_id = sixel_atomic_fetch_add_u32(&g_sixel_palette_timeline_job_seq, 1U);
    if (job_id > (unsigned int)INT_MAX) {
        return -1;
    }

    return (int)job_id;
}

/*
 * Quantizer engines wrap each palette solver behind a uniform interface so
 * palette.c can switch between the legacy 8bit implementations and future
 * RGBFLOAT32 variants.  The registry remains private to this translation unit;
 * callers simply ask for a model and optionally request float32 support.
 */
typedef SIXELSTATUS (*sixel_palette_quant_dispatch_fn)(
    sixel_palette_t *palette,
    void const *data,
    unsigned int length,
    int pixelformat,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger,
    int *job_seq,
    char const *engine_name,
    sixel_palette_telemetry_t *telemetry);

/*
 * Dispatch adapters keep the function pointer signatures uniform and avoid
 * undefined behavior from casting between incompatible function types.
 */
static SIXELSTATUS
sixel_palette_build_kmeans_dispatch(sixel_palette_t *palette,
                                    void const *data,
                                    unsigned int length,
                                    int pixelformat,
                                    sixel_allocator_t *allocator,
                                    sixel_timeline_logger_t *logger,
                                    int *job_seq,
                                    char const *engine_name,
                                    sixel_palette_telemetry_t *telemetry)
{
    unsigned char const *bytes;

    bytes = (unsigned char const *)data;
    return sixel_palette_build_kmeans(palette,
                                      bytes,
                                      length,
                                      pixelformat,
                                      allocator,
                                      logger,
                                      job_seq,
                                      engine_name,
                                      telemetry);
}

static SIXELSTATUS
sixel_palette_build_kmeans_float32_dispatch(
    sixel_palette_t *palette,
    void const *data,
    unsigned int length,
    int pixelformat,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger,
    int *job_seq,
    char const *engine_name,
    sixel_palette_telemetry_t *telemetry)
{
    float const *samples;

    samples = (float const *)data;
    return sixel_palette_build_kmeans_float32(palette,
                                              samples,
                                              length,
                                              pixelformat,
                                              allocator,
                                              logger,
                                              job_seq,
                                              engine_name,
                                              telemetry);
}

static SIXELSTATUS
sixel_palette_build_kcenter_dispatch(sixel_palette_t *palette,
                                     void const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     sixel_timeline_logger_t *logger,
                                     int *job_seq,
                                     char const *engine_name,
                                     sixel_palette_telemetry_t *telemetry)
{
    unsigned char const *bytes;

    bytes = (unsigned char const *)data;
    return sixel_palette_build_kcenter(palette,
                                       bytes,
                                       length,
                                       pixelformat,
                                       allocator,
                                       logger,
                                       job_seq,
                                       engine_name,
                                       telemetry);
}

static SIXELSTATUS
sixel_palette_build_kcenter_float32_dispatch(
    sixel_palette_t *palette,
    void const *data,
    unsigned int length,
    int pixelformat,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger,
    int *job_seq,
    char const *engine_name,
    sixel_palette_telemetry_t *telemetry)
{
    float const *samples;

    samples = (float const *)data;
    return sixel_palette_build_kcenter_float32(palette,
                                               samples,
                                               length,
                                               pixelformat,
                                               allocator,
                                               logger,
                                               job_seq,
                                               engine_name,
                                               telemetry);
}

static SIXELSTATUS
sixel_palette_build_kmedoids_dispatch(sixel_palette_t *palette,
                                      void const *data,
                                      unsigned int length,
                                      int pixelformat,
                                      sixel_allocator_t *allocator,
                                      sixel_timeline_logger_t *logger,
                                      int *job_seq,
                                      char const *engine_name,
                                      sixel_palette_telemetry_t *telemetry)
{
    unsigned char const *bytes;

    bytes = (unsigned char const *)data;
    return sixel_palette_build_kmedoids(palette,
                                        bytes,
                                        length,
                                        pixelformat,
                                        allocator,
                                        logger,
                                        job_seq,
                                        engine_name,
                                        telemetry);
}

static SIXELSTATUS
sixel_palette_build_kmedoids_float32_dispatch(
    sixel_palette_t *palette,
    void const *data,
    unsigned int length,
    int pixelformat,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger,
    int *job_seq,
    char const *engine_name,
    sixel_palette_telemetry_t *telemetry)
{
    float const *samples;

    samples = (float const *)data;
    return sixel_palette_build_kmedoids_float32(palette,
                                                samples,
                                                length,
                                                pixelformat,
                                                allocator,
                                                logger,
                                                job_seq,
                                                engine_name,
                                                telemetry);
}

static SIXELSTATUS
sixel_palette_build_heckbert_dispatch(sixel_palette_t *palette,
                                      void const *data,
                                      unsigned int length,
                                      int pixelformat,
                                      sixel_allocator_t *allocator,
                                      sixel_timeline_logger_t *logger,
                                      int *job_seq,
                                      char const *engine_name,
                                      sixel_palette_telemetry_t *telemetry)
{
    unsigned char const *bytes;

    bytes = (unsigned char const *)data;
    return sixel_palette_build_heckbert(palette,
                                        bytes,
                                        length,
                                        pixelformat,
                                        allocator,
                                        logger,
                                        job_seq,
                                        engine_name,
                                        telemetry);
}

static SIXELSTATUS
sixel_palette_build_heckbert_float32_dispatch(
    sixel_palette_t *palette,
    void const *data,
    unsigned int length,
    int pixelformat,
    sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger,
    int *job_seq,
    char const *engine_name,
    sixel_palette_telemetry_t *telemetry)
{
    float const *samples;

    samples = (float const *)data;
    return sixel_palette_build_heckbert_float32(palette,
                                                samples,
                                                length,
                                                pixelformat,
                                                allocator,
                                                logger,
                                                job_seq,
                                                engine_name,
                                                telemetry);
}

typedef struct sixel_palette_quant_engine {
    char const *name;
    int quantize_model;
    int requires_float32;
    sixel_palette_quant_dispatch_fn build_fn;
} sixel_palette_quant_engine_t;

static sixel_palette_quant_engine_t const sixel_palette_quant_engines[] = {
    {
        "kmeans-float32",
        SIXEL_QUANTIZE_MODEL_KMEANS,
        1,
        sixel_palette_build_kmeans_float32_dispatch,
    },
    {
        "kmeans-legacy",
        SIXEL_QUANTIZE_MODEL_KMEANS,
        0,
        sixel_palette_build_kmeans_dispatch,
    },
    {
        "heckbert-float32",
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        1,
        sixel_palette_build_heckbert_float32_dispatch,
    },
    {
        "kmedoids-float32",
        SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        1,
        sixel_palette_build_kmedoids_float32_dispatch,
    },
    {
        "kmedoids-legacy",
        SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        0,
        sixel_palette_build_kmedoids_dispatch,
    },
    {
        "kcenter-float32",
        SIXEL_QUANTIZE_MODEL_KCENTER,
        1,
        sixel_palette_build_kcenter_float32_dispatch,
    },
    {
        "kcenter-legacy",
        SIXEL_QUANTIZE_MODEL_KCENTER,
        0,
        sixel_palette_build_kcenter_dispatch,
    },
    {
        "heckbert-legacy",
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        0,
        sixel_palette_build_heckbert_dispatch,
    },
};

static size_t
sixel_palette_quant_engine_total(void)
{
    size_t total;

    total = sizeof(sixel_palette_quant_engines)
          / sizeof(sixel_palette_quant_engines[0]);
    return total;
}

static char const *
sixel_palette_merge_mode_name(int merge_mode)
{
    switch (merge_mode) {
    case SIXEL_FINAL_MERGE_WARD:
        return "ward";
    case SIXEL_FINAL_MERGE_AUTO:
        return "auto";
    case SIXEL_FINAL_MERGE_NONE:
    default:
        return "none";
    }
}

static void
sixel_palette_format_quant_message(
    char *buffer,
    size_t size,
    sixel_palette_quant_engine_t const *engine,
    sixel_palette_telemetry_t const *telemetry)
{
    char merge_desc[64];
    double init_ms;
    double iterate_ms;
    double merge_ms;
    double export_ms;
    unsigned int iterate_count;
    unsigned int merge_iterate_count;
    char const *merge_name;
    int merge_mode;

    if (buffer == NULL) {
        return;
    }

    init_ms = 0.0;
    iterate_ms = 0.0;
    merge_ms = 0.0;
    export_ms = 0.0;
    iterate_count = 0U;
    merge_iterate_count = 0U;
    merge_name = "none";
    merge_mode = SIXEL_FINAL_MERGE_NONE;
    if (telemetry != NULL) {
        init_ms = telemetry->init_ms;
        iterate_ms = telemetry->iterate_ms;
        merge_ms = telemetry->merge_ms;
        export_ms = telemetry->export_ms;
        iterate_count = telemetry->iterate_count;
        merge_iterate_count = telemetry->merge_iterate_count;
        merge_mode = telemetry->merge_mode;
    }
    merge_name = sixel_palette_merge_mode_name(merge_mode);
    if (merge_ms > 0.0 || merge_iterate_count > 0U
        || merge_mode != SIXEL_FINAL_MERGE_NONE) {
        (void)sixel_compat_snprintf(merge_desc,
                                    sizeof(merge_desc),
                                    "%s:%u/%.3fms",
                                    merge_name,
                                    merge_iterate_count,
                                    merge_ms);
    } else {
        (void)sixel_compat_snprintf(merge_desc,
                                    sizeof(merge_desc),
                                    "%s",
                                    merge_name);
    }

    (void)sixel_compat_snprintf(buffer,
                                size,
                                "engine=%s init=%.3fms iter=%u/%.3fms "
                                "merge=%s export=%.3fms",
                                engine != NULL ? engine->name : "",
                                init_ms,
                                iterate_count,
                                iterate_ms,
                                merge_desc,
                                export_ms);
}

/* Locate a quantizer engine that satisfies the model/format requirements. */
static sixel_palette_quant_engine_t const *
sixel_palette_quant_engine_lookup(int quantize_model,
                                  int needs_float32)
{
    sixel_palette_quant_engine_t const *engine;
    size_t index;
    size_t total;

    engine = NULL;
    total = sixel_palette_quant_engine_total();
    for (index = 0U; index < total; ++index) {
        engine = &sixel_palette_quant_engines[index];
        if (engine->quantize_model != quantize_model) {
            continue;
        }
        if (needs_float32 && !engine->requires_float32) {
            continue;
        }
        if (!needs_float32 && engine->requires_float32) {
            continue;
        }
        return engine;
    }

    return NULL;
}

static SIXELSTATUS
sixel_palette_quant_engine_run(sixel_palette_quant_engine_t const *engine,
                               sixel_palette_t *palette,
                               void const *data,
                               unsigned int length,
                               int pixelformat,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_timeline_logger_t *logger;
    char const *forced_failure;
    char const *post_build_failure;
    sixel_palette_telemetry_t telemetry;
    /*
     * Quantizer internals still accept an int pointer as the timeline logging
     * enable flag, but concrete ids now come from the shared atomic allocator.
     */
    int child_job_seq;
    int job_id;
    char span_message[192];

    status = SIXEL_LOGIC_ERROR;
    forced_failure = NULL;
    post_build_failure = NULL;
    if (engine == NULL || engine->build_fn == NULL) {
        return status;
    }
    forced_failure = sixel_test_environment_palette_quantizer_failure();
    if (forced_failure != NULL &&
            (strcmp(forced_failure, engine->name) == 0 ||
             (strcmp(forced_failure, "kmeans") == 0 &&
              engine->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS))) {
        return SIXEL_RUNTIME_ERROR;
    }

    logger = NULL;
    (void)sixel_timeline_logger_prepare_env(allocator, &logger);
    job_id = sixel_palette_timeline_next_job_id();
    child_job_seq = 0;
    memset(&telemetry, 0, sizeof(telemetry));
    span_message[0] = '\0';

    sixel_timeline_logger_logf(logger,
                      "palette",
                      "palette/build",
                      "start",
                      job_id);

    status = engine->build_fn(palette,
                              data,
                              length,
                              pixelformat,
                              allocator,
                              logger,
                              &child_job_seq,
                              engine->name,
                              &telemetry);

    post_build_failure =
        sixel_test_environment_palette_quantizer_post_build_failure();
    if (SIXEL_SUCCEEDED(status) && post_build_failure != NULL &&
            strcmp(post_build_failure, engine->name) == 0) {
        status = SIXEL_RUNTIME_ERROR;
    }

    sixel_palette_format_quant_message(span_message,
                                       sizeof(span_message),
                                       engine,
                                       &telemetry);

    sixel_timeline_logger_logf(logger,
                      "palette",
                      "palette/build",
                      "finish",
                      job_id);

    sixel_timeline_logger_unref(logger);

    return status;
}

/* Solvers choose arithmetic from the actual input type. A float failure
 * must not retry byte storage, even without the native output cache.
 * Explicit snap still runs inside the builder. */
static SIXELSTATUS
sixel_palette_apply_typed_engine(sixel_palette_t *palette,
                                 int quantize_model,
                                 void const *data,
                                 unsigned int length,
                                 int pixelformat,
                                 sixel_allocator_t *allocator)
{
    sixel_palette_quant_engine_t const *engine;

    if (quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS &&
            SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        /* Byte lookup caches cannot represent the Lloyd float coordinates. */
        SIXEL_PALETTE_CONTEXT(palette)->lut_policy = SIXEL_LUT_POLICY_NONE;
    }
    engine = sixel_palette_quant_engine_lookup(quantize_model,
        SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat));
    if (engine == NULL) {
        return SIXEL_LOGIC_ERROR;
    }
    return sixel_palette_quant_engine_run(engine, palette, data, length,
        pixelformat, allocator);
}

/* Palette orchestration delegates algorithm specifics to dedicated modules. */

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

/* Invalidating working entries also invalidates their output interpretation.
 * Workers only borrow a completed cache; preparation and disposal are owned
 * by the palette's caller outside the parallel application interval. */
static void
sixel_palette_clear_output(sixel_palette_storage_t *storage)
{
    sixel_cdef_free(storage->output_srgb);
    sixel_cdef_free(storage->output_device);
    storage->output_srgb = NULL;
    storage->output_device = NULL;
    storage->output_colorspace = SIXEL_COLORSPACE_GAMMA;
    storage->output_rgb101_count = 0U;
    storage->output_hls_ready = 0;
}

static void
sixel_palette_dispose(sixel_palette_storage_t *storage)
{
    sixel_allocator_t *allocator;

    if (storage == NULL) {
        return;
    }

    allocator = storage->allocator;
    sixel_palette_clear_output(storage);
    if (storage->entries != NULL) {
        sixel_allocator_free(allocator, storage->entries);
        storage->entries = NULL;
    }
    if (storage->entries_float32 != NULL) {
        sixel_allocator_free(allocator, storage->entries_float32);
        storage->entries_float32 = NULL;
    }
}

static void
sixel_palette_vtbl_ref(sixel_palette_t *palette)
{
    sixel_palette_storage_t *storage;

    storage = NULL;
    if (palette != NULL) {
        storage = SIXEL_PALETTE_STORAGE(palette);
        (void)sixel_atomic_fetch_add_u32(&storage->ref, 1U);
    }
}

static void
sixel_palette_vtbl_unref(sixel_palette_t *palette)
{
    sixel_palette_storage_t *storage;
    sixel_allocator_t *allocator;
    unsigned int previous;

    storage = NULL;
    allocator = NULL;
    previous = 0U;
    if (palette == NULL) {
        return;
    }

    storage = SIXEL_PALETTE_STORAGE(palette);
    previous = sixel_atomic_fetch_sub_u32(&storage->ref, 1U);
    if (previous == 1U) {
        allocator = storage->allocator;
        sixel_palette_dispose(storage);
        storage->allocator = NULL;
        if (allocator != NULL) {
            sixel_allocator_free(allocator, storage);
            sixel_allocator_unref(allocator);
        }
    }
}

static SIXELSTATUS
sixel_palette_vtbl_init_entries(
    sixel_palette_t *palette,
    sixel_palette_entries_request_t const *request)
{
    if (request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_palette_set_entries(palette,
                                     request->entries,
                                     request->colors,
                                     request->depth,
                                     NULL);
}

static SIXELSTATUS
sixel_palette_vtbl_init_entries_float32(
    sixel_palette_t *palette,
    sixel_palette_float32_entries_request_t const *request)
{
    if (request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_palette_set_entries_float32(palette,
                                             request->entries,
                                             request->colors,
                                             request->depth,
                                             NULL);
}

static SIXELSTATUS
sixel_palette_vtbl_get_entries(
    sixel_palette_t const *palette,
    sixel_palette_entries_view_t *view)
{
    sixel_palette_storage_t const *storage;

    storage = NULL;
    if (palette == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    view->entries = storage->entries;
    view->entries_size = storage->entries_size;
    view->entry_count = storage->entry_count;
    view->depth = storage->depth;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_vtbl_get_entries_float32(
    sixel_palette_t const *palette,
    sixel_palette_float32_entries_view_t *view)
{
    sixel_palette_storage_t const *storage;

    storage = NULL;
    if (palette == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    view->entries = storage->entries_float32;
    view->entries_size = storage->entries_float32_size;
    view->entry_count = storage->entry_count;
    view->depth = storage->float_depth;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_vtbl_get_metadata(
    sixel_palette_t const *palette,
    sixel_palette_metadata_t *metadata)
{
    sixel_palette_storage_t const *storage;

    storage = NULL;
    if (palette == NULL || metadata == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    metadata->entry_count = storage->entry_count;
    metadata->requested_colors = storage->requested_colors;
    metadata->original_colors = storage->original_colors;
    metadata->depth = storage->depth;
    metadata->float_depth = storage->float_depth;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_palette_storage_t *storage;

    storage = NULL;
    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;

    storage = (sixel_palette_storage_t *)
        sixel_allocator_malloc(allocator, sizeof(*storage));
    if (storage == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_factory_new: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    storage->palette_interface.vtbl = &g_sixel_palette_vtbl;
    storage->ref = 1U;
    storage->allocator = allocator;
    storage->entries = NULL;
    storage->entries_size = 0U;
    storage->entries_float32 = NULL;
    storage->entries_float32_size = 0U;
    storage->entry_count = 0U;
    storage->requested_colors = 0U;
    storage->original_colors = 0U;
    storage->depth = 0;
    storage->float_depth = 0;
    memset(&storage->build_attempt, 0, sizeof(storage->build_attempt));
    storage->build_attempt_active = 0;
    storage->build_context = NULL;
    storage->output_srgb = NULL;
    storage->output_device = NULL;
    storage->output_colorspace = SIXEL_COLORSPACE_GAMMA;
    storage->output_rgb101_count = 0U;
    storage->output_hls_ready = 0;
    sixel_allocator_ref(allocator);

    *object = &storage->palette_interface;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_build_attempt_begin(
    sixel_palette_t *palette,
    sixel_palette_build_attempt_t const *attempt)
{
    sixel_palette_storage_t *storage;

    storage = NULL;
    if (palette == NULL || attempt == NULL ||
            attempt->reserved_colors >= SIXEL_PALETTE_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (storage->build_context != NULL ||
            storage->build_attempt_active != 0) {
        return SIXEL_LOGIC_ERROR;
    }
    storage->build_attempt = *attempt;
    storage->build_attempt_active = 1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_build_attempt_finish(
    sixel_palette_t *palette,
    sixel_palette_build_attempt_t *attempt)
{
    sixel_palette_storage_t *storage;

    storage = NULL;
    if (palette == NULL || attempt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (storage->build_context != NULL ||
            storage->build_attempt_active == 0) {
        return SIXEL_LOGIC_ERROR;
    }
    *attempt = storage->build_attempt;
    memset(&storage->build_attempt, 0, sizeof(storage->build_attempt));
    storage->build_attempt_active = 0;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_storage_t *storage;
    size_t required;
    size_t old_size;
    unsigned char *resized;

    storage = NULL;
    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);

    required = (size_t)colors * (size_t)depth;
    if (required == 0U) {
        if (storage->entries != NULL) {
            sixel_allocator_free(allocator, storage->entries);
            storage->entries = NULL;
        }
        storage->entries_size = 0U;
        return SIXEL_OK;
    }

    if (storage->entries != NULL && storage->entries_size >= required) {
        return SIXEL_OK;
    }

    old_size = storage->entries_size;
    if (storage->entries == NULL) {
        resized = (unsigned char *)sixel_allocator_malloc(allocator, required);
    } else {
        resized = (unsigned char *)sixel_allocator_realloc(allocator,
                                                           storage->entries,
                                                           required);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (required > old_size) {
        memset(resized + old_size, 0, required - old_size);
    }

    storage->entries = resized;
    storage->entries_size = required;

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
    sixel_palette_storage_t *storage;
    size_t required_bytes;
    size_t old_bytes;
    float *resized;

    storage = NULL;
    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);

    required_bytes = (size_t)colors * (size_t)depth;
    if (required_bytes == 0U) {
        if (storage->entries_float32 != NULL) {
            sixel_allocator_free(allocator, storage->entries_float32);
            storage->entries_float32 = NULL;
        }
        storage->entries_float32_size = 0U;
        return SIXEL_OK;
    }

    if (storage->entries_float32 != NULL
            && storage->entries_float32_size >= required_bytes) {
        return SIXEL_OK;
    }

    old_bytes = storage->entries_float32_size;
    if (storage->entries_float32 == NULL) {
        resized = (float *)sixel_allocator_malloc(allocator,
                                                  required_bytes);
    } else {
        resized = (float *)sixel_allocator_realloc(allocator,
                                                   storage->entries_float32,
                                                   required_bytes);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries_float32: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (required_bytes > old_bytes) {
        memset((unsigned char *)resized + old_bytes,
               0,
               required_bytes - old_bytes);
    }

    storage->entries_float32 = resized;
    storage->entries_float32_size = required_bytes;

    status = SIXEL_OK;

    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;
    sixel_palette_storage_t *storage;

    storage = NULL;
    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = storage->allocator;
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

    storage->entry_count = colors;
    storage->requested_colors = colors;
    storage->depth = depth;
    sixel_palette_clear_output(storage);

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_storage_t *storage;
    size_t payload_size;

    storage = NULL;
    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);

    status = sixel_palette_resize(palette, colors, depth, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    payload_size = (size_t)colors * (size_t)depth;
    if (entries != NULL && storage->entries != NULL && payload_size > 0U) {
        memcpy(storage->entries, entries, payload_size);
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_set_entries_float32(sixel_palette_t *palette,
                                  float const *entries,
                                  unsigned int colors,
                                  int depth,
                                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;
    sixel_palette_storage_t *storage;
    size_t payload_size;

    storage = NULL;
    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = storage->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (colors == 0U || depth == 0 || entries == NULL) {
        sixel_palette_clear_output(storage);
        if (storage->entries_float32 != NULL) {
            sixel_allocator_free(work_allocator,
                                 storage->entries_float32);
            storage->entries_float32 = NULL;
        }
        storage->entries_float32_size = 0U;
        storage->float_depth = 0;
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
    if (payload_size > 0U && storage->entries_float32 != NULL) {
        memcpy(storage->entries_float32, entries, payload_size);
    }
    storage->entries_float32_size = payload_size;
    storage->float_depth = depth;
    sixel_palette_clear_output(storage);

    return SIXEL_OK;
}

/* Publish cover in both palette views only after the native repair and
 * all required allocations have succeeded. The float view is authoritative. */
static SIXELSTATUS
sixel_palette_repair_native(sixel_palette_t *palette,
                            sixel_palette_generate_request_t const *request,
                            unsigned int *ncolors,
                            unsigned int capacity,
                            sixel_allocator_t *work_allocator)
{
    sixel_palette_storage_t *storage;
    SIXELSTATUS status;
    float repaired[SIXEL_PALETTE_MAX * 3];
    unsigned int repaired_count;
    unsigned int index;
    unsigned int channel;

    if (sixel_palette_cover_float_budget(sixel_palette_cover_policy(),
            *ncolors) == 0) {
        return SIXEL_OK;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (storage->entries_float32 == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        return status;
    }
    memcpy(repaired, storage->entries_float32,
        *ncolors * 3u * sizeof(float));
    repaired_count = *ncolors;
    status = sixel_palette_cover_float(repaired, &repaired_count,
        request->data, request->length, request->pixelformat,
        sixel_palette_cover_policy(), sixel_palette_cover_mode(),
        sixel_palette_cover_grow_enabled(),
        capacity,
        work_allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    /* Reserve both views before publishing either. A failed allocation
     * can grow capacity, but cannot leave mismatched palette contents. */
    status = sixel_palette_resize_entries(palette, repaired_count, 3,
        work_allocator);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_palette_resize_entries_float32(palette,
            repaired_count, 3u * sizeof(float), work_allocator);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }
    memcpy(storage->entries_float32, repaired,
        repaired_count * 3u * sizeof(float));
    for (index = 0; index < repaired_count; ++index) {
        for (channel = 0; channel < 3; ++channel) {
            storage->entries[index * 3u + channel] =
                sixel_pixelformat_float_channel_to_byte(
                    request->pixelformat, (int)channel,
                    repaired[index * 3u + channel]);
        }
    }
    *ncolors = repaired_count;
    return SIXEL_OK;
}

/*
 * sixel_palette_generate builds the palette entries inside the provided
 * sixel_palette_t instance.  The dispatcher consults the quantizer engine
 * registry to select the requested solver. All generators
 * choose arithmetic from the actual input type; a typed float failure is
 * returned without narrowing or trying another solver. True byte requests
 * retain their historical Heckbert fallback where binning permits it.
 *
 * The branches share cache management and policy post-processing helpers.
 * The palette object tracks the generated metadata so the caller can
 * publish it without recomputing.
 */
static SIXELSTATUS
sixel_palette_generate_internal(
    sixel_palette_t *palette,
    sixel_palette_generate_request_t const *request,
    int preserve_extended_rgb)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_storage_t *storage;
    sixel_palette_build_context_t context;
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    int effective_quantize_model;
    unsigned int snap_component;
    unsigned int color_capacity;
    sixel_allocator_t *work_allocator;
    sixel_palette_generate_request_t sampled_request;
    sixel_palette_generate_request_t const *original_request;
    float *snap_samples;
    double sample[3];
    unsigned int sample_index;
    unsigned int sample_channel;
    int snap_started;
    SIXELSTATUS snap_status;

    storage = NULL;
    original_request = request;
    snap_samples = NULL;
    snap_started = 0;
    memset(&context, 0, sizeof(context));
    effective_quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    if (palette == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    sixel_palette_clear_output(storage);
    if (storage->build_context != NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: palette build is already active.");
        return SIXEL_LOGIC_ERROR;
    }
    work_allocator = storage->allocator;
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result_depth = sixel_helper_compute_depth(request->pixelformat);
    if (result_depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = (unsigned int)result_depth;
    context.requested_colors = request->requested_colors;
    context.method_for_largest = request->method_for_largest;
    context.method_for_rep = request->method_for_rep;
    context.quality_mode = request->quality_mode;
    context.force_palette = request->force_palette;
    /* Destination-aware stages preserve samples unless SAMPLE is explicit.
     * The common owner keeps final snap separate from solver-local hooks. */
    context.use_reversible = preserve_extended_rgb &&
        !sixel_palette_snap_destination_active() ? 0 : request->use_reversible;
    context.quantize_model = request->quantize_model;
    context.final_merge_mode = request->final_merge_mode;
    context.lut_policy = request->lut_policy;
    context.attempt = storage->build_attempt_active != 0
        ? &storage->build_attempt
        : NULL;
    /* Caller-owned slots are absent from every color calculation. This
     * attempt-local capacity also bounds cover growth on the byte path. */
    color_capacity = SIXEL_PALETTE_MAX;
    if (context.attempt != NULL) {
        color_capacity -= context.attempt->reserved_colors;
    }
    storage->requested_colors = request->requested_colors;
    storage->build_context = &context;

    status = sixel_palette_snap_begin(request->pixelformat,
        context.quantize_model, work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    snap_started = 1;
    if (sixel_palette_snap_destination_active() &&
            sixel_palette_should_snap(SIXEL_PALETTE_SNAP_STAGE_SAMPLE)) {
        snap_samples = sixel_allocator_malloc(work_allocator, request->length);
        if (snap_samples == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(snap_samples, request->data, request->length);
        for (sample_index = 0U;
                sample_index < request->length / (3U * sizeof(float));
                ++sample_index) {
            for (sample_channel = 0U; sample_channel < 3U; ++sample_channel) {
                sample[sample_channel] =
                    snap_samples[sample_index * 3U + sample_channel];
            }
            sixel_palette_snap_triple(sample, 1, request->pixelformat,
                                       SIXEL_PALETTE_SNAP_STAGE_SAMPLE);
            status = sixel_palette_snap_status();
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            for (sample_channel = 0U; sample_channel < 3U; ++sample_channel) {
                snap_samples[sample_index * 3U + sample_channel] =
                    (float)sample[sample_channel];
            }
        }
        sampled_request = *request;
        sampled_request.data = (unsigned char const *)snap_samples;
        request = &sampled_request;
    }
    status = SIXEL_FALSE;

    if (context.quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = sixel_palette_apply_typed_engine(palette,
            context.quantize_model, request->data, request->length,
            request->pixelformat, work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            effective_quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
            ncolors = storage->entry_count;
            origcolors = storage->original_colors;
            depth = (unsigned int)storage->depth;
            goto success;
        }
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
            goto end;
        }
    } else if (context.quantize_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
        status = sixel_palette_apply_typed_engine(palette,
            context.quantize_model, request->data, request->length,
            request->pixelformat, work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            effective_quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
            ncolors = storage->entry_count;
            origcolors = storage->original_colors;
            depth = (unsigned int)storage->depth;
            goto success;
        }
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
            /* Preserve the original failure and the requested numeric type. */
            goto end;
        }
    } else if (context.quantize_model == SIXEL_QUANTIZE_MODEL_KMEDOIDS) {
        status = sixel_palette_apply_typed_engine(palette,
            context.quantize_model, request->data, request->length,
            request->pixelformat, work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            effective_quantize_model = SIXEL_QUANTIZE_MODEL_KMEDOIDS;
            ncolors = storage->entry_count;
            origcolors = storage->original_colors;
            depth = (unsigned int)storage->depth;
            goto success;
        }
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
            /* Preserve the original failure and the requested numeric type. */
            goto end;
        }
    } else if (context.quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        status = sixel_palette_apply_typed_engine(palette,
            SIXEL_QUANTIZE_MODEL_MEDIANCUT, request->data, request->length,
            request->pixelformat, work_allocator);
        goto after_quantizer;
    }

    if (preserve_extended_rgb != 0) {
        goto end;
    }

    status = sixel_palette_apply_typed_engine(palette,
        SIXEL_QUANTIZE_MODEL_MEDIANCUT, request->data, request->length,
        request->pixelformat, work_allocator);
    if (SIXEL_SUCCEEDED(status)) {
        effective_quantize_model = SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    }

after_quantizer:
    if (SIXEL_SUCCEEDED(status)) {
        effective_quantize_model = SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    }
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = storage->entry_count;
    origcolors = storage->original_colors;
    depth = (unsigned int)storage->depth;
    if (context.use_reversible && storage->entries != NULL) {
        sixel_palette_snap_entries(storage->entries, storage->entries_float32,
                                   ncolors, request->pixelformat);
    }
    status = SIXEL_OK;

success:
    request = original_request;
    snap_status = sixel_palette_snap_status();
    if (SIXEL_FAILED(snap_status)) {
        status = snap_status;
        goto end;
    }
    if (SIXEL_SUCCEEDED(status) && color_capacity < SIXEL_PALETTE_MAX &&
            ncolors > color_capacity) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (SIXEL_SUCCEEDED(status) && preserve_extended_rgb) {
        if (request->use_reversible) {
            status = sixel_palette_snap_native(storage->entries_float32,
                ncolors, request->pixelformat);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            /* Refresh only the compatibility view, never round the native
             * result through bytes before working-space conversion. */
            for (snap_component = 0; snap_component < ncolors * 3u;
                    ++snap_component) {
                storage->entries[snap_component] =
                    sixel_pixelformat_float_channel_to_byte(
                        request->pixelformat, (int)(snap_component % 3u),
                        storage->entries_float32[snap_component]);
            }
        }
        /* Cover follows final snap. Its new anchors and funded midpoints
         * retain their coordinates unless the caller explicitly selects
         * application preparation below. Never rebuild this typed palette
         * from the RGB101 compatibility cache. */
        status = sixel_palette_repair_native(palette, request, &ncolors,
            color_capacity, work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (sixel_palette_snap_destination_active()) {
            status = sixel_palette_snap_stage_entries(
                storage->entries_float32, ncolors, request->pixelformat,
                SIXEL_PALETTE_SNAP_STAGE_APPLY);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            for (snap_component = 0U; snap_component < ncolors * 3U;
                    ++snap_component) {
                storage->entries[snap_component] =
                    sixel_pixelformat_float_channel_to_byte(
                        request->pixelformat, (int)(snap_component % 3U),
                        storage->entries_float32[snap_component]);
            }
        }
    }
    /*
     * Legacy byte repair follows the same source-coverage objective. The
     * nearest-entry threshold is a heuristic, not a convex-hull test; neither
     * repair pass guarantees that every source color becomes reachable.
     */
    if (SIXEL_SUCCEEDED(status) && storage->entries != NULL
            && storage->entries_float32 == NULL) {
        unsigned char anchors[SIXEL_PALETTE_COVER_ANCHOR_MAX * 3u];
        unsigned int anchor_count;
        unsigned int budget;

        /*
         * Soft takes its anchors out of the samples the solver was given, so
         * every anchor is a color the image actually contains.  The samples
         * are still in the request, so this needs no plumbing.
         */
        anchor_count = 0u;
        budget = sixel_palette_cover_budget(sixel_palette_cover_policy(),
                                            ncolors);
        if (budget > SIXEL_PALETTE_COVER_ANCHOR_MAX) {
            budget = SIXEL_PALETTE_COVER_ANCHOR_MAX;
        }
        if (sixel_palette_cover_mode() == SIXEL_PALETTE_COVER_MODE_SOFT
                && sixel_palette_cover_candidates_supported(
                       request->pixelformat)) {
            /*
             * Soft owns the outcome once the format is readable, INCLUDING
             * the outcome of finding nothing to do.  Falling back to hard on
             * an empty candidate list would put the cube lattice on exactly
             * the images the palette already covers -- the ones the cube is
             * worst for.
             */
            if (budget > 0u) {
                anchor_count = sixel_palette_cover_collect_candidates(
                    request->data,
                    request->length,
                    request->pixelformat,
                    storage->entries,
                    ncolors,
                    (int)depth,
                    anchors,
                    budget,
                    work_allocator);
            }
            if (sixel_palette_cover_grow_enabled()
                    && ncolors < color_capacity) {
                unsigned int room;
                unsigned int i;

                room = color_capacity - ncolors;
                if (room > anchor_count) {
                    room = anchor_count;
                }
                if (SIXEL_SUCCEEDED(sixel_palette_resize_entries(
                            palette, ncolors + room, depth, work_allocator))
                        && storage->entries != NULL) {
                    for (i = 0u; i < room; ++i) {
                        memcpy(storage->entries
                                   + (size_t)(ncolors + i) * (size_t)depth,
                               anchors + (size_t)i * 3u,
                               3u);
                    }
                    ncolors += room;
                }
            }
            /*
             * Whatever did not fit as a new entry is funded by merging, the
             * same way cover_grow=0 funds everything.
             */
            sixel_palette_cover_place(storage->entries,
                                      ncolors,
                                      (int)depth,
                                      anchors,
                                      anchor_count);
        } else if (sixel_palette_cover_grow_enabled()
                && ncolors < color_capacity) {
            /*
             * Growing spends no existing entry, so the anchors cost nothing in
             * quality -- but it hands back more colors than the caller asked
             * for, which is only safe when they opted in.  A caller that named
             * a color count because that is all their terminal has must not be
             * handed extra registers.
             */
            unsigned char extra[SIXEL_PALETTE_COVER_ANCHOR_MAX * 3u];
            unsigned int room;
            unsigned int added;

            room = color_capacity - ncolors;
            if (room > SIXEL_PALETTE_COVER_ANCHOR_MAX) {
                room = SIXEL_PALETTE_COVER_ANCHOR_MAX;
            }
            added = sixel_palette_cover_missing_anchors(
                storage->entries,
                ncolors,
                (int)depth,
                sixel_palette_cover_policy(),
                extra,
                room);
            if (added > 0u
                    && SIXEL_SUCCEEDED(sixel_palette_resize_entries(
                           palette, ncolors + added, depth, work_allocator))
                    && storage->entries != NULL) {
                unsigned int i;

                for (i = 0u; i < added; ++i) {
                    memcpy(storage->entries
                               + (size_t)(ncolors + i) * (size_t)depth,
                           extra + (size_t)i * 3u,
                           3u);
                }
                ncolors += added;
            }
            /*
             * Growing is capped by the ceiling, not by the anchor count, so a
             * palette that starts near SIXEL_PALETTE_MAX has room for only the
             * first few.  Merge for whatever is still missing: without this,
             * asking to grow a 250-color palette bought seven corners and lost
             * the faces and edges that merging alone would have placed, making
             * cover_grow=1 strictly worse than cover_grow=0.
             */
            (void)sixel_palette_cover_anchor_rgb888(storage->entries,
                                                    ncolors,
                                                    (int)depth);
        } else {
            (void)sixel_palette_cover_anchor_rgb888(storage->entries,
                                                    ncolors,
                                                    (int)depth);
        }
    }

    storage->entry_count = ncolors;
    storage->original_colors = origcolors;
    storage->depth = (int)depth;

end:
    sixel_allocator_free(work_allocator, snap_samples);
    if (snap_started) {
        snap_status = sixel_palette_snap_end();
        if (SIXEL_FAILED(snap_status) && SIXEL_SUCCEEDED(status)) {
            status = snap_status;
        }
    }
    if (SIXEL_SUCCEEDED(status) && context.attempt != NULL) {
        context.attempt->quantize_model = effective_quantize_model;
    }
    storage->build_context = NULL;
    return status;
}

static SIXELSTATUS
sixel_palette_vtbl_generate(
    sixel_palette_t *palette,
    sixel_palette_generate_request_t const *request)
{
    return sixel_palette_generate_internal(palette, request, 0);
}

SIXELSTATUS
sixel_palette_generate_extended(
    sixel_palette_t *palette,
    sixel_palette_generate_request_t const *request)
{
    sixel_palette_storage_t *storage;

    if (palette == NULL || request == NULL || request->prefer_float32 == 0 ||
            (request->quantize_model != SIXEL_QUANTIZE_MODEL_KMEANS &&
             request->quantize_model != SIXEL_QUANTIZE_MODEL_KCENTER &&
             request->quantize_model != SIXEL_QUANTIZE_MODEL_KMEDOIDS &&
             request->quantize_model != SIXEL_QUANTIZE_MODEL_MEDIANCUT) ||
            (request->use_reversible &&
             !sixel_palette_snap_destination_active() &&
             sixel_palette_should_snap(
                 SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_PRE)) ||
            (sixel_resolve_final_merge_mode(request->final_merge_mode) !=
                 SIXEL_FINAL_MERGE_NONE &&
             sixel_resolve_final_merge_mode(request->final_merge_mode) !=
                 SIXEL_FINAL_MERGE_WARD) ||
            !SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (storage->build_attempt_active == 0 ||
            (request->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS &&
             sixel_get_kmeans_feedback_mode() !=
                SIXEL_PALETTE_KMEANS_FEEDBACK_OFF)) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* The opt-in is a call argument, not retained instance or thread state.
     * Every return clears the ordinary attempt-local build context. */
    return sixel_palette_generate_internal(palette, request, 1);
}

/* Legacy byte access is mutable. Matching compatibility bytes do not own
 * precision: keep every original float fraction, including signed and
 * extended coordinates. A mismatch denotes an actual byte edit, so rebuild
 * the complete view before either lookup or output can consume it. */
SIXELSTATUS
sixel_palette_reconcile_bytes(sixel_palette_t *palette, int pixelformat)
{
    sixel_palette_storage_t *storage;
    size_t index;
    size_t count;
    int changed;
    int channel;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (storage->entries_float32 == NULL) {
        return SIXEL_OK;
    }
    count = (size_t)storage->entry_count * 3U;
    if (storage->entries == NULL || storage->depth != 3 ||
            storage->float_depth != (int)(3U * sizeof(float)) ||
            count / 3U != storage->entry_count ||
            count > SIZE_MAX / sizeof(float) ||
            storage->entries_size < count ||
            storage->entries_float32_size < count * sizeof(float)) {
        return SIXEL_BAD_ARGUMENT;
    }
    changed = 0;
    for (index = 0U; index < count; ++index) {
        if (!isfinite(storage->entries_float32[index])) {
            return SIXEL_BAD_INPUT;
        }
        channel = (int)(index % 3U);
        if (storage->entries[index] !=
                sixel_pixelformat_float_channel_to_byte(pixelformat,
                    channel, storage->entries_float32[index])) {
            changed = 1;
        }
    }
    if (changed) {
        for (index = 0U; index < count; ++index) {
            storage->entries_float32[index] = sixel_pixelformat_byte_to_float(
                pixelformat, (int)(index % 3U), storage->entries[index]);
        }
        sixel_palette_clear_output(storage);
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_prepare_rgb101(sixel_palette_t *palette, int source_colorspace,
                             int output_definition)
{
    return sixel_palette_prepare_output(palette, source_colorspace,
        output_definition, SIXEL_COLORSPACE_GAMMA);
}

SIXELSTATUS
sixel_palette_prepare_output(sixel_palette_t *palette, int source_colorspace,
                             int output_definition, int output_colorspace)
{
    return sixel_palette_prepare_output_definition(palette, source_colorspace,
        output_definition, output_colorspace, NULL);
}

SIXELSTATUS
sixel_palette_prepare_output_definition(sixel_palette_t *palette,
    int source_colorspace, int output_definition, int output_colorspace,
    sixel_color_def_t const *definition)
{
    sixel_palette_storage_t *storage;
    sixel_color_def_t *srgb;
    sixel_color_def_t *destination;
    sixel_color_xform_t *forward;
    SIXELSTATUS status;
    unsigned char codes[SIXEL_PALETTE_MAX * 3];
    float output_rgb[SIXEL_PALETTE_MAX * 3];
    double source[3];
    double gamma[3];
    double output[3];
    unsigned int index;
    unsigned int channel;
    unsigned int offset;

    if (palette == NULL ||
            (output_colorspace != SIXEL_COLORSPACE_GAMMA &&
             output_colorspace != SIXEL_COLORSPACE_LINEAR &&
             output_colorspace != SIXEL_COLORSPACE_SMPTEC) ||
            (output_definition != SIXEL_PALETTE_OUTPUT_LEGACY &&
             output_colorspace != SIXEL_COLORSPACE_GAMMA) ||
            (definition != NULL &&
             output_definition != SIXEL_NAMED_COLOR_COUNT) ||
            (definition == NULL &&
             sixel_cdef_named_name(output_definition) == NULL) ||
            (source_colorspace != SIXEL_COLORSPACE_GAMMA &&
             source_colorspace != SIXEL_COLORSPACE_LINEAR &&
             source_colorspace != SIXEL_COLORSPACE_OKLAB &&
             source_colorspace != SIXEL_COLORSPACE_CIELAB &&
             source_colorspace != SIXEL_COLORSPACE_DIN99D)) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (storage->entries == NULL || storage->entries_float32 == NULL ||
            storage->entry_count == 0U ||
            storage->entry_count > SIXEL_PALETTE_MAX ||
            storage->depth != 3 ||
            storage->float_depth != (int)(3U * sizeof(float)) ||
            storage->entries_float32_size <
                storage->entry_count * 3U * sizeof(float)) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = SIXEL_BAD_ALLOCATION;
    srgb = NULL;
    destination = NULL;
    forward = NULL;
    if (output_definition != SIXEL_PALETTE_OUTPUT_LEGACY) {
        srgb = sixel_cdef_name("srgb");
        destination = definition != NULL ? sixel_cdef_clone(definition)
            : sixel_cdef_name(sixel_cdef_named_name(output_definition));
        if (srgb == NULL || destination == NULL) {
            goto end;
        }
        forward = sixel_cxform_new(srgb, destination, 1);
        if (forward == NULL) {
            goto end;
        }
    }
    status = SIXEL_BAD_INPUT;
    for (index = 0U; index < storage->entry_count; ++index) {
        offset = index * 3U;
        for (channel = 0U; channel < 3U; ++channel) {
            source[channel] = storage->entries_float32[offset + channel];
        }
        if (forward != NULL) {
            if (!sixel_color_convert(gamma, source, source_colorspace,
                    SIXEL_COLORSPACE_GAMMA) ||
                    !sixel_cxform_apply(forward, gamma, output)) {
                goto end;
            }
        } else if (!sixel_color_convert_output(output, source,
                       source_colorspace, output_colorspace)) {
            goto end;
        }
        /* Quantize only the serialized view. Snap policy owns changes to
         * the working palette; caching wire codes must not enable snapping
         * implicitly or replace the palette used for lookup and diffusion. */
        for (channel = 0U; channel < 3U; ++channel) {
            if (output[channel] < 0.0) {
                output[channel] = 0.0;
            } else if (output[channel] > 1.0) {
                output[channel] = 1.0;
            }
            output_rgb[offset + channel] = (float)output[channel];
            codes[offset + channel] =
                (unsigned char)(output[channel] * 100.0 + 0.5);
            output[channel] =
                ((codes[offset + channel] * 255 + 50) / 100) / 255.0;
        }
        if (destination != NULL &&
                !sixel_cdef_to_xyz(destination, output, gamma, 1)) {
            goto end;
        }
    }
    /* Publish only after every transform succeeds. Keep both working views
     * unchanged, including extended coordinates and unsnapped centroids.
     * Cached codes retain the same indices; no later reordering is allowed. */
    sixel_palette_clear_output(storage);
    storage->output_srgb = srgb;
    storage->output_device = destination;
    storage->output_colorspace = output_colorspace;
    srgb = destination = NULL;
    memcpy(storage->output_rgb101, codes, storage->entry_count * 3U);
    memcpy(storage->output_rgb_float, output_rgb,
        storage->entry_count * 3U * sizeof(float));
    storage->output_rgb101_count = storage->entry_count;
    status = SIXEL_OK;
end:
    sixel_cxform_free(forward);
    sixel_cdef_free(srgb);
    sixel_cdef_free(destination);
    return status;
}

SIXELSTATUS
sixel_palette_get_rgb101(sixel_palette_t const *palette,
                         unsigned char const **entries, unsigned int *count)
{
    sixel_palette_storage_t const *storage;

    if (palette == NULL || entries == NULL || count == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *entries = NULL;
    *count = 0U;
    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    if (storage->output_rgb101_count == 0U ||
            storage->output_rgb101_count != storage->entry_count) {
        return SIXEL_LOGIC_ERROR;
    }
    *entries = storage->output_rgb101;
    *count = storage->output_rgb101_count;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_prepare_hls(sixel_palette_t *palette)
{
    sixel_palette_storage_t *storage;
    unsigned char const *codes;
    unsigned int count;
    unsigned int i;
    int n;
    int hls[3];
    double rgb[3];
    SIXELSTATUS status;

    status = sixel_palette_get_rgb101(palette, &codes, &count);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
    if (!storage->output_hls_ready) {
        for (i = 0U; i < count; ++i) {
            for (n = 0; n < 3; ++n) {
                rgb[n] = storage->output_rgb_float[i * 3U + n];
            }
            sixel_color_rgb_to_hls(hls, rgb);
            sixel_color_hls_to_rgb101(storage->output_hls_rgb101 + i * 3U,
                hls);
        }
        storage->output_hls_ready = 1;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_get_hls_rgb101(sixel_palette_t const *palette,
                             unsigned char const **entries,
                             unsigned int *count)
{
    SIXELSTATUS status;

    status = sixel_palette_get_rgb101(palette, entries, count);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    *entries = NULL;
    *count = 0U;
    if (!SIXEL_PALETTE_STORAGE_CONST(palette)->output_hls_ready) {
        return SIXEL_LOGIC_ERROR;
    }
    *entries = SIXEL_PALETTE_STORAGE_CONST(palette)->output_hls_rgb101;
    *count = SIXEL_PALETTE_STORAGE_CONST(palette)->output_rgb101_count;
    return SIXEL_OK;
}

/* HLS consumes destination RGB before percentage rounding. Its lifetime
 * and invalidation match RGB101, while neither view modifies working RGB. */
SIXELSTATUS
sixel_palette_get_output_rgb(sixel_palette_t const *palette,
                             float const **entries, unsigned int *count)
{
    unsigned char const *codes;
    SIXELSTATUS status;

    if (entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *entries = NULL;
    status = sixel_palette_get_rgb101(palette, &codes, count);
    if (SIXEL_SUCCEEDED(status)) {
        *entries = SIXEL_PALETTE_STORAGE_CONST(palette)->output_rgb_float;
    }
    return status;
}

int
sixel_palette_output_is_p3(sixel_palette_t const *palette)
{
    sixel_palette_storage_t const *storage;

    if (palette == NULL) {
        return 0;
    }
    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    return storage->output_rgb101_count != 0U &&
        storage->output_device != NULL;
}

int
sixel_palette_output_is_gamma(sixel_palette_t const *palette)
{
    sixel_palette_storage_t const *storage;

    if (palette == NULL) {
        return 0;
    }
    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    return storage->output_rgb101_count != 0U &&
        storage->output_device == NULL &&
        storage->output_colorspace == SIXEL_COLORSPACE_GAMMA;
}

int
sixel_palette_to_output(sixel_palette_t const *palette, double out[3],
                        double const in[3], int workspace)
{
    sixel_palette_storage_t const *storage;
    double gamma[3];
    double xyz[3];
    double result[3];

    if (palette == NULL || out == NULL || in == NULL) {
        return 0;
    }
    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    if (storage->output_rgb101_count == 0U) {
        return 0;
    }
    if (storage->output_device == NULL) {
        return sixel_color_convert_output(out, in, workspace,
            storage->output_colorspace);
    }
    if (!sixel_color_convert(gamma, in, workspace, SIXEL_COLORSPACE_GAMMA) ||
            !sixel_cdef_to_xyz(storage->output_srgb, gamma, xyz, 1) ||
            !sixel_cdef_from_xyz(storage->output_device, xyz, result, 1)) {
        return 0;
    }
    memcpy(out, result, sizeof(result));
    return 1;
}

int
sixel_palette_from_output(sixel_palette_t const *palette, double out[3],
                          double const in[3], int workspace)
{
    sixel_palette_storage_t const *storage;
    double xyz[3];
    double gamma[3];

    if (palette == NULL || out == NULL || in == NULL) {
        return 0;
    }
    storage = SIXEL_PALETTE_STORAGE_CONST(palette);
    if (storage->output_rgb101_count == 0U) {
        return 0;
    }
    if (storage->output_device == NULL) {
        return sixel_color_from_output(out, in, storage->output_colorspace,
            workspace);
    }
    /* Use the same immutable definitions in reverse. In particular, P3
     * primaries may become negative/over-one sRGB: do not project to bytes
     * or clamp before the working-space residual has been computed. */
    if (!sixel_cdef_to_xyz(storage->output_device, in, xyz, 1) ||
            !sixel_cdef_from_xyz(storage->output_srgb, xyz, gamma, 1)) {
        return 0;
    }
    return sixel_color_convert(out, gamma, SIXEL_COLORSPACE_GAMMA, workspace);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
