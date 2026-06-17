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

#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-heckbert.h"
#include "palette-kcenter.h"
#include "palette-kmeans.h"
#include "palette-kmedoids.h"
#include "palette-private.h"
#include "allocator.h"
#include "status.h"
#include "compat_stub.h"
#include "timeline-logger.h"

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
    sixel_palette_telemetry_t telemetry;
    /*
     * Quantizer internals still accept an int pointer as the timeline logging
     * enable flag, but concrete ids now come from the shared atomic allocator.
     */
    int child_job_seq;
    int job_id;
    char span_message[192];

    status = SIXEL_LOGIC_ERROR;
    if (engine == NULL || engine->build_fn == NULL) {
        return status;
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

/* Try the preferred float32 K-means engine before falling back to legacy. */
static SIXELSTATUS
sixel_palette_apply_kmeans_engines(sixel_palette_t *palette,
                                   void const *data,
                                   unsigned int length,
                                   int pixelformat,
                                   sixel_allocator_t *allocator,
                                   int prefer_float32)
{
    sixel_palette_quant_engine_t const *engine;
    sixel_palette_build_context_t *context;
    SIXELSTATUS status;
    int saved_lut_policy;

    engine = NULL;
    context = NULL;
    status = SIXEL_LOGIC_ERROR;
    saved_lut_policy = SIXEL_LUT_POLICY_AUTO;
    context = SIXEL_PALETTE_CONTEXT(palette);
    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    saved_lut_policy = context->lut_policy;

    if (prefer_float32) {
        engine = sixel_palette_quant_engine_lookup(
            SIXEL_QUANTIZE_MODEL_KMEANS,
            1);
        if (engine != NULL) {
            context->lut_policy = SIXEL_LUT_POLICY_NONE;
            status = sixel_palette_quant_engine_run(engine,
                                                    palette,
                                                    data,
                                                    length,
                                                    pixelformat,
                                                    allocator);
            if (SIXEL_SUCCEEDED(status)) {
                return status;
            }
            context->lut_policy = saved_lut_policy;
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
                                     void const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     int prefer_float32)
{
    sixel_palette_quant_engine_t const *engine;
    SIXELSTATUS status;

    status = SIXEL_LOGIC_ERROR;
    if (prefer_float32 && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
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

/* Run k-medoids and keep the same float32-first fallback ladder. */
static SIXELSTATUS
sixel_palette_apply_kcenter_engines(sixel_palette_t *palette,
                                    void const *data,
                                    unsigned int length,
                                    int pixelformat,
                                    sixel_allocator_t *allocator,
                                    int prefer_float32)
{
    sixel_palette_quant_engine_t const *engine;
    SIXELSTATUS status;

    engine = NULL;
    status = SIXEL_LOGIC_ERROR;

    if (prefer_float32 && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        engine = sixel_palette_quant_engine_lookup(
            SIXEL_QUANTIZE_MODEL_KCENTER,
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
        SIXEL_QUANTIZE_MODEL_KCENTER,
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

static SIXELSTATUS
sixel_palette_apply_kmedoids_engines(sixel_palette_t *palette,
                                     void const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     int prefer_float32)
{
    sixel_palette_quant_engine_t const *engine;
    SIXELSTATUS status;

    engine = NULL;
    status = SIXEL_LOGIC_ERROR;

    if (prefer_float32 && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        engine = sixel_palette_quant_engine_lookup(
            SIXEL_QUANTIZE_MODEL_KMEDOIDS,
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
        SIXEL_QUANTIZE_MODEL_KMEDOIDS,
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

static void
sixel_palette_dispose(sixel_palette_storage_t *storage)
{
    sixel_allocator_t *allocator;

    if (storage == NULL) {
        return;
    }

    allocator = storage->allocator;
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
    storage->build_context = NULL;
    sixel_allocator_ref(allocator);

    *object = &storage->palette_interface;
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

    return SIXEL_OK;
}

/*
 * sixel_palette_generate builds the palette entries inside the provided
 * sixel_palette_t instance.  The dispatcher consults the quantizer engine
 * registry to pick either the legacy 8bit solvers or (in later phases) the
 * RGBFLOAT32-enabled variants.  The high-level flow is:
 *
 *   1. Honour explicit K-means/K-center requests by running the preferred
 *      engine (float32 when available, otherwise the current 8bit solver).
 *      Failed runs fall back to Heckbert to preserve historical behaviour.
 *   2. Honour explicit K-medoids requests with the same fallback ladder.
 *   3. Invoke the median-cut/Heckbert engine for every other case, including
 *      the AUTO mode.
 *
 * Both branches share helper routines for cache management and post-processing
 * (for example reversible palette transformation).  The palette object tracks
 * the generated metadata so the caller can publish it without recomputing.
 */
static SIXELSTATUS
sixel_palette_vtbl_generate(
    sixel_palette_t *palette,
    sixel_palette_generate_request_t const *request)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_storage_t *storage;
    sixel_palette_build_context_t context;
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    sixel_allocator_t *work_allocator;

    storage = NULL;
    memset(&context, 0, sizeof(context));
    if (palette == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    storage = SIXEL_PALETTE_STORAGE(palette);
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
    context.temporal_reference_data = request->temporal_reference_data;
    context.temporal_reference_length = request->temporal_reference_length;
    context.temporal_reference_pixelformat =
        request->temporal_reference_pixelformat;
    context.temporal_match_weight = request->temporal_match_weight;
    context.method_for_largest = request->method_for_largest;
    context.method_for_rep = request->method_for_rep;
    context.quality_mode = request->quality_mode;
    context.force_palette = request->force_palette;
    context.use_reversible = request->use_reversible;
    context.quantize_model = request->quantize_model;
    context.final_merge_mode = request->final_merge_mode;
    context.lut_policy = request->lut_policy;
    storage->requested_colors = request->requested_colors;
    storage->build_context = &context;

    status = SIXEL_FALSE;

    if (context.quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = sixel_palette_apply_kmeans_engines(palette,
                                                    request->data,
                                                    request->length,
                                                    request->pixelformat,
                                                    work_allocator,
                                                    request->prefer_float32);
        if (SIXEL_SUCCEEDED(status)) {
            ncolors = storage->entry_count;
            origcolors = storage->original_colors;
            depth = (unsigned int)storage->depth;
            goto success;
        }
    } else if (context.quantize_model == SIXEL_QUANTIZE_MODEL_KCENTER) {
        status = sixel_palette_apply_kcenter_engines(palette,
                                                     request->data,
                                                     request->length,
                                                     request->pixelformat,
                                                     work_allocator,
                                                     request->prefer_float32);
        if (SIXEL_SUCCEEDED(status)) {
            ncolors = storage->entry_count;
            origcolors = storage->original_colors;
            depth = (unsigned int)storage->depth;
            goto success;
        }
    } else if (context.quantize_model == SIXEL_QUANTIZE_MODEL_KMEDOIDS) {
        status = sixel_palette_apply_kmedoids_engines(palette,
                                                      request->data,
                                                      request->length,
                                                      request->pixelformat,
                                                      work_allocator,
                                                      request->prefer_float32);
        if (SIXEL_SUCCEEDED(status)) {
            ncolors = storage->entry_count;
            origcolors = storage->original_colors;
            depth = (unsigned int)storage->depth;
            goto success;
        }
    } else if (context.quantize_model == SIXEL_QUANTIZE_MODEL_MEDIANCUT) {
        status = sixel_palette_apply_mediancut_engine(palette,
                                                      request->data,
                                                      request->length,
                                                      request->pixelformat,
                                                      work_allocator,
                                                      request->prefer_float32);
        goto after_quantizer;
    }

    status = sixel_palette_apply_mediancut_engine(palette,
                                                  request->data,
                                                  request->length,
                                                  request->pixelformat,
                                                  work_allocator,
                                                  request->prefer_float32);

after_quantizer:
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = storage->entry_count;
    origcolors = storage->original_colors;
    depth = (unsigned int)storage->depth;
    if (context.use_reversible && storage->entries != NULL) {
        sixel_palette_reversible_palette(storage->entries,
                                         ncolors,
                                         SIXEL_PIXELFORMAT_RGB888);
    }
    status = SIXEL_OK;

success:
    storage->entry_count = ncolors;
    storage->original_colors = origcolors;
    storage->depth = (int)depth;

end:
    storage->build_context = NULL;
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
