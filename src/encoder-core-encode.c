/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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
 * this file is derived from "sixel" original version (2014-3-2)
 * http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz
 *
 * Initial developer of this file is kmiya@culti.
 *
 * He distributes it under very permissive license which permits
 * using, copying, modification, redistribution, and all other
 * public activities without any restrictions.
 *
 * He declares this is compatible with MIT/BSD/GPL.
 *
 * Hayaki Saito (saitoha@me.com) modified this and re-licensed
 * it under the MIT license.
 *
 * Araki Ken added high-color encoding mode(sixel_encode_highcolor)
 * extension.
 *
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */

#include <sixel.h>
#include "compat_stub.h"
#include "encoder-core-private.h"
#include "encoder-core-packet.h"
#include "output-factory.h"
#include "dither.h"
#include "colorspace.h"
#include "options.h"
#include "pixelformat.h"
#include "palette-common-snap.h"
#include "timeline-logger.h"
#include "threading.h"
#include "encoder-core-highcolor.h"
#include "gpu-palette.h"
#include "planner.h"
#if SIXEL_ENABLE_THREADS
# include "sixel_atomic.h"
# include <6cells.h>
#endif

#define DCS_START_7BIT       "\033P"
#define DCS_START_7BIT_SIZE  (sizeof(DCS_START_7BIT) - 1)
#define DCS_START_8BIT       "\220"
#define DCS_START_8BIT_SIZE  (sizeof(DCS_START_8BIT) - 1)
#define DCS_END_7BIT         "\033\\"
#define DCS_END_7BIT_SIZE    (sizeof(DCS_END_7BIT) - 1)
#define DCS_END_8BIT         "\234"
#define DCS_END_8BIT_SIZE    (sizeof(DCS_END_8BIT) - 1)
#define DCS_7BIT(x)          DCS_START_7BIT x DCS_END_7BIT
#define DCS_8BIT(x)          DCS_START_8BIT x DCS_END_8BIT
#define SCREEN_PACKET_SIZE   256


typedef struct sixel_encode_work {
    char *map;
    size_t map_size;
    uint64_t *occupancy;
    size_t occupancy_words;
    uint64_t *band_columns;
    sixel_node_t **columns;
    size_t columns_size;
    unsigned char *active_colors;
    size_t active_colors_size;
    int *active_color_index;
    size_t active_color_index_size;
    int requested_threads;
} sixel_encode_work_t;

typedef struct sixel_band_state {
    int row_in_band;
    int fillable;
    /*
     * Size-policy fill must cover only source-backed rows.  row_in_band also
     * counts transparent offset padding, so using it directly would paint the
     * virtual top padding in a partial band.
     */
    int fill_mask;
    int active_color_count;
} sixel_band_state_t;

static int
sixel_encode_delta_is_legacy(sixel_dither_t const *dither,
                             sixel_output_t const *output);

static SIXELSTATUS
sixel_encode_capture_delta(sixel_dither_t *dither,
                            sixel_index_t const *indexes,
                            size_t pixel_count,
                            unsigned char const *palette,
                            float const *palette_float,
                            size_t palette_count,
                            sixel_output_t const *output);

#if SIXEL_ENABLE_THREADS
/*
 * Parallel execution relies on dedicated buffers per band and reusable
 * per-worker scratch spaces.  The main thread prepares the context and
 * pushes one job for each six-line band:
 *
 *   +------------------------+      enqueue jobs      +------------------+
 *   | main thread (producer) | ---------------------> | threadpool queue |
 *   +------------------------+                        +------------------+
 *            |                                                       |
 *            | allocate band buffers                                 |
 *            v                                                       v
 *   +------------------------+      execute callbacks      +-----------------+
 *   | per-worker workspace   | <-------------------------- | worker threads  |
 *   +------------------------+                             +-----------------+
 *            |
 *            | build SIXEL fragments
 *            v
 *   +------------------------+  ordered combination after join  +-----------+
 *   | band buffer array      | -------------------------------> | final I/O |
 *   +------------------------+                                  +-----------+
 */
typedef struct sixel_parallel_band_buffer {
    unsigned char *data;
    size_t size;
    size_t used;
    SIXELSTATUS status;
    int ready;
    int dispatched;
} sixel_parallel_band_buffer_t;

struct sixel_parallel_context;
typedef struct sixel_parallel_worker_state
    sixel_parallel_worker_state_t;

typedef struct sixel_parallel_context {
    sixel_index_t *pixels;
    int width;
    int height;
    int encoded_width;
    int encoded_height;
    int offset_left;
    int offset_top;
    int ncolors;
    int keycolor;
    unsigned char *palstate;
    int encode_policy;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    int thread_count;
    int band_count;
    sixel_parallel_band_buffer_t *bands;
    int *band_source_rows_total;
    int *band_source_rows_ready;
    sixel_parallel_worker_state_t **workers;
    int worker_capacity;
    int worker_registered;
    sixel_thread_pool_t *pool;
    sixel_timeline_logger_t *logger;
    sixel_mutex_t mutex;
    int mutex_ready;
    sixel_cond_t cond_band_ready;
    int cond_ready;
    sixel_thread_t writer_thread;
    int writer_started;
    int next_band_to_flush;
    int writer_should_stop;
    SIXELSTATUS writer_error;
    int queue_capacity;
    int pin_threads;
} sixel_parallel_context_t;

typedef struct sixel_parallel_row_notifier {
    sixel_parallel_context_t *context;
    sixel_timeline_logger_t *logger;
    int band_height;
    int image_height;
} sixel_parallel_row_notifier_t;

static void sixel_parallel_writer_stop(sixel_parallel_context_t *ctx,
                                       int force_abort);
static int sixel_parallel_writer_main(void *arg);
#endif

#if SIXEL_ENABLE_THREADS
static int sixel_parallel_jobs_allowed(sixel_parallel_context_t *ctx);
static void sixel_parallel_context_abort_locked(sixel_parallel_context_t *ctx,
                                               SIXELSTATUS status);
#endif

#if SIXEL_ENABLE_THREADS
struct sixel_parallel_worker_state {
    int initialized;
    int index;
    SIXELSTATUS writer_error;
    sixel_parallel_band_buffer_t *band_buffer;
    sixel_parallel_context_t *context;
    sixel_output_t *output;
    sixel_encode_work_t work;
    sixel_band_state_t band;
};
#endif

static void sixel_encode_work_init(sixel_encode_work_t *work);
static SIXELSTATUS sixel_encode_work_allocate(sixel_encode_work_t *work,
                                              int width,
                                              int ncolors,
                                              sixel_allocator_t *allocator);
static void sixel_encode_work_cleanup(sixel_encode_work_t *work,
                                      sixel_allocator_t *allocator);
static void sixel_band_state_reset(sixel_band_state_t *state);
static void sixel_band_finish(sixel_encode_work_t *work,
                              sixel_band_state_t *state, int width);
static int sixel_output_has_transparent_offset(sixel_output_t const *output);
static int sixel_count_sixel_bands(int height);
static void sixel_band_classify(sixel_encode_work_t *work,
                                 sixel_band_state_t *state,
                                 sixel_index_t const *pixels,
                                 int width,
                                 int height,
                                 int map_width,
                                 int band_start,
                                 int band_height,
                                 int offset_left,
                                 int offset_top,
                                 int ncolors,
                                 int keycolor,
                                 unsigned char const *palstate,
                                 int encode_policy,
                                 unsigned char const *mask);
static SIXELSTATUS sixel_band_compose(sixel_encode_work_t *work,
                                      sixel_band_state_t *state,
                                      sixel_output_t *output,
                                      int width,
                                      int ncolors,
                                      int keycolor,
                                      sixel_allocator_t *allocator);
static SIXELSTATUS sixel_band_emit(sixel_encode_work_t *work,
                                   sixel_band_state_t *state,
                                   sixel_output_t *output,
                                   int ncolors,
                                   int keycolor,
                                   int last_row_index);
static SIXELSTATUS sixel_put_flash(sixel_output_t *const output);
static void sixel_advance(sixel_output_t *output, int nwrite);
static int sixel_encode_body_ormode_nplanes(int ncolors);
static SIXELSTATUS
sixel_encode_body_ormode_emit_palette(unsigned char const *palette,
                                      float const *palette_float,
                                      int ncolors,
                                      int keycolor,
                                      sixel_output_t *output);
static SIXELSTATUS
sixel_encode_body_ormode_band(sixel_index_t const *pixels,
                              int width,
                              int height,
                              int band_index,
                              int nplanes,
                              sixel_output_t *output);
static SIXELSTATUS
sixel_encode_body_ormode_band_offset(sixel_index_t const *pixels,
                                     int width,
                                     int height,
                                     int band_index,
                                     int nplanes,
                                     int encoded_width,
                                     int encoded_height,
                                     int offset_left,
                                     int offset_top,
                                     sixel_output_t *output);

static void
sixel_timeline_logger_prepare_default(sixel_allocator_t *allocator,
                                      sixel_timeline_logger_t **logger)
{
    if (logger == NULL) {
        return;
    }

    *logger = NULL;
    (void)sixel_timeline_logger_prepare_env(allocator, logger);
}

static void
sixel_parallel_dither_configure(int height,
                                int total_threads,
                                size_t pixel_count,
                                sixel_dither_t const *dither,
                                sixel_dither_encode_allocation_t *allocation)
{
    sixel_dither_encode_request_t request = {0};

    request.height = height;
    request.total_threads = total_threads;
    request.ncolors = dither->ncolors;
    request.method_for_diffuse = dither->method_for_diffuse;
    request.prefer_staged = sixel_dither_encode_default_staged(
        total_threads, request.method_for_diffuse);
    request.gpu_encode_only = sixel_gpu_palette_policy_claims_apply_stage(
        dither->gpu_policy, dither->gpu_palette_threshold,
        dither->lut_policy, dither->method_for_diffuse,
        dither->method_for_scan, pixel_count);

    /* Resolve each request-bound control before consulting its registered
     * environment. The pure allocator sees identical precedence in the
     * early planner and this post-palette execution path. */
    if (dither->dither_parallel_threads_max_override != 0) {
        request.threads_max = dither->dither_parallel_threads_max;
        request.threads_max_set = 1;
    } else {
        request.threads_max_set =
            sixel_option_resolve_registered_uint_binding(
                SIXEL_OPTION_SCHEMA_DIFFUSION,
                NULL,
                SIXEL_SUBOPTION_BINDING_ID_2(
                    dither_parallel_threads_max,
                    dither_parallel_threads_max_override),
                &request.threads_max);
    }
    if (dither->dither_parallel_band_width_override != 0) {
        request.band_height = dither->dither_parallel_band_width;
        request.band_height_set = 1;
    } else {
        request.band_height_set =
            sixel_option_resolve_registered_uint_binding(
                SIXEL_OPTION_SCHEMA_DIFFUSION,
                NULL,
                SIXEL_SUBOPTION_BINDING_ID_2(
                    dither_parallel_band_width,
                    dither_parallel_band_width_override),
                &request.band_height);
    }
    if (dither->dither_parallel_band_overwrap_override != 0) {
        request.overlap = dither->dither_parallel_band_overwrap;
        request.overlap_set = 1;
    } else {
        request.overlap_set =
            sixel_option_resolve_registered_uint_binding(
                SIXEL_OPTION_SCHEMA_DIFFUSION,
                NULL,
                SIXEL_SUBOPTION_BINDING_ID_2(
                    dither_parallel_band_overwrap,
                    dither_parallel_band_overwrap_override),
                &request.overlap);
    }
    sixel_dither_encode_allocate(&request, allocation);
}

static int
sixel_output_has_transparent_offset(sixel_output_t const *output)
{
    if (output == NULL) {
        return 0;
    }

    return output->transparent_offset_left != 0 ||
           output->transparent_offset_top != 0;
}

static int
sixel_count_sixel_bands(int height)
{
    int bands;

    if (height <= 0) {
        return 0;
    }

    bands = height / 6;
    if ((height % 6) != 0) {
        bands += 1;
    }

    return bands;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_compute_transparent_extent(sixel_output_t const *output,
                                        int width,
                                        int height,
                                        int *encoded_width,
                                        int *encoded_height)
{
    int left;
    int top;

    if (output == NULL || encoded_width == NULL ||
        encoded_height == NULL || width < 1 || height < 1) {
        return SIXEL_BAD_ARGUMENT;
    }

    left = output->transparent_offset_left;
    top = output->transparent_offset_top;
    if (left < 0 || top < 0 ||
        left > INT_MAX - width || top > INT_MAX - height) {
        sixel_helper_set_additional_message(
            "transparent-offset makes encoded size overflow.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    *encoded_width = width + left;
    *encoded_height = height + top;
    return SIXEL_OK;
}

#if SIXEL_ENABLE_THREADS
static int sixel_parallel_band_writer(char *data, int size, void *priv);
static int sixel_parallel_worker_main(sixel_thread_pool_job_t job,
                                      void *userdata,
                                      void *workspace);
static SIXELSTATUS
sixel_parallel_context_begin(sixel_parallel_context_t *ctx,
                             sixel_index_t *pixels,
                             int width,
                             int height,
                             int ncolors,
                             int keycolor,
                             unsigned char *palstate,
                             sixel_output_t *output,
                             sixel_allocator_t *allocator,
                             int requested_threads,
                             int worker_capacity,
                             int queue_capacity,
                             int pin_threads,
                             sixel_timeline_logger_t *logger);
static SIXELSTATUS sixel_parallel_context_grow(sixel_parallel_context_t *ctx,
                                              int target_threads);
static void sixel_parallel_submit_band(sixel_parallel_context_t *ctx,
                                       int band_index);
static void
sixel_parallel_submit_source_empty_bands(sixel_parallel_context_t *ctx);
static SIXELSTATUS sixel_parallel_context_wait(sixel_parallel_context_t *ctx,
                                               int force_abort);
static void sixel_parallel_palette_row_ready(void *priv, int row_index);
static int
sixel_parallel_context_is_ormode(sixel_parallel_context_t const *ctx);
static SIXELSTATUS
sixel_parallel_worker_flush_output(sixel_parallel_worker_state_t *state);
static SIXELSTATUS sixel_encode_emit_palette(int bodyonly,
                          int ncolors,
                          int keycolor,
                          unsigned char const *palette,
                          float const *palette_float,
                          sixel_output_t *output);

static void
sixel_parallel_context_init(sixel_parallel_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->pixels = NULL;
    ctx->keycolor = (-1);
    ctx->encode_policy = SIXEL_ENCODEPOLICY_AUTO;
    ctx->writer_error = SIXEL_OK;
}

static int
sixel_parallel_context_is_ormode(sixel_parallel_context_t const *ctx)
{
    if (ctx == NULL || ctx->output == NULL) {
        return 0;
    }
    return ctx->output->ormode != 0 ? 1 : 0;
}

static void
sixel_parallel_worker_release_nodes(sixel_parallel_worker_state_t *state,
                                    sixel_allocator_t *allocator)
{
    sixel_node_t *np;

    if (state == NULL || state->output == NULL) {
        return;
    }

    while ((np = state->output->node_free) != NULL) {
        state->output->node_free = np->next;
        sixel_allocator_free(allocator, np);
    }
    state->output->node_top = NULL;
}

static void
sixel_parallel_worker_cleanup(sixel_parallel_worker_state_t *state,
                              sixel_allocator_t *allocator)
{
    if (state == NULL) {
        return;
    }
    sixel_parallel_worker_release_nodes(state, allocator);
    if (state->output != NULL) {
        sixel_output_unref(state->output);
        state->output = NULL;
    }
    sixel_encode_work_cleanup(&state->work, allocator);
    sixel_band_state_reset(&state->band);
    state->initialized = 0;
    state->index = 0;
    state->writer_error = SIXEL_OK;
    state->band_buffer = NULL;
    state->context = NULL;
}

static void
sixel_parallel_context_cleanup(sixel_parallel_context_t *ctx)
{
    int i;

    if (ctx->workers != NULL) {
        for (i = 0; i < ctx->worker_capacity; i++) {
            sixel_parallel_worker_cleanup(ctx->workers[i], ctx->allocator);
        }
        free(ctx->workers);
        ctx->workers = NULL;
    }
    sixel_parallel_writer_stop(ctx, 1);
    if (ctx->bands != NULL) {
        if (ctx->band_count < 0) {
            ctx->band_count = 0;
        }
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 6001)
#endif
        for (i = 0; i < ctx->band_count; i++) {
            free(ctx->bands[i].data);
            ctx->bands[i].data = NULL;
        }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        free(ctx->bands);
        ctx->bands = NULL;
    }
    free(ctx->band_source_rows_total);
    ctx->band_source_rows_total = NULL;
    free(ctx->band_source_rows_ready);
    ctx->band_source_rows_ready = NULL;
    ctx->band_count = 0;
    if (ctx->pool != NULL) {
        ctx->pool->vtbl->unref(ctx->pool);
        ctx->pool = NULL;
    }
    if (ctx->cond_ready) {
        sixel_cond_destroy(&ctx->cond_band_ready);
        ctx->cond_ready = 0;
    }
    if (ctx->mutex_ready) {
        sixel_mutex_destroy(&ctx->mutex);
        ctx->mutex_ready = 0;
    }
}

/*
 * Abort the pipeline when either a worker or the writer encounters an error.
 * The helper normalizes the `writer_error` bookkeeping so later callers see a
 * consistent stop request regardless of whether the mutex has been
 * initialized.
 */
static void
sixel_parallel_context_abort_locked(sixel_parallel_context_t *ctx,
                                    SIXELSTATUS status)
{
    if (ctx == NULL) {
        return;
    }
    if (!ctx->mutex_ready) {
        if (ctx->writer_error == SIXEL_OK) {
            ctx->writer_error = status;
        }
        ctx->writer_should_stop = 1;
        return;
    }

    sixel_mutex_lock(&ctx->mutex);
    if (ctx->writer_error == SIXEL_OK) {
        ctx->writer_error = status;
    }
    ctx->writer_should_stop = 1;
    sixel_cond_broadcast(&ctx->cond_band_ready);
    sixel_mutex_unlock(&ctx->mutex);
}

/*
 * Determine whether additional bands should be queued or executed.  The
 * producer and workers call this guard to avoid redundant work once the writer
 * decides to shut the pipeline down.
 */
static int
sixel_parallel_jobs_allowed(sixel_parallel_context_t *ctx)
{
    int accept;

    if (ctx == NULL) {
        return 0;
    }
    if (!ctx->mutex_ready) {
        if (ctx->writer_should_stop || ctx->writer_error != SIXEL_OK) {
            return 0;
        }
        return 1;
    }

    sixel_mutex_lock(&ctx->mutex);
    accept = (!ctx->writer_should_stop && ctx->writer_error == SIXEL_OK);
    sixel_mutex_unlock(&ctx->mutex);
    return accept;
}

static void
sixel_parallel_worker_reset(sixel_parallel_worker_state_t *state)
{
    if (state == NULL || state->output == NULL) {
        return;
    }

    sixel_band_state_reset(&state->band);
    /* Successful band finish clears map ranges before the next band.
     * Workspaces belong to this context and are freed even on failure, so
     * another encode cannot inherit them. Reset only band-local output. */
    state->writer_error = SIXEL_OK;
    state->output->pos = 0;
    state->output->save_count = 0;
    state->output->save_pixel = 0;
    state->output->active_palette = (-1);
    state->output->node_top = NULL;
    state->output->node_free = NULL;
}

static SIXELSTATUS
sixel_parallel_worker_prepare(sixel_parallel_worker_state_t *state,
                              sixel_parallel_context_t *ctx)
{
    SIXELSTATUS status;

    if (state->initialized) {
        return SIXEL_OK;
    }

    sixel_encode_work_init(&state->work);
    sixel_band_state_reset(&state->band);
    state->writer_error = SIXEL_OK;
    state->band_buffer = NULL;
    state->context = ctx;

    if (!sixel_parallel_context_is_ormode(ctx)) {
        status = sixel_encode_work_allocate(&state->work,
                                            ctx->encoded_width,
                                            ctx->ncolors,
                                            ctx->allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    status = sixel_encoder_core_create_output_from_factory(&state->output,
                                              sixel_parallel_band_writer,
                                              state,
                                              ctx->allocator);
    if (SIXEL_FAILED(status)) {
        if (!sixel_parallel_context_is_ormode(ctx)) {
            sixel_encode_work_cleanup(&state->work, ctx->allocator);
        }
        return status;
    }

    state->output->has_8bit_control = ctx->output->has_8bit_control;
    state->output->has_sixel_scrolling = ctx->output->has_sixel_scrolling;
    state->output->has_sdm_glitch = ctx->output->has_sdm_glitch;
    state->output->has_gri_arg_limit = ctx->output->has_gri_arg_limit;
    state->output->skip_dcs_envelope = 1;
    state->output->skip_header = 1;
    state->output->palette_type = ctx->output->palette_type;
    state->output->colorspace = ctx->output->colorspace;
    state->output->source_colorspace = ctx->output->source_colorspace;
    state->output->pixelformat = ctx->output->pixelformat;
    state->output->penetrate_multiplexer =
        ctx->output->penetrate_multiplexer;
    state->output->encode_policy = ctx->output->encode_policy;
    state->output->ormode = ctx->output->ormode;
    state->output->keycolor_reserved = ctx->output->keycolor_reserved;
    state->output->transparent_mask = ctx->output->transparent_mask;

    state->initialized = 1;
    state->index = (-1);

    if (ctx->mutex_ready) {
        sixel_mutex_lock(&ctx->mutex);
    }
    if (ctx->worker_registered < ctx->worker_capacity) {
        state->index = ctx->worker_registered;
        ctx->workers[state->index] = state;
        ctx->worker_registered += 1;
    }
    if (ctx->mutex_ready) {
        sixel_mutex_unlock(&ctx->mutex);
    }

    if (state->index < 0) {
        sixel_parallel_worker_cleanup(state, ctx->allocator);
        return SIXEL_RUNTIME_ERROR;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_parallel_context_grow(sixel_parallel_context_t *ctx, int target_threads)
{
    int capped_target;
    int delta;
    int status;

    if (ctx == NULL || ctx->pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    capped_target = target_threads;
    if (capped_target > ctx->worker_capacity) {
        capped_target = ctx->worker_capacity;
    }
    if (ctx->band_count > 0 && capped_target > ctx->band_count) {
        capped_target = ctx->band_count;
    }
    if (capped_target <= ctx->thread_count) {
        return SIXEL_OK;
    }

    delta = capped_target - ctx->thread_count;
    status = ctx->pool->vtbl->grow(ctx->pool, delta);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    ctx->thread_count += delta;

    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                          "controller",
                          "encode",
                          "grow_workers",
                          -1);
    }

    return SIXEL_OK;
}

static int
sixel_parallel_band_writer(char *data, int size, void *priv)
{
    sixel_parallel_worker_state_t *state;
    sixel_parallel_band_buffer_t *band;
    size_t required;
    size_t capacity;
    size_t new_capacity;
    unsigned char *tmp;

    state = (sixel_parallel_worker_state_t *)priv;
    if (state == NULL || data == NULL || size <= 0) {
        return size;
    }
    band = state->band_buffer;
    if (band == NULL) {
        state->writer_error = SIXEL_RUNTIME_ERROR;
        return size;
    }
    if (state->writer_error != SIXEL_OK) {
        return size;
    }

    required = band->used + (size_t)size;
    if (required < band->used) {
        state->writer_error = SIXEL_BAD_INTEGER_OVERFLOW;
        return size;
    }
    capacity = band->size;
    if (required > capacity) {
        if (capacity == 0) {
            new_capacity = (size_t)SIXEL_OUTPUT_PACKET_SIZE;
        } else {
            new_capacity = capacity;
        }
        while (new_capacity < required) {
            if (new_capacity > SIZE_MAX / 2) {
                new_capacity = required;
                break;
            }
            new_capacity *= 2;
        }
        tmp = (unsigned char *)realloc(band->data, new_capacity);
        if (tmp == NULL) {
            state->writer_error = SIXEL_BAD_ALLOCATION;
            return size;
        }
        band->data = tmp;
        band->size = new_capacity;
    }

    memcpy(band->data + band->used, data, (size_t)size);
    band->used += (size_t)size;

    return size;
}

static SIXELSTATUS
sixel_parallel_worker_flush_output(sixel_parallel_worker_state_t *state)
{
    if (state == NULL || state->output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (state->output->pos > 0) {
        state->writer_error = sixel_output_write_bytes(
            state->output,
            (char *)state->output->buffer,
            state->output->pos);
        state->output->pos = 0;
    }
    if (state->writer_error != SIXEL_OK) {
        return state->writer_error;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_parallel_create_pool(sixel_thread_pool_t **pool,
                           int threads,
                           int queue_depth,
                           size_t workspace_size,
                           sixel_thread_pool_worker_function_t worker,
                           void *userdata)
{
    sixel_threadpool_service_t *service;
    sixel_thread_pool_create_request_t request;
    void *service_object;
    SIXELSTATUS status;

    if (pool != NULL) {
        *pool = NULL;
    }
    if (pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    service = NULL;
    service_object = NULL;
    status = sixel_components_getservice("services/threadpool",
                                         &service_object);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    service = (sixel_threadpool_service_t *)service_object;
    if (service == NULL || service->vtbl == NULL ||
        service->vtbl->create_pool == NULL) {
        if (service != NULL && service->vtbl != NULL &&
            service->vtbl->unref != NULL) {
            service->vtbl->unref(service);
        }
        return SIXEL_BAD_ARGUMENT;
    }

    request.threads = threads;
    request.queue_size = queue_depth;
    request.workspace_size = workspace_size;
    request.worker = worker;
    request.userdata = userdata;
    request.workspace_cleanup = NULL;
    status = service->vtbl->create_pool(service, &request, pool);
    if (service->vtbl->unref != NULL) {
        service->vtbl->unref(service);
    }

    return status;
}

static void
sixel_parallel_prepare_band_source_rows(sixel_parallel_context_t *ctx)
{
    int band_index;
    int band_start;
    int band_end;
    int source_start;
    int source_end;

    if (ctx == NULL || ctx->band_source_rows_total == NULL ||
            ctx->band_source_rows_ready == NULL) {
        return;
    }

    /*
     * PaletteApply reports readiness in source-image coordinates.  The encoder
     * consumes fixed six-line bands in encoded-image coordinates, and a
     * transparent offset shifts those two spaces apart.  Precomputing the
     * source rows required by each encoded band lets the producer dispatch only
     * after every row that band can read has actually been written.
     */
    for (band_index = 0; band_index < ctx->band_count; band_index++) {
        band_start = band_index * 6;
        band_end = ctx->encoded_height;
        if (band_start <= ctx->encoded_height - 6) {
            band_end = band_start + 6;
        }

        source_start = band_start - ctx->offset_top;
        source_end = band_end - ctx->offset_top;
        if (source_start < 0) {
            source_start = 0;
        }
        if (source_start > ctx->height) {
            source_start = ctx->height;
        }
        if (source_end < 0) {
            source_end = 0;
        }
        if (source_end > ctx->height) {
            source_end = ctx->height;
        }

        ctx->band_source_rows_total[band_index] = source_end - source_start;
        ctx->band_source_rows_ready[band_index] = 0;
    }
}

static void
sixel_parallel_submit_source_empty_bands(sixel_parallel_context_t *ctx)
{
    int band_index;

    if (ctx == NULL || ctx->band_source_rows_total == NULL) {
        return;
    }

    /*
     * A positive top offset can create leading bands that contain only
     * transparent padding.  No PaletteApply row can ever close those bands, so
     * enqueue them explicitly before waiting for source-row notifications.
     */
    for (band_index = 0; band_index < ctx->band_count; band_index++) {
        if (ctx->band_source_rows_total[band_index] == 0) {
            sixel_parallel_submit_band(ctx, band_index);
        }
    }
}

static SIXELSTATUS
sixel_parallel_context_begin(sixel_parallel_context_t *ctx,
                             sixel_index_t *pixels,
                             int width,
                             int height,
                             int ncolors,
                             int keycolor,
                             unsigned char *palstate,
                             sixel_output_t *output,
                             sixel_allocator_t *allocator,
                             int requested_threads,
                             int worker_capacity,
                             int queue_capacity,
                             int pin_threads,
                             sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    int nbands;
    int encoded_width;
    int encoded_height;
    int threads;
    int i;

    if (ctx == NULL || pixels == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    ctx->pixels = pixels;
    ctx->width = width;
    ctx->height = height;
    ctx->encoded_width = encoded_width;
    ctx->encoded_height = encoded_height;
    ctx->offset_left = output->transparent_offset_left;
    ctx->offset_top = output->transparent_offset_top;
    ctx->ncolors = ncolors;
    ctx->keycolor = keycolor;
    ctx->palstate = palstate;
    ctx->encode_policy = output->encode_policy;
    ctx->allocator = allocator;
    ctx->output = output;
    ctx->logger = logger;
    ctx->pin_threads = (pin_threads != 0) ? 1 : 0;
    ctx->bands = NULL;
    ctx->band_count = 0;
    ctx->workers = NULL;
    ctx->worker_capacity = 0;
    ctx->band_source_rows_total = NULL;
    ctx->band_source_rows_ready = NULL;

    nbands = sixel_count_sixel_bands(encoded_height);
    if (nbands <= 0) {
        return SIXEL_OK;
    }
    threads = requested_threads;
    if (threads > nbands) {
        threads = nbands;
    }
    if (threads < 1) {
        threads = 1;
    }
    ctx->thread_count = threads;
    if (worker_capacity < threads) {
        worker_capacity = threads;
    }
    if (worker_capacity > nbands) {
        worker_capacity = nbands;
    }
    ctx->worker_capacity = worker_capacity;

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "controller",
                          "encode",
                          "context_begin",
                          -1);
    }

    ctx->bands = (sixel_parallel_band_buffer_t *)calloc((size_t)nbands,
                                                        sizeof(*ctx->bands));
    if (ctx->bands == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (i = 0; i < nbands; ++i) {
        ctx->bands[i].data = NULL;
        ctx->bands[i].size = 0;
        ctx->bands[i].used = 0;
        ctx->bands[i].status = SIXEL_OK;
        ctx->bands[i].ready = 0;
        ctx->bands[i].dispatched = 0;
    }
    ctx->band_count = nbands;

    ctx->band_source_rows_total = (int *)calloc((size_t)nbands,
                                                sizeof(int));
    if (ctx->band_source_rows_total == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    ctx->band_source_rows_ready = (int *)calloc((size_t)nbands,
                                                sizeof(int));
    if (ctx->band_source_rows_ready == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_parallel_prepare_band_source_rows(ctx);

    ctx->workers = (sixel_parallel_worker_state_t **)
        calloc((size_t)ctx->worker_capacity, sizeof(*ctx->workers));
    if (ctx->workers == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_mutex_init(&ctx->mutex);
    if (status != SIXEL_OK) {
        return status;
    }
    ctx->mutex_ready = 1;

    status = sixel_cond_init(&ctx->cond_band_ready);
    if (status != SIXEL_OK) {
        return status;
    }
    ctx->cond_ready = 1;

    ctx->queue_capacity = queue_capacity;
    if (ctx->queue_capacity < 1) {
        ctx->queue_capacity = nbands;
    }
    if (ctx->queue_capacity > nbands) {
        ctx->queue_capacity = nbands;
    }

    status = sixel_parallel_create_pool(
        &ctx->pool,
        threads,
        ctx->queue_capacity,
        sizeof(sixel_parallel_worker_state_t),
        sixel_parallel_worker_main,
        ctx);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    ctx->pool->vtbl->set_affinity(ctx->pool, ctx->pin_threads);

    /* Initialize writer-visible fields before the writer thread starts.
     * Serialize initialization of writer-visible state so the writer thread
     * cannot observe partially initialized fields on startup.
     */
    sixel_mutex_lock(&ctx->mutex);
    ctx->next_band_to_flush = 0;
    ctx->writer_should_stop = 0;
    ctx->writer_error = SIXEL_OK;

    status = sixel_thread_create(&ctx->writer_thread,
                                 sixel_parallel_writer_main,
                                 ctx);
    if (SIXEL_FAILED(status)) {
        sixel_mutex_unlock(&ctx->mutex);
        return status;
    }
    ctx->writer_started = 1;
    sixel_mutex_unlock(&ctx->mutex);

    return SIXEL_OK;
}

static void
sixel_parallel_submit_band(sixel_parallel_context_t *ctx, int band_index)
{
    sixel_thread_pool_job_t job;
    SIXELSTATUS status;
    int dispatch;

    if (ctx == NULL || ctx->pool == NULL) {
        return;
    }
    if (band_index < 0 || band_index >= ctx->band_count) {
        return;
    }

    dispatch = 0;
    /*
     * Multiple producers may notify the same band when PaletteApply runs in
     * parallel.  Guard the dispatched flag so only the first notifier pushes
     * work into the encoder queue.
     */
    if (ctx->mutex_ready) {
        sixel_mutex_lock(&ctx->mutex);
        if (!ctx->bands[band_index].dispatched
                && !ctx->writer_should_stop
                && ctx->writer_error == SIXEL_OK) {
            ctx->bands[band_index].dispatched = 1;
            dispatch = 1;
        }
        sixel_mutex_unlock(&ctx->mutex);
    } else {
        if (!ctx->bands[band_index].dispatched
                && sixel_parallel_jobs_allowed(ctx)) {
            ctx->bands[band_index].dispatched = 1;
            dispatch = 1;
        }
    }

    if (!dispatch) {
        return;
    }

    sixel_fence_release();
    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                          "controller",
                          "encode",
                          "dispatch",
                          band_index);
    }
    job.band_index = band_index;
    status = ctx->pool->vtbl->push(ctx->pool, job);
    if (SIXEL_FAILED(status)) {
        if (ctx->mutex_ready) {
            sixel_mutex_lock(&ctx->mutex);
            sixel_parallel_context_abort_locked(ctx, status);
            sixel_mutex_unlock(&ctx->mutex);
        } else {
            ctx->writer_error = status;
        }
    }
}

static SIXELSTATUS
sixel_parallel_context_wait(sixel_parallel_context_t *ctx, int force_abort)
{
    int pool_error;

    if (ctx == NULL || ctx->pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    ctx->pool->vtbl->finish(ctx->pool);
    pool_error = ctx->pool->vtbl->get_error(ctx->pool);
    sixel_parallel_writer_stop(ctx, force_abort || pool_error != SIXEL_OK);

    if (pool_error != SIXEL_OK) {
        return pool_error;
    }
    if (ctx->writer_error != SIXEL_OK) {
        return ctx->writer_error;
    }

    return SIXEL_OK;
}

/*
 * Producer callback invoked after PaletteApply finishes a scanline.  The
 * helper promotes every sixth row (or the final partial band) into the job
 * queue so workers can begin encoding while dithering continues.
 */
static void
sixel_parallel_palette_row_ready(void *priv, int row_index)
{
    sixel_parallel_row_notifier_t *notifier;
    sixel_parallel_context_t *ctx;
    sixel_timeline_logger_t *logger;
    int band_height;
    int band_index;
    int ready_row;
    int should_dispatch;
    int rows_ready;
    int rows_total;

    notifier = (sixel_parallel_row_notifier_t *)priv;
    if (notifier == NULL) {
        return;
    }
    ctx = notifier->context;
    logger = notifier->logger;
    if (ctx == NULL || ctx->band_count <= 0 || ctx->height <= 0) {
        return;
    }
    if (row_index < 0 || row_index >= ctx->height) {
        return;
    }
    band_height = notifier->band_height;
    if (band_height < 1) {
        band_height = 6;
    }

    /*
     * Dither workers may complete source-row ranges out of order.  A shifted
     * encoded band can also straddle two dither bands, so the last source row
     * seen by one worker is not enough to prove the encoded band is complete.
     * Count source-row arrivals per encoded six-line band and dispatch only
     * when every source row for that encoded band is present.
     */
    ready_row = ctx->offset_top + row_index;
    band_index = ready_row / band_height;
    if (band_index >= ctx->band_count) {
        band_index = ctx->band_count - 1;
    }
    if (band_index < 0) {
        return;
    }

    should_dispatch = 0;
    if (ctx->band_source_rows_total != NULL &&
            ctx->band_source_rows_ready != NULL) {
        if (ctx->mutex_ready) {
            sixel_mutex_lock(&ctx->mutex);
        }
        rows_total = ctx->band_source_rows_total[band_index];
        rows_ready = ctx->band_source_rows_ready[band_index];
        if (rows_total > 0 && rows_ready < rows_total) {
            rows_ready += 1;
            ctx->band_source_rows_ready[band_index] = rows_ready;
            if (rows_ready == rows_total) {
                should_dispatch = 1;
            }
        }
        if (ctx->mutex_ready) {
            sixel_mutex_unlock(&ctx->mutex);
        }
    } else if ((ready_row % band_height) == band_height - 1 ||
            row_index == ctx->height - 1) {
        should_dispatch = 1;
    }

    if (!should_dispatch) {
        return;
    }

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "controller",
                          "encode",
                          "row_gate",
                          band_index);
    }

    sixel_parallel_submit_band(ctx, band_index);
}

static SIXELSTATUS
sixel_parallel_flush_band(sixel_parallel_context_t *ctx, int band_index)
{
    sixel_parallel_band_buffer_t *band;
    size_t offset;
    size_t chunk;

    band = &ctx->bands[band_index];
    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                          "worker",
                          "encode",
                          "writer_flush",
                          band_index);
    }
    offset = 0;
    while (offset < band->used) {
        chunk = band->used - offset;
        if (chunk > (size_t)(SIXEL_OUTPUT_PACKET_SIZE - ctx->output->pos)) {
            chunk = (size_t)(SIXEL_OUTPUT_PACKET_SIZE - ctx->output->pos);
        }
        memcpy(ctx->output->buffer + ctx->output->pos,
               band->data + offset,
               chunk);
        sixel_advance(ctx->output, (int)chunk);
        offset += chunk;
    }
    return SIXEL_OK;
}

static int
sixel_parallel_worker_main(sixel_thread_pool_job_t job,
                           void *userdata,
                           void *workspace)
{
    sixel_parallel_context_t *ctx;
    sixel_parallel_worker_state_t *state;
    sixel_parallel_band_buffer_t *band;
    SIXELSTATUS status;
    int band_index;
    int band_start;
    int band_height;
    int last_row_index;
    int nplanes;

    ctx = (sixel_parallel_context_t *)userdata;
    state = (sixel_parallel_worker_state_t *)workspace;

    if (ctx == NULL || state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    band = NULL;
    status = SIXEL_OK;
    band_index = job.band_index;
    band_start = 0;
    band_height = 0;
    last_row_index = -1;
    if (band_index < 0 || band_index >= ctx->band_count) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    band = &ctx->bands[band_index];
    if (ctx->mutex_ready) {
        /* Synchronize band state reset with the writer thread. */
        sixel_mutex_lock(&ctx->mutex);
        band->used = 0;
        band->status = SIXEL_OK;
        band->ready = 0;
        sixel_mutex_unlock(&ctx->mutex);
    } else {
        band->used = 0;
        band->status = SIXEL_OK;
        band->ready = 0;
    }

    sixel_fence_acquire();

    status = sixel_parallel_worker_prepare(state, ctx);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (!sixel_parallel_jobs_allowed(ctx)) {
        if (ctx->mutex_ready) {
            sixel_mutex_lock(&ctx->mutex);
            if (ctx->writer_error != SIXEL_OK) {
                status = ctx->writer_error;
            } else {
                status = SIXEL_RUNTIME_ERROR;
            }
            sixel_mutex_unlock(&ctx->mutex);
        } else if (ctx->writer_error != SIXEL_OK) {
            status = ctx->writer_error;
        } else {
            status = SIXEL_RUNTIME_ERROR;
        }
        goto cleanup;
    }

    state->band_buffer = band;
    sixel_parallel_worker_reset(state);

    band_start = band_index * 6;
    band_height = ctx->encoded_height - band_start;
    if (band_height > 6) {
        band_height = 6;
    }
    if (band_height <= 0) {
        goto cleanup;
    }

    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                          "worker",
                          "encode",
                          "worker_start",
                          band_index);
    }

    if (sixel_parallel_context_is_ormode(ctx)) {
        nplanes = sixel_encode_body_ormode_nplanes(ctx->ncolors);
        status = sixel_encode_body_ormode_band(ctx->pixels,
                                               ctx->width,
                                               ctx->height,
                                               band_index,
                                               nplanes,
                                               state->output);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_parallel_worker_flush_output(state);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        goto done;
    }

    sixel_band_classify(&state->work,
                         &state->band,
                         ctx->pixels,
                         ctx->width,
                         ctx->height,
                         ctx->encoded_width,
                         band_start,
                         band_height,
                         ctx->offset_left,
                         ctx->offset_top,
                         ctx->ncolors,
                         ctx->keycolor,
                         ctx->palstate,
                         ctx->encode_policy,
                         state->output->transparent_mask);

    status = sixel_band_compose(&state->work,
                                &state->band,
                                state->output,
                                ctx->encoded_width,
                                ctx->ncolors,
                                ctx->keycolor,
                                ctx->allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    last_row_index = band_start + band_height - 1;
    status = sixel_band_emit(&state->work,
                             &state->band,
                             state->output,
                             ctx->ncolors,
                             ctx->keycolor,
                             last_row_index);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_put_flash(state->output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_parallel_worker_flush_output(state);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_band_finish(&state->work, &state->band, ctx->encoded_width);
done:
    status = SIXEL_OK;

cleanup:
    sixel_parallel_worker_release_nodes(state, ctx->allocator);
    if (band != NULL && ctx->mutex_ready && ctx->cond_ready) {
        sixel_fence_release();
        sixel_mutex_lock(&ctx->mutex);
        band->status = status;
        band->ready = 1;
        sixel_cond_broadcast(&ctx->cond_band_ready);
        sixel_mutex_unlock(&ctx->mutex);
    }
    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                          "worker",
                          "encode",
                          "worker_done",
                          band_index);
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }
    return SIXEL_OK;
}

static void
sixel_parallel_writer_stop(sixel_parallel_context_t *ctx, int force_abort)
{
    int should_signal;

    if (ctx == NULL || !ctx->writer_started) {
        return;
    }

    should_signal = ctx->mutex_ready && ctx->cond_ready;
    if (should_signal) {
        sixel_mutex_lock(&ctx->mutex);
        if (force_abort) {
            ctx->writer_should_stop = 1;
        }
        sixel_cond_broadcast(&ctx->cond_band_ready);
        sixel_mutex_unlock(&ctx->mutex);
    } else if (force_abort) {
        ctx->writer_should_stop = 1;
    }

    sixel_thread_join(&ctx->writer_thread);
    ctx->writer_started = 0;
    ctx->writer_should_stop = 0;
    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                          "writer",
                          "encode",
                          "writer_stop",
                          -1);
    }
}

static int
sixel_parallel_writer_main(void *arg)
{
    sixel_parallel_context_t *ctx;
    sixel_parallel_band_buffer_t *band;
    SIXELSTATUS status;
    int band_index;

    ctx = (sixel_parallel_context_t *)arg;
    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (ctx->logger != NULL) {
        sixel_timeline_logger_logf(ctx->logger,
                                   "writer",
                                   "encode",
                                   "writer_start",
                                   -1);
    }

    for (;;) {
        sixel_mutex_lock(&ctx->mutex);
        while (!ctx->writer_should_stop &&
               ctx->next_band_to_flush < ctx->band_count) {
    band_index = ctx->next_band_to_flush;
    band = &ctx->bands[band_index];
    if (band->ready) {
        break;
    }
            sixel_cond_wait(&ctx->cond_band_ready, &ctx->mutex);
        }

        if (ctx->writer_should_stop) {
            sixel_mutex_unlock(&ctx->mutex);
            break;
        }

        if (ctx->next_band_to_flush >= ctx->band_count) {
            sixel_mutex_unlock(&ctx->mutex);
            break;
        }

        band_index = ctx->next_band_to_flush;
        band = &ctx->bands[band_index];
        if (!band->ready) {
            sixel_mutex_unlock(&ctx->mutex);
            continue;
        }
        band->ready = 0;
        ctx->next_band_to_flush += 1;
        sixel_mutex_unlock(&ctx->mutex);

        sixel_fence_acquire();
        status = band->status;
        if (ctx->logger != NULL) {
            sixel_timeline_logger_logf(ctx->logger,
                              "writer",
                              "encode",
                              "writer_dequeue",
                              band_index);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_parallel_flush_band(ctx, band_index);
        }
        if (SIXEL_FAILED(status)) {
            sixel_parallel_context_abort_locked(ctx, status);
            break;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_body_parallel(sixel_index_t *pixels,
                           int width,
                           int height,
                           int ncolors,
                           int keycolor,
                           sixel_output_t *output,
                           unsigned char *palstate,
                           sixel_allocator_t *allocator,
                           int requested_threads,
                           int pin_threads)
{
    sixel_parallel_context_t ctx = {0};
    SIXELSTATUS status;
    int nbands;
    int threads;
    int i;
    int queue_depth;
    int encoded_width;
    int encoded_height;
    sixel_timeline_logger_t *logger;

    sixel_parallel_context_init(&ctx);
    sixel_timeline_logger_prepare_default(allocator, &logger);
    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        sixel_timeline_logger_unref(logger);
        return status;
    }
    (void)encoded_width;
    nbands = sixel_count_sixel_bands(encoded_height);
    if (nbands <= 0) {
        sixel_timeline_logger_unref(logger);
        return SIXEL_OK;
    }

    threads = requested_threads;
    if (threads > nbands) {
        threads = nbands;
    }
    if (threads < 1) {
        threads = 1;
    }
    ctx.thread_count = threads;
    queue_depth = threads * 3;
    if (queue_depth > nbands) {
        queue_depth = nbands;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    status = sixel_parallel_context_begin(&ctx,
                                          pixels,
                                          width,
                                          height,
                                          ncolors,
                                          keycolor,
                                          palstate,
                                          output,
                                          allocator,
                                          threads,
                                          threads,
                                          queue_depth,
                                          pin_threads,
                                          logger);
    if (SIXEL_FAILED(status)) {
        sixel_parallel_context_cleanup(&ctx);
        sixel_timeline_logger_unref(logger);
        return status;
    }

    for (i = 0; i < nbands; i++) {
        sixel_parallel_submit_band(&ctx, i);
    }

    status = sixel_parallel_context_wait(&ctx, 0);
    if (SIXEL_FAILED(status)) {
        sixel_parallel_context_cleanup(&ctx);
        sixel_timeline_logger_unref(logger);
        return status;
    }

    sixel_parallel_context_cleanup(&ctx);
    sixel_timeline_logger_unref(logger);
    return SIXEL_OK;
}
#endif

#if SIXEL_ENABLE_THREADS
/*
 * Execute PaletteApply, band encoding, and output emission as a pipeline.
 * The producer owns the dithered index buffer and enqueues bands once every
 * six rows have been produced.  Worker threads encode in parallel while the
 * writer emits completed bands in-order to preserve deterministic output.
 */
static SIXELSTATUS
sixel_encode_body_pipeline(unsigned char *pixels,
                           int width,
                           int height,
                           unsigned char const *palette,
                           float const *palette_float,
                           sixel_dither_t *dither,
                           sixel_output_t *output,
                           int encode_threads)
{
    SIXELSTATUS status;
    SIXELSTATUS wait_status;
    sixel_parallel_context_t ctx = {0};
    sixel_index_t *indexes;
    sixel_index_t *result;
    sixel_allocator_t *allocator;
    size_t pixel_count;
    size_t buffer_size;
    int threads;
    int nbands;
    int queue_depth;
    int waited;
    int dither_threads_budget;
    int worker_capacity;
    int boost_target;
    int encoded_width;
    int encoded_height;
    sixel_timeline_logger_t *logger;
    int owns_logger;
    sixel_parallel_row_notifier_t notifier;

    if (pixels == NULL
            || (palette == NULL && palette_float == NULL)
            || dither == NULL
            || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    threads = encode_threads;
    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    (void)encoded_width;
    nbands = sixel_count_sixel_bands(encoded_height);
    if (threads <= 1 || nbands <= 1) {
        return SIXEL_RUNTIME_ERROR;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (height != 0 && pixel_count / (size_t)height != (size_t)width) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    buffer_size = pixel_count * sizeof(*indexes);
    allocator = dither->allocator;
    indexes = (sixel_index_t *)sixel_allocator_malloc(allocator, buffer_size);
    if (indexes == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_parallel_context_init(&ctx);
    logger = dither->pipeline_logger;
    owns_logger = 0;
    if (logger == NULL) {
        logger = NULL;
        sixel_timeline_logger_prepare_default(allocator, &logger);
        owns_logger = logger != NULL ? 1 : 0;
    }
    /* The local logger remains valid until the entire pipeline is drained. */
    sixel_timeline_logger_logf(logger,
        "controller", "encode", "body_start", -1);
    notifier.context = &ctx;
    notifier.logger = logger;
    notifier.band_height = 6;
    notifier.image_height = encoded_height;
    waited = 0;
    status = SIXEL_OK;

    status = sixel_encode_emit_palette(dither->bodyonly,
                                       dither->ncolors,
                                       dither->keycolor,
                                       palette,
                                       palette_float,
                                       output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    queue_depth = threads * 3;
    if (queue_depth > nbands) {
        queue_depth = nbands;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    dither_threads_budget = dither->pipeline_dither_threads;
    worker_capacity = threads + dither_threads_budget;
    if (worker_capacity < threads) {
        worker_capacity = threads;
    }
    if (worker_capacity > nbands) {
        worker_capacity = nbands;
    }

    dither->pipeline_index_buffer = indexes;
    dither->pipeline_index_size = buffer_size;
    dither->pipeline_row_callback = sixel_parallel_palette_row_ready;
    dither->pipeline_row_priv = &notifier;
    dither->pipeline_logger = logger;
    dither->pipeline_image_width = width;
    dither->pipeline_image_height = height;

    if (logger != NULL) {
        /* Mark row-pipeline setup before spawning workers. Resolved counts
         * and geometry belong to the post-encode dither_contract trace;
         * this JSONL logger records structural event fields only. */
        sixel_timeline_logger_logf(logger,
                          "controller",
                          "pipeline",
                          "configure",
                          -1);
    }

    status = sixel_parallel_context_begin(&ctx,
                                          indexes,
                                          width,
                                          height,
                                          dither->ncolors,
                                          dither->keycolor,
                                          NULL,
                                          output,
                                          allocator,
                                          threads,
                                          worker_capacity,
                                          queue_depth,
                                          dither->pipeline_pin_threads,
                                          logger);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_parallel_submit_source_empty_bands(&ctx);

    result = sixel_dither_apply_palette(dither, pixels, width, height);
    if (result == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }
    if (result != indexes) {
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }
    if (dither->pipeline_accumulation_result_enabled != 0) {
        status = sixel_encode_capture_delta(
            dither, indexes, pixel_count, palette, palette_float,
            (size_t)dither->ncolors, output);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    /*
     * All dithering work has finished at this point.  Reclaim the idle dither
     * workers for encoding so the tail of the pipeline drains with additional
     * parallelism instead of leaving those CPU resources unused.
     */
    boost_target = threads + dither_threads_budget;
    status = sixel_parallel_context_grow(&ctx, boost_target);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_parallel_context_wait(&ctx, 0);
    waited = 1;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

cleanup:
    dither->pipeline_row_callback = NULL;
    dither->pipeline_row_priv = NULL;
    dither->pipeline_index_buffer = NULL;
    dither->pipeline_index_size = 0;
    dither->pipeline_image_width = 0;
    dither->pipeline_image_height = 0;
    dither->pipeline_transparent_mask = NULL;
    dither->pipeline_transparent_mask_size = 0;
    dither->pipeline_transparent_keycolor = (-1);
    dither->pipeline_accumulation_result_enabled = 0;
    if (!waited && ctx.pool != NULL) {
        wait_status = sixel_parallel_context_wait(&ctx, status != SIXEL_OK);
        if (status == SIXEL_OK) {
            status = wait_status;
        }
    }
    sixel_parallel_context_cleanup(&ctx);
    sixel_timeline_logger_logf(logger,
        "controller", "encode", "body_finish", -1);
    if (owns_logger) {
        sixel_timeline_logger_unref(logger);
    }
    if (indexes != NULL) {
        sixel_allocator_free(allocator, indexes);
    }
    return status;
}
#else
static SIXELSTATUS
sixel_encode_body_pipeline(unsigned char *pixels,
                           int width,
                           int height,
                           unsigned char const *palette,
                           float const *palette_float,
                           sixel_dither_t *dither,
                           sixel_output_t *output,
                           int encode_threads)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)palette;
    (void)palette_float;
    (void)dither;
    (void)output;
    (void)encode_threads;
    return SIXEL_RUNTIME_ERROR;
}
#endif

#if SIXEL_ENABLE_THREADS
/*
 * OR mode shares the producer/writer pipeline with the normal encoder, but its
 * worker body emits bit-planes from the finished palette-index rows directly.
 * The palette is emitted before workers start so every band can stay
 * independent and the writer only has to concatenate ordered body fragments.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encode_body_ormode_pipeline(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *palette,
                                  float const *palette_float,
                                  sixel_dither_t *dither,
                                  sixel_output_t *output,
                                  int encode_threads)
{
    SIXELSTATUS status;
    SIXELSTATUS wait_status;
    sixel_parallel_context_t ctx = {0};
    sixel_index_t *indexes;
    sixel_index_t *result;
    sixel_allocator_t *allocator;
    size_t pixel_count;
    size_t buffer_size;
    int threads;
    int nbands;
    int queue_depth;
    int waited;
    int dither_threads_budget;
    int worker_capacity;
    int boost_target;
    sixel_timeline_logger_t *logger;
    int owns_logger;
    sixel_parallel_row_notifier_t notifier;

    if (pixels == NULL || palette == NULL || dither == NULL ||
            output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    threads = encode_threads;
    nbands = (height + 5) / 6;
    if (threads <= 1 || nbands <= 1) {
        return SIXEL_RUNTIME_ERROR;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (height != 0 && pixel_count / (size_t)height != (size_t)width) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    buffer_size = pixel_count * sizeof(*indexes);
    allocator = dither->allocator;
    indexes = (sixel_index_t *)sixel_allocator_malloc(allocator, buffer_size);
    if (indexes == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_parallel_context_init(&ctx);
    logger = dither->pipeline_logger;
    owns_logger = 0;
    if (logger == NULL) {
        logger = NULL;
        sixel_timeline_logger_prepare_default(allocator, &logger);
        owns_logger = logger != NULL ? 1 : 0;
    }
    /* The local logger remains valid until the entire pipeline is drained. */
    sixel_timeline_logger_logf(logger,
        "controller", "encode", "body_start", -1);
    notifier.context = &ctx;
    notifier.logger = logger;
    notifier.band_height = 6;
    notifier.image_height = height;
    waited = 0;
    status = SIXEL_OK;

    status = sixel_encode_body_ormode_emit_palette(palette,
                                                   palette_float,
                                                   dither->ncolors,
                                                   dither->keycolor,
                                                   output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    queue_depth = threads * 3;
    if (queue_depth > nbands) {
        queue_depth = nbands;
    }
    if (queue_depth < 1) {
        queue_depth = 1;
    }

    dither_threads_budget = dither->pipeline_dither_threads;
    worker_capacity = threads + dither_threads_budget;
    if (worker_capacity < threads) {
        worker_capacity = threads;
    }
    if (worker_capacity > nbands) {
        worker_capacity = nbands;
    }

    dither->pipeline_index_buffer = indexes;
    dither->pipeline_index_size = buffer_size;
    dither->pipeline_row_callback = sixel_parallel_palette_row_ready;
    dither->pipeline_row_priv = &notifier;
    dither->pipeline_logger = logger;
    dither->pipeline_image_width = width;
    dither->pipeline_image_height = height;

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "controller",
                          "pipeline",
                          "configure",
                          -1);
    }

    status = sixel_parallel_context_begin(&ctx,
                                          indexes,
                                          width,
                                          height,
                                          dither->ncolors,
                                          dither->keycolor,
                                          NULL,
                                          output,
                                          allocator,
                                          threads,
                                          worker_capacity,
                                          queue_depth,
                                          dither->pipeline_pin_threads,
                                          logger);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_parallel_submit_source_empty_bands(&ctx);

    result = sixel_dither_apply_palette(dither, pixels, width, height);
    if (result == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }
    if (result != indexes) {
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }
    if (dither->pipeline_accumulation_result_enabled != 0) {
        status = sixel_encode_capture_delta(
            dither, indexes, pixel_count, palette, palette_float,
            (size_t)dither->ncolors, output);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    /*
     * PaletteApply is complete, so the encode queue can borrow the dither-side
     * thread budget and finish any remaining bands with a wider worker set.
     */
    boost_target = threads + dither_threads_budget;
    status = sixel_parallel_context_grow(&ctx, boost_target);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_parallel_context_wait(&ctx, 0);
    waited = 1;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

cleanup:
    dither->pipeline_row_callback = NULL;
    dither->pipeline_row_priv = NULL;
    dither->pipeline_index_buffer = NULL;
    dither->pipeline_index_size = 0;
    dither->pipeline_image_width = 0;
    dither->pipeline_image_height = 0;
    dither->pipeline_transparent_mask = NULL;
    dither->pipeline_transparent_mask_size = 0;
    dither->pipeline_transparent_keycolor = (-1);
    dither->pipeline_accumulation_result_enabled = 0;
    if (!waited && ctx.pool != NULL) {
        wait_status = sixel_parallel_context_wait(&ctx, status != SIXEL_OK);
        if (status == SIXEL_OK) {
            status = wait_status;
        }
    }
    sixel_parallel_context_cleanup(&ctx);
    sixel_timeline_logger_logf(logger,
        "controller", "encode", "body_finish", -1);
    if (owns_logger) {
        sixel_timeline_logger_unref(logger);
    }
    if (indexes != NULL) {
        sixel_allocator_free(allocator, indexes);
    }
    return status;
}
#else
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encode_body_ormode_pipeline(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *palette,
                                  float const *palette_float,
                                  sixel_dither_t *dither,
                                  sixel_output_t *output,
                                  int encode_threads)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)palette;
    (void)palette_float;
    (void)dither;
    (void)output;
    (void)encode_threads;
    return SIXEL_RUNTIME_ERROR;
}
#endif

/* implementation */

static void
sixel_advance(sixel_output_t *output, int nwrite)
{
    if ((output->pos += nwrite) >= SIXEL_OUTPUT_PACKET_SIZE) {
        (void)sixel_output_write_bytes(output,
                                       (char *)output->buffer,
                                       SIXEL_OUTPUT_PACKET_SIZE);
        memcpy(output->buffer,
               output->buffer + SIXEL_OUTPUT_PACKET_SIZE,
               (size_t)(output->pos -= SIXEL_OUTPUT_PACKET_SIZE));
    }
}


static void
sixel_putc(unsigned char *buffer, unsigned char value)
{
    *buffer = value;
}


static void
sixel_puts(unsigned char *buffer, char const *value, int size)
{
    memcpy(buffer, (void *)value, (size_t)size);
}

/*
 * Append a literal byte several times while respecting the output packet
 * boundary.  The helper keeps `sixel_advance` responsible for flushing and
 * preserves the repeating logic used by DECGRI sequences.
 */
static void
sixel_output_emit_literal(sixel_output_t *output,
                          unsigned char value,
                          int count)
{
    int chunk;

    if (count <= 0) {
        return;
    }

    while (count > 0) {
        chunk = SIXEL_OUTPUT_PACKET_SIZE - output->pos;
        if (chunk > count) {
            chunk = count;
        }
        memset(output->buffer + output->pos, value, (size_t)chunk);
        sixel_advance(output, chunk);
        count -= chunk;
    }
}

/* Only nonzero words reach this helper. The portable branch keeps the bit-mask
 * iterators available on compilers without a trailing-zero builtin. */
static int
sixel_encode_trailing_zeros(uint64_t bits)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll((unsigned long long)bits);
#else
    int shift;

    shift = 0;
    if ((bits & UINT64_C(0xffffffff)) == 0) {
        bits >>= 32;
        shift += 32;
    }
    if ((bits & UINT64_C(0xffff)) == 0) {
        bits >>= 16;
        shift += 16;
    }
    if ((bits & UINT64_C(0xff)) == 0) {
        bits >>= 8;
        shift += 8;
    }
    if ((bits & UINT64_C(0xf)) == 0) {
        bits >>= 4;
        shift += 4;
    }
    if ((bits & UINT64_C(3)) == 0) {
        bits >>= 2;
        shift += 2;
    }
    return shift + ((bits & UINT64_C(1)) == 0);
#endif
}

/* Consume each occupied column once, loading a new 64-column word only
 * after exhausting the current one. Padding bits stay zero because classify
 * records only source-backed columns inside the encoded width. */
static int
sixel_compose_next_column(uint64_t const *occupancy, size_t words,
                          size_t *word_index, uint64_t *bits, int width)
{
    int column;

    while (*bits == 0) {
        *word_index += 1;
        if (*word_index >= words) {
            return width;
        }
        *bits = occupancy[*word_index];
    }
    column = (int)(*word_index * 64U) +
        sixel_encode_trailing_zeros(*bits);
    *bits &= *bits - UINT64_C(1);
    return column;
}


#if HAVE_LDIV
static int
sixel_putnum_impl(char *buffer, long value, int pos)
{
    ldiv_t r;

    r = ldiv(value, 10);
    if (r.quot > 0) {
        pos = sixel_putnum_impl(buffer, r.quot, pos);
    }
    /*
     * r.rem is guaranteed to be in [0, 9] because the divisor is 10, so the
     * explicit cast documents the safe narrowing from long to char.
     */
    *(buffer + pos) = (char)('0' + r.rem);
    return pos + 1;
}
#endif  /* HAVE_LDIV */


static int
sixel_putnum(char *buffer, int value)
{
    int pos;

#if HAVE_LDIV
    pos = sixel_putnum_impl(buffer, value, 0);
#else
    pos = sprintf(buffer, "%d", value);
#endif  /* HAVE_LDIV */

    return pos;
}


static SIXELSTATUS
sixel_put_flash(sixel_output_t *const output)
{
    int nwrite;

    if (output->save_count <= 0) {
        return SIXEL_OK;
    }

    if (output->has_gri_arg_limit) {  /* VT240 Max 255 ? */
            while (output->save_count > 255) {
                /* argument of DECGRI('!') is limited to 255 in real VT */
                sixel_puts(output->buffer + output->pos, "!255", 4);
                sixel_advance(output, 4);
                sixel_putc(output->buffer + output->pos,
                           (unsigned char)output->save_pixel);
                sixel_advance(output, 1);
                output->save_count -= 255;
            }
        }

    if (output->save_count > 3) {
        /* DECGRI Graphics Repeat Introducer ! Pn Ch */
        sixel_putc(output->buffer + output->pos, '!');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, output->save_count);
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos,
                   (unsigned char)output->save_pixel);
        sixel_advance(output, 1);
    } else {
        sixel_output_emit_literal(output,
                                  (unsigned char)output->save_pixel,
                                  output->save_count);
    }

    output->save_pixel = 0;
    output->save_count = 0;

    return SIXEL_OK;
}


/*
 * Emit a run of identical SIXEL cells while keeping the existing repeat
 * accumulator intact.  The helper extends the current run when possible and
 * falls back to flushing through DECGRI before starting a new symbol.
 */
static SIXELSTATUS
sixel_emit_run(sixel_output_t *output, int symbol, int count)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (count <= 0) {
        return SIXEL_OK;
    }

    if (output->save_count > 0) {
        if (output->save_pixel == symbol) {
            output->save_count += count;
            return SIXEL_OK;
        }

        status = sixel_put_flash(output);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    output->save_pixel = symbol;
    output->save_count = count;

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_put_pixel(sixel_output_t *const output, int pix)
{
    if (pix < 0 || pix > '?') {
        pix = 0;
    }

    return sixel_emit_run(output, pix + '?', 1);
}

static SIXELSTATUS
sixel_node_new(sixel_node_t **np, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;

    *np = (sixel_node_t *)sixel_allocator_malloc(allocator,
                                                 sizeof(sixel_node_t));
    if (np == NULL) {
        sixel_helper_set_additional_message(
            "sixel_node_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

/* Write one complete run with the same DECGRI spelling as sixel_put_flash.
 * The buffer has two packets. sixel_advance leaves pos below one packet,
 * so at least SIXEL_OUTPUT_PACKET_SIZE bytes are free before every write.
 * A chunk needs at most the decimal digits of an int plus two bytes (or
 * three speculative literal bytes). Advance after each chunk, including
 * every !255 chunk, before borrowing that space again. */
static void
sixel_put_run_fast(sixel_output_t *output, int symbol, int count)
{
    unsigned char *p;
    char digits[sizeof(int) * CHAR_BIT];
    int nwrite;
    int value;
    int ndigits;

    if (count <= 0) {
        return;
    }
    if (output->has_gri_arg_limit) {
        while (count > 255) {
            p = output->buffer + output->pos;
            p[0] = '!';
            p[1] = '2';
            p[2] = '5';
            p[3] = '5';
            p[4] = (unsigned char)symbol;
            sixel_advance(output, 5);
            count -= 255;
        }
    }
    p = output->buffer + output->pos;
    if (count > 3) {
        nwrite = 0;
        p[nwrite++] = '!';
        if (count < 10) {
            p[nwrite++] = (unsigned char)('0' + count);
        } else if (count < 100) {
            p[nwrite++] = (unsigned char)('0' + count / 10);
            p[nwrite++] = (unsigned char)('0' + count % 10);
        } else if (count < 1000) {
            p[nwrite++] = (unsigned char)('0' + count / 100);
            p[nwrite++] = (unsigned char)('0' + count / 10 % 10);
            p[nwrite++] = (unsigned char)('0' + count % 10);
        } else {
            ndigits = 0;
            for (value = count; value > 0; value /= 10) {
                digits[ndigits++] = (char)('0' + value % 10);
            }
            while (ndigits > 0) {
                p[nwrite++] = (unsigned char)digits[--ndigits];
            }
        }
        p[nwrite++] = (unsigned char)symbol;
        sixel_advance(output, nwrite);
    } else {
        p[0] = (unsigned char)symbol;
        p[1] = (unsigned char)symbol;
        p[2] = (unsigned char)symbol;
        sixel_advance(output, count);
    }
}


static SIXELSTATUS
sixel_put_node(
    sixel_output_t /* in */     *output,  /* output context */
    int            /* in/out */ *x,       /* header position */
    sixel_node_t   /* in */     *np,      /* node object */
    int            /* in */     ncolors,  /* number of palette colors */
    int            /* in */     keycolor) /* transparent color number */
{
    int nwrite;
    int i;
    int end;
    int mx;
    unsigned char const *row;
    unsigned char value;

    /* Each node ends its runs locally, and workers reset save_count at
     * band entry. No deferred repeat can cross this node's color selector.
     * The leading gap is empty; composition (including size underpaint)
     * starts the node on a nonempty column, so these runs cannot merge. */
    if (ncolors != 2 || keycolor == (-1) || output->keycolor_reserved) {
        /* Designate the palette index, preserving the monochrome rule. */
        if (output->active_palette != np->pal) {
            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos,
                                  np->pal);
            sixel_advance(output, nwrite);
            output->active_palette = np->pal;
        }
    }
    if (*x < np->sx) {
        sixel_put_run_fast(output, '?', np->sx - *x);
        *x = np->sx;
    }
    row = (unsigned char const *)np->map;
    mx = np->mx;
    for (i = *x; i < mx; i = end) {
        value = row[i];
        if (value == 0 || value > '?') {
            /* Out-of-range masks retain the empty-cell interpretation.
             * Coalesce them with zero masks before spelling the run. */
            value = 0;
            for (end = i + 1; end < mx &&
                    (row[end] == 0 || row[end] > '?'); end++) {
            }
        } else {
            for (end = i + 1; end < mx && row[end] == value; end++) {
            }
        }
        sixel_put_run_fast(output, '?' + value, end - i);
    }
    if (*x < mx) {
        *x = mx;
    }
    return SIXEL_OK;
}


SIXELSTATUS
sixel_encode_header(int width, int height, int keycolor, sixel_output_t *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int p[3] = {0, 0, 0};
    int pcount = 3;
    int use_raster_attributes = 1;

    if (output->ormode) {
        p[0] = 7;
        p[1] = 5;
    } else if (output->require_keep_header ||
               sixel_output_has_transparent_offset(output)) {
        /*
         * Position padding and progressive presentation both preserve the
         * destination even when an opaque baseline has no omission key.
         */
        p[1] = 1;
    } else if (keycolor >= 0 || output->transparent_mask != NULL) {
        if (output->transparent_policy ==
                SIXEL_ALPHA_POLICY_COMPOSITE ||
            output->transparent_policy ==
                SIXEL_ALPHA_POLICY_KEEP) {
            /*
             * P2=1 asks the terminal to keep the current image-plane pixel
             * when a zero/transparent SIXEL cell is omitted. Composite
             * policy reaches this branch only when the loader could not
             * resolve a color and retained source transparency.
             */
            p[1] = 1;
        } else if (output->transparent_policy ==
                   SIXEL_ALPHA_POLICY_CLEAR) {
            /*
             * Spell out P2=0 for transparent output so the header carries
             * the clear-to-background contract even when P2's default value
             * would be equivalent.
             */
            pcount = 2;
        }
    }

    output->pos = 0;

    if (pcount == 3 && p[2] == 0) {
        pcount--;
        if (p[1] == 0) {
            pcount--;
            if (p[0] == 0) {
                pcount--;
            }
        }
    }

    status = sixel_output_begin_image(output,
                                      width,
                                      height,
                                      p[0],
                                      p[1],
                                      p[2],
                                      pcount,
                                      use_raster_attributes);

    return status;
}


static int
sixel_palette_float_pixelformat_for_colorspace(int colorspace)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_LINEAR:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    case SIXEL_COLORSPACE_CIELAB:
        return SIXEL_PIXELFORMAT_CIELABFLOAT32;
    case SIXEL_COLORSPACE_DIN99D:
        return SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    default:
        return SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
}

static int
sixel_output_palette_channel_to_pct(unsigned char const *palette,
                                    float const *palette_float,
                                    int n,
                                    int channel)
{
    size_t index;
    float value;
    int percent;

    index = (size_t)n * 3U + (size_t)channel;
    if (palette_float != NULL) {
        value = palette_float[index];
        if (value < 0.0f) {
            value = 0.0f;
        } else if (value > 1.0f) {
            value = 1.0f;
        }
        percent = (int)(value * 100.0f + 0.5f);
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }
        return percent;
    }

    if (palette != NULL) {
        return (palette[index] * 100 + 127) / 255;
    }

    return 0;
}

/* The history projection and byte comparison must agree on wire semantics.
 * Source coordinates are checked separately before binding comparison. */
static int
sixel_encode_delta_is_legacy(sixel_dither_t const *dither,
                             sixel_output_t const *output)
{
    return !dither->use_rgb101_palette && !output->ormode &&
        !dither->bodyonly && output->colorspace == SIXEL_COLORSPACE_GAMMA &&
        !(dither->ncolors == 2 && dither->keycolor >= 0 &&
          !dither->keycolor_reserved);
}

static double
sixel_output_palette_channel_to_float(unsigned char const *palette,
                                      float const *palette_float,
                                      int n,
                                      int channel)
{
    size_t index;
    double value;

    index = (size_t)n * 3U + (size_t)channel;
    value = 0.0;
    if (palette_float != NULL) {
        value = palette_float[index];
    } else if (palette != NULL) {
        value = (double)palette[index] / 255.0;
    }
    if (value < 0.0) {
        value = 0.0;
    } else if (value > 1.0) {
        value = 1.0;
    }

    return value;
}

/* Match the HLS writer and the established decoder's integer-percentage
 * reconstruction. Work per palette entry, never per source pixel. */
static void
sixel_encode_hls_history(unsigned char *emitted,
                         unsigned char const *palette,
                         float const *palette_float, size_t count)
{
    size_t i;
    int channel;
    int hls[3];
    unsigned char codes[3];
    double rgb[3];

    for (i = 0U; i < count; ++i) {
        for (channel = 0; channel < 3; ++channel) {
            rgb[channel] = sixel_output_palette_channel_to_float(palette,
                palette_float, (int)i, channel);
        }
        sixel_color_rgb_to_hls(hls, rgb);
        sixel_color_hls_to_rgb101(codes, hls);
        for (channel = 0; channel < 3; ++channel) {
            emitted[i * 3U + channel] =
                (unsigned char)((codes[channel] * 255U + 50U) / 100U);
        }
    }
}

/* Retained RGB must describe the serialized color, not its pre-wire table
 * value. Project a small capture-only copy through the same percentages as
 * the RGB/HLS writer. Lookup and diffusion keep their own working values.
 * Native RGB/HLS already supplies its corresponding projection. Other wire
 * interpretations retain their separate migration boundaries. */
static SIXELSTATUS
sixel_encode_capture_delta(sixel_dither_t *dither,
                            sixel_index_t const *indexes,
                            size_t pixel_count,
                            unsigned char const *palette,
                            float const *palette_float,
                            size_t palette_count,
                            sixel_output_t const *output)
{
    unsigned char emitted[SIXEL_PALETTE_MAX * 3];
    size_t i;
    int percent;

    if (dither == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (sixel_encode_delta_is_legacy(dither, output)) {
        if (palette == NULL || palette_count > SIXEL_PALETTE_MAX) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (output->palette_type == SIXEL_PALETTETYPE_HLS) {
            sixel_encode_hls_history(emitted, palette, palette_float,
                palette_count);
        } else {
            for (i = 0U; i < palette_count * 3U; ++i) {
                percent = sixel_output_palette_channel_to_pct(palette,
                    palette_float, (int)(i / 3U), (int)(i % 3U));
                emitted[i] = (unsigned char)((percent * 255 + 50) / 100);
            }
        }
        palette = emitted;
    }
    return sixel_dither_set_pipeline_accumulation_result_rgb(dither,
        indexes, pixel_count, palette, palette_count);
}


static SIXELSTATUS
output_rgb_palette_definition(
    sixel_output_t /* in */ *output,
    unsigned char const /* in */ *palette,
    float const /* in */ *palette_float,
    int            /* in */ n,
    int            /* in */ keycolor
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int nwrite;

    if (n != keycolor) {
        /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
        sixel_putc(output->buffer + output->pos, '#');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, n);
        sixel_advance(output, nwrite);
        sixel_puts(output->buffer + output->pos, ";2;", 3);
        sixel_advance(output, 3);
        nwrite = sixel_putnum(
            (char *)output->buffer + output->pos,
            sixel_output_palette_channel_to_pct(palette,
                                                palette_float,
                                                n,
                                                0));
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos, ';');
        sixel_advance(output, 1);
        nwrite = sixel_putnum(
            (char *)output->buffer + output->pos,
            sixel_output_palette_channel_to_pct(palette,
                                                palette_float,
                                                n,
                                                1));
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos, ';');
        sixel_advance(output, 1);
        nwrite = sixel_putnum(
            (char *)output->buffer + output->pos,
            sixel_output_palette_channel_to_pct(palette,
                                                palette_float,
                                                n,
                                                2));
        sixel_advance(output, nwrite);
    }

    status = SIXEL_OK;

    return status;
}


static SIXELSTATUS
output_hls_palette_definition(
    sixel_output_t /* in */ *output,
    unsigned char const /* in */ *palette,
    float const /* in */ *palette_float,
    int            /* in */ n,
    int            /* in */ keycolor
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    double rgb[3];
    int hls[3];
    int nwrite;

    if (n != keycolor) {
        rgb[0] = sixel_output_palette_channel_to_float(palette,
            palette_float, n, 0);
        rgb[1] = sixel_output_palette_channel_to_float(palette,
            palette_float, n, 1);
        rgb[2] = sixel_output_palette_channel_to_float(palette,
            palette_float, n, 2);
        sixel_color_rgb_to_hls(hls, rgb);
        /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
        sixel_putc(output->buffer + output->pos, '#');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, n);
        sixel_advance(output, nwrite);
        sixel_puts(output->buffer + output->pos, ";1;", 3);
        sixel_advance(output, 3);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, hls[0]);
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos, ';');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, hls[1]);
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos, ';');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, hls[2]);
        sixel_advance(output, nwrite);
    }

    status = SIXEL_OK;
    return status;
}


static void
sixel_encode_work_init(sixel_encode_work_t *work)
{
    work->map = NULL;
    work->map_size = 0;
    work->occupancy = NULL;
    work->occupancy_words = 0;
    work->band_columns = NULL;
    work->columns = NULL;
    work->columns_size = 0;
    work->active_colors = NULL;
    work->active_colors_size = 0;
    work->active_color_index = NULL;
    work->active_color_index_size = 0;
    work->requested_threads = 1;
}

static SIXELSTATUS
sixel_encode_work_allocate(sixel_encode_work_t *work,
                           int width,
                           int ncolors,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int len;
    size_t columns_size;
    size_t occupancy_words;
    size_t active_colors_size;
    size_t active_color_index_size;

    /* All map offsets in classify and compose fit in int. Check the whole
     * color plane before allocating, rather than each pixel's offset. */
    if (width > INT_MAX / ncolors ||
            (ncolors <= 2 &&
             (size_t)width > SIZE_MAX / sizeof(uint64_t))) {
        sixel_helper_set_additional_message(
            "sixel_encode_body: color plane size overflow.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    len = ncolors * width;
    work->map = (char *)sixel_allocator_calloc(allocator,
                                               (size_t)len,
                                               sizeof(char));
    if (work->map == NULL && len > 0) {
        sixel_helper_set_additional_message(
            "sixel_encode_body: sixel_allocator_calloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    work->map_size = (size_t)len;

    /* One bit per encoded column is independent of the six row bits in map.
     * Round in size_t and check the product before allocating all colors. */
    occupancy_words = ((size_t)width + 63U) / 64U;
    if (occupancy_words > SIZE_MAX / sizeof(uint64_t) / (size_t)ncolors) {
        sixel_helper_set_additional_message(
            "sixel_encode_body: occupancy size overflow.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (occupancy_words > 0) {
        work->occupancy = (uint64_t *)sixel_allocator_calloc(allocator,
            occupancy_words * (size_t)ncolors, sizeof(uint64_t));
        if (work->occupancy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: occupancy allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    work->occupancy_words = occupancy_words;

    /* Up to two colors use a transpose buffer: bytes 0..5 hold source colors
     * and byte 6 holds the paint-row mask. Other palettes use row writes. */
    if (ncolors <= 2) {
        work->band_columns = (uint64_t *)sixel_allocator_malloc(allocator,
            (size_t)width * sizeof(uint64_t));
        if (work->band_columns == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: band column allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    columns_size = sizeof(sixel_node_t *) * (size_t)width;
    if (width > 0) {
        work->columns = (sixel_node_t **)sixel_allocator_malloc(
            allocator,
            columns_size);
        if (work->columns == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(work->columns, 0, columns_size);
        work->columns_size = columns_size;
    } else {
        work->columns = NULL;
        work->columns_size = 0;
    }

    active_colors_size = (size_t)ncolors;
    if (active_colors_size > 0) {
        work->active_colors =
            (unsigned char *)sixel_allocator_malloc(allocator,
                                                    active_colors_size);
        if (work->active_colors == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(work->active_colors, 0, active_colors_size);
        work->active_colors_size = active_colors_size;
    } else {
        work->active_colors = NULL;
        work->active_colors_size = 0;
    }

    active_color_index_size = sizeof(int) * (size_t)ncolors;
    if (active_color_index_size > 0) {
        work->active_color_index = (int *)sixel_allocator_malloc(
            allocator,
            active_color_index_size);
        if (work->active_color_index == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(work->active_color_index, 0, active_color_index_size);
        work->active_color_index_size = active_color_index_size;
    } else {
        work->active_color_index = NULL;
        work->active_color_index_size = 0;
    }

    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        if (work->active_color_index != NULL) {
            sixel_allocator_free(allocator, work->active_color_index);
            work->active_color_index = NULL;
        }
        work->active_color_index_size = 0;
        if (work->active_colors != NULL) {
            sixel_allocator_free(allocator, work->active_colors);
            work->active_colors = NULL;
        }
        work->active_colors_size = 0;
        if (work->columns != NULL) {
            sixel_allocator_free(allocator, work->columns);
            work->columns = NULL;
        }
        work->columns_size = 0;
        if (work->band_columns != NULL) {
            sixel_allocator_free(allocator, work->band_columns);
            work->band_columns = NULL;
        }
        if (work->occupancy != NULL) {
            sixel_allocator_free(allocator, work->occupancy);
            work->occupancy = NULL;
        }
        work->occupancy_words = 0;
        if (work->map != NULL) {
            sixel_allocator_free(allocator, work->map);
            work->map = NULL;
        }
        work->map_size = 0;
    }

    return status;
}

static void
sixel_encode_work_cleanup(sixel_encode_work_t *work,
                          sixel_allocator_t *allocator)
{
    if (work->active_color_index != NULL) {
        sixel_allocator_free(allocator, work->active_color_index);
        work->active_color_index = NULL;
    }
    work->active_color_index_size = 0;
    if (work->active_colors != NULL) {
        sixel_allocator_free(allocator, work->active_colors);
        work->active_colors = NULL;
    }
    work->active_colors_size = 0;
    if (work->columns != NULL) {
        sixel_allocator_free(allocator, work->columns);
        work->columns = NULL;
    }
    work->columns_size = 0;
    if (work->band_columns != NULL) {
        sixel_allocator_free(allocator, work->band_columns);
        work->band_columns = NULL;
    }
    if (work->occupancy != NULL) {
        sixel_allocator_free(allocator, work->occupancy);
        work->occupancy = NULL;
    }
    work->occupancy_words = 0;
    if (work->map != NULL) {
        sixel_allocator_free(allocator, work->map);
        work->map = NULL;
    }
    work->map_size = 0;
}

static void
sixel_band_state_reset(sixel_band_state_t *state)
{
    state->row_in_band = 0;
    state->fillable = 0;
    state->fill_mask = 0;
    state->active_color_count = 0;
}

static void
sixel_band_finish(sixel_encode_work_t *work, sixel_band_state_t *state,
                   int width)
{
    int c;
    int first;
    int remaining;
    size_t begin;
    size_t end;
    size_t word;
    size_t edge;
    uint64_t *occupied;

    if (work->active_colors == NULL
        || work->active_color_index == NULL) {
        state->active_color_count = 0;
        return;
    }

    /* With every plane active, their merged range is the whole map. Batch
     * all resets in this dense case rather than issuing one per color. */
    if ((size_t)state->active_color_count == work->active_colors_size) {
        memset(work->map, 0, work->map_size);
        memset(work->occupancy, 0, work->active_colors_size *
               work->occupancy_words * sizeof(uint64_t));
        memset(work->active_colors, 0, work->active_colors_size);
        state->active_color_count = 0;
        return;
    }

    c = 0;
    remaining = state->active_color_count;
    while (remaining > 0) {
        while (work->active_colors[c] == 0) {
            c++;
        }
        first = c;
        occupied = work->occupancy + (size_t)c * work->occupancy_words;
        /* Every active color has source occupancy. Round the first edge
         * outward to its occupancy word to avoid an extra bit scan. */
        word = 0;
        while (occupied[word] == 0) {
            word++;
        }
        begin = (size_t)c * width + word * 64U;
        /* Merge adjacent active color planes into one clear, including their
         * internal zero padding. Never cross an inactive color plane. This
         * bounds the work to used colors without a small memset per node or
         * color. The enclosing span includes all size-policy gap fill. */
        do {
            work->active_colors[c++] = 0;
            remaining--;
        } while (remaining > 0 && work->active_colors[c] != 0);
        occupied = work->occupancy + (size_t)(c - 1) * work->occupancy_words;
        word = work->occupancy_words;
        while (occupied[word - 1] == 0) {
            word--;
        }
        edge = word * 64U;
        if (edge > (size_t)width) {
            edge = (size_t)width;
        }
        end = (size_t)(c - 1) * width + edge;
        memset(work->map + begin, 0, end - begin);
        memset(work->occupancy + (size_t)first * work->occupancy_words, 0,
               (size_t)(c - first) * work->occupancy_words * sizeof(uint64_t));
    }
    state->active_color_count = 0;
}

static SIXELSTATUS
sixel_encode_emit_palette(int bodyonly,
                          int ncolors,
                          int keycolor,
                          unsigned char const *palette,
                          float const *palette_float,
                          sixel_output_t *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;

    /*
     * Two colors with a key is the caller's mono convention (-e): the visible
     * color is whatever the terminal has selected, so emitting a definition
     * would override it.  A key the encoder reserved carries no such promise --
     * skipping the palette there paints the frame in the register the previous
     * frame happened to leave behind.
     */
    if (bodyonly
            || (ncolors == 2 && keycolor != (-1)
                && !output->keycolor_reserved)) {
        return SIXEL_OK;
    }

    if (palette == NULL && palette_float == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_emit_palette: missing palette data.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (output->palette_type == SIXEL_PALETTETYPE_HLS) {
        for (n = 0; n < ncolors; n++) {
            status = output_hls_palette_definition(output,
                                                   palette,
                                                   palette_float,
                                                   n,
                                                   keycolor);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    } else {
        for (n = 0; n < ncolors; n++) {
            status = output_rgb_palette_definition(output,
                                                   palette,
                                                   palette_float,
                                                   n,
                                                   keycolor);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    }

    status = SIXEL_OK;

end:
    return status;
}

/* Compare all eight bytes without borrowing into a neighboring byte, then
 * gather their equality bits. Packed colors use shifts, not host byte order. */
static unsigned int
sixel_band_color_mask(uint64_t word, unsigned int color)
{
    uint64_t different;
    uint64_t equal;

    different = word ^ ((uint64_t)color * UINT64_C(0x0101010101010101));
    equal = ~(((different & UINT64_C(0x7f7f7f7f7f7f7f7f)) +
              UINT64_C(0x7f7f7f7f7f7f7f7f)) |
              different | UINT64_C(0x7f7f7f7f7f7f7f7f));
    return (unsigned int)((((equal >> 7) & UINT64_C(0x0101010101010101)) *
                           UINT64_C(0x0102040810204080)) >> 56);
}

static void
sixel_band_classify(sixel_encode_work_t *work,
                    sixel_band_state_t *state,
                    sixel_index_t const *pixels,
                    int width,
                    int height,
                    int map_width,
                    int band_start,
                    int band_height,
                    int offset_left,
                    int offset_top,
                    int ncolors,
                    int keycolor,
                    unsigned char const *palstate,
                    int encode_policy,
                    unsigned char const *mask)
{
    int row_bit;
    int source_row;
    int source_band_start;
    int target_x;
    int pix;
    int x;
    unsigned int rows;
    unsigned int color_rows;
    uint64_t word;
    uint64_t paint_bit;
    uint64_t *columns;
    sixel_index_t const *source;
    unsigned char const *source_mask;
    unsigned char *active_colors;
    int *active_color_index;
    int active_count;
    char *map;
    uint64_t *occupancy;
    size_t occupancy_words;

    map = work->map;
    occupancy = work->occupancy;
    occupancy_words = work->occupancy_words;
    columns = work->band_columns;
    active_colors = work->active_colors;
    active_color_index = work->active_color_index;
    active_count = 0;
    source_band_start = band_start - offset_top;
    if (source_band_start < 0) {
        source_band_start = 0;
    }
    state->fillable = encode_policy == SIXEL_ENCODEPOLICY_SIZE;
    if (state->fillable && palstate != NULL) {
        state->fillable = source_band_start < height &&
            pixels[(size_t)source_band_start * (size_t)width] < ncolors;
    }
    state->fill_mask = 0;
    state->row_in_band = band_height;
    /* One- and two-color palettes repeat colors, so transposing lets us
     * combine several pixels into one map store and occupancy update. For
     * larger palettes the decomposition costs more than the stores it saves;
     * retain row writes, with the same checks hoisted out of the pixel loop. */
    if (ncolors <= 2) {
        memset(columns, 0, (size_t)width * sizeof(uint64_t));
    }

    /* The pipeline permits source rasters larger than INT_MAX, so source
     * offsets use size_t. Work allocation bounds map offsets to int.
     * Read rows in order so active_color_index retains the exact
     * first-seen order: equal-span nodes use it to break ties in compose. */
    for (row_bit = 0; row_bit < band_height; row_bit++) {
        source_row = band_start + row_bit - offset_top;
        if (source_row < 0 || source_row >= height) {
            continue;
        }
        state->fill_mask |= 1 << row_bit;
        source = pixels + (size_t)source_row * (size_t)width;
        /* Masks follow source coordinates, including with virtual margins. */
        source_mask = mask == NULL ? NULL :
            mask + (size_t)source_row * (size_t)width;
        paint_bit = UINT64_C(1) << (48 + row_bit);
        for (x = 0; x < width; x++) {
            if (source_mask != NULL && source_mask[x] != 0) {
                state->fillable = 0;
                continue;
            }
            pix = source[x];
            if (pix >= ncolors || pix == keycolor) {
                if (palstate == NULL) {
                    state->fillable = 0;
                }
                continue;
            }
            if (ncolors <= 2) {
                columns[x] |= ((uint64_t)pix << (8 * row_bit)) | paint_bit;
            } else {
                target_x = offset_left + x;
                map[pix * map_width + target_x] |= (1 << row_bit);
                occupancy[(size_t)pix * occupancy_words +
                    (size_t)target_x / 64U] |=
                    UINT64_C(1) << ((unsigned int)target_x % 64U);
            }
            if (active_colors[pix] == 0) {
                active_colors[pix] = 1;
                active_color_index[active_count++] = pix;
            }
        }
    }
    state->active_color_count = active_count;
    if (ncolors > 2) {
        return;
    }

    /* Only painted rows participate. In particular, a cleared byte in top
     * padding or a masked row must not become an occurrence of color zero.
     * Combine repeated colors before touching the scattered color planes:
     * each (color, column) gets one map store and one occupancy update. */
    for (x = 0; x < width; x++) {
        word = columns[x];
        rows = (unsigned int)(word >> 48);
        target_x = offset_left + x;
        while (rows != 0) {
            row_bit = sixel_encode_trailing_zeros(rows);
            pix = (int)((word >> (8 * row_bit)) & UINT64_C(0xff));
            color_rows = sixel_band_color_mask(word, (unsigned int)pix) & rows;
            map[pix * map_width + target_x] = (char)color_rows;
            occupancy[(size_t)pix * occupancy_words +
                (size_t)target_x / 64U] |=
                UINT64_C(1) << ((unsigned int)target_x % 64U);
            rows &= ~color_rows;
        }
    }
}

static SIXELSTATUS
sixel_band_compose(sixel_encode_work_t *work,
                   sixel_band_state_t *state,
                   sixel_output_t *output,
                   int width,
                   int ncolors,
                   int keycolor,
                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int color_index;
    int c;
    unsigned char *row;
    int sx;
    int mx;
    int next_column;
    size_t word_index;
    uint64_t bits;
    uint64_t const *occupancy;
    sixel_node_t *np;
    sixel_node_t *column_head;
    sixel_node_t *column_tail;
    sixel_node_t top;

    (void)ncolors;
    (void)keycolor;
    row = NULL;
    np = NULL;
    column_head = NULL;
    column_tail = NULL;
    top.next = NULL;

    if (work->columns != NULL) {
        memset(work->columns, 0, work->columns_size);
    }
    output->node_top = NULL;

    for (color_index = 0;
         color_index < state->active_color_count;
         color_index++) {
        c = work->active_color_index[color_index];
        row = (unsigned char *)(work->map + c * width);
        occupancy = work->occupancy + (size_t)c * work->occupancy_words;
        word_index = 0;
        bits = occupancy[0];
        sx = sixel_compose_next_column(occupancy, work->occupancy_words,
                                       &word_index, &bits, width);
        while (sx < width) {
            mx = sx + 1;
            next_column = sixel_compose_next_column(occupancy,
                work->occupancy_words, &word_index, &bits, width);
            /* mx is one past the last occupied column. Include up to nine
             * intervening zero columns, but never append trailing zeros.
             * Keep the first column beyond a longer gap for the next node. */
            while (next_column < width && next_column - mx <= 9) {
                mx = next_column + 1;
                next_column = sixel_compose_next_column(occupancy,
                    work->occupancy_words, &word_index, &bits, width);
            }

            if ((np = output->node_free) != NULL) {
                output->node_free = np->next;
            } else {
                status = sixel_node_new(&np, allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }

            np->pal = c;
            np->sx = sx;
            np->mx = mx;
            np->map = (char *)row;
            np->next = NULL;

            if (work->columns != NULL) {
                column_head = work->columns[sx];
                if (column_head == NULL
                    || column_head->mx <= np->mx) {
                    np->next = column_head;
                    work->columns[sx] = np;
                } else {
                    column_tail = column_head;
                    while (column_tail->next != NULL
                           && column_tail->next->mx > np->mx) {
                        column_tail = column_tail->next;
                    }
                    np->next = column_tail->next;
                    column_tail->next = np;
                }
            } else {
                top.next = output->node_top;
                column_tail = &top;

                while (column_tail->next != NULL) {
                    if (np->sx < column_tail->next->sx) {
                        break;
                    } else if (np->sx == column_tail->next->sx
                               && np->mx > column_tail->next->mx) {
                        break;
                    }
                    column_tail = column_tail->next;
                }

                np->next = column_tail->next;
                column_tail->next = np;
                output->node_top = top.next;
            }

            sx = next_column;
        }
    }

    if (work->columns != NULL) {
        top.next = NULL;
        column_tail = &top;
        for (sx = 0; sx < width; sx++) {
            column_head = work->columns[sx];
            if (column_head == NULL) {
                continue;
            }
            column_tail->next = column_head;
            while (column_tail->next != NULL) {
                column_tail = column_tail->next;
            }
            work->columns[sx] = NULL;
        }
        output->node_top = top.next;
    }

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_band_emit(sixel_encode_work_t *work,
                sixel_band_state_t *state,
                sixel_output_t *output,
                int ncolors,
                int keycolor,
                int last_row_index)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_node_t *np;
    sixel_node_t *next;
    sixel_node_t *prev;
    int x;
    int emit_next_line;

    emit_next_line = (last_row_index >= 6);
    if (emit_next_line) {
        /*
         * Emit DECGNL only after the first band. The first band starts at the
         * origin, so leading DECGNL would shift short images down by 6 rows.
         */
        output->buffer[output->pos] = '-';
        sixel_advance(output, 1);
    }

    for (x = 0; (np = output->node_top) != NULL;) {
        if (x > np->sx) {
            output->buffer[output->pos] = '$';
            sixel_advance(output, 1);
            x = 0;
        }

        if (state->fillable) {
            memset(np->map + np->sx,
                   state->fill_mask,
                   (size_t)(np->mx - np->sx));
        }
        status = sixel_put_node(output,
                                &x,
                                np,
                                ncolors,
                                keycolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        /* The outer loop always emits the head, so no search is needed. */
        next = np->next;
        output->node_top = next;
        np->next = output->node_free;
        output->node_free = np;
        np = next;
        prev = NULL;

        while (np != NULL) {
            if (np->sx < x) {
                prev = np;
                np = np->next;
                continue;
            }

            if (state->fillable) {
                memset(np->map + np->sx,
                       state->fill_mask,
                       (size_t)(np->mx - np->sx));
            }
            status = sixel_put_node(output,
                                    &x,
                                    np,
                                    ncolors,
                                    keycolor);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            /* Only skipped nodes remain in the list. Keep their last node
             * as the predecessor across consecutive removals, avoiding a
             * head-to-node search while preserving emission/fill order. */
            next = np->next;
            if (prev == NULL) {
                output->node_top = next;
            } else {
                prev->next = next;
            }
            np->next = output->node_free;
            output->node_free = np;
            np = next;
        }

        state->fillable = 0;
    }

    status = SIXEL_OK;

end:
    (void)work;
    return status;
}


SIXELSTATUS
sixel_encode_body(
    sixel_index_t       /* in */ *pixels,
    int                 /* in */ width,
    int                 /* in */ height,
    unsigned char       /* in */ *palette,
    float const         /* in */ *palette_float,
    int                 /* in */ ncolors,
    int                 /* in */ keycolor,
    int                 /* in */ bodyonly,
    sixel_output_t      /* in */ *output,
    unsigned char       /* in */ *palstate,
    sixel_allocator_t   /* in */ *allocator,
    int                 /* in */ pin_threads,
    sixel_timeline_logger_t      /* in */ *logger)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int band_start;
    int band_height;
    int last_row_index;
    int encoded_width;
    int encoded_height;
    int offset_left;
    int offset_top;
    sixel_node_t *np;
    sixel_encode_work_t work;
    sixel_band_state_t band;
    int logging_active;
    int job_index;

#if !SIXEL_ENABLE_THREADS
    (void) pin_threads;
#endif

    sixel_encode_work_init(&work);
    sixel_band_state_reset(&band);

    /* Record the caller/environment preference even before we fan out. */
    work.requested_threads = sixel_threads_resolve();

    if (ncolors < 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    output->active_palette = (-1);

    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    /* Transparent extent validates offset_left + width. Bound the entire
     * source raster here so row-major reads need no per-pixel checks. */
    if (height > INT_MAX / width) {
        sixel_helper_set_additional_message(
            "sixel_encode_body: source image size overflow.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    offset_left = output->transparent_offset_left;
    offset_top = output->transparent_offset_top;

    logging_active = logger != NULL;
    job_index = 0;

    status = sixel_encode_emit_palette(bodyonly,
                                       ncolors,
                                       keycolor,
                                       palette,
                                       palette_float,
                                       output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

#if SIXEL_ENABLE_THREADS
    {
        int nbands;
        int threads;

        nbands = sixel_count_sixel_bands(encoded_height);
        threads = work.requested_threads;
        if (nbands > 1 && threads > 1) {
            status = sixel_encode_body_parallel(pixels,
                                                width,
                                                height,
                                                ncolors,
                                                keycolor,
                                                output,
                                                palstate,
                                                allocator,
                                                threads,
                                                pin_threads);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            goto finalize;
        }
    }
#endif

    if (logging_active) {
        sixel_timeline_logger_logf(logger,
                          "controller",
                          "encode",
                          "configure",
                          -1);
    }

    status = sixel_encode_work_allocate(&work,
                                        encoded_width,
                                        ncolors,
                                        allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    band_start = 0;
    while (band_start < encoded_height) {
        band_height = encoded_height - band_start;
        if (band_height > 6) {
            band_height = 6;
        }

        band.row_in_band = 0;
        band.fillable = 0;
        band.fill_mask = 0;
        band.active_color_count = 0;

        if (logging_active) {
            sixel_timeline_logger_logf(logger,
                              "worker",
                              "encode",
                              "start",
                              job_index);
        }

        sixel_band_classify(&work,
                             &band,
                             pixels,
                             width,
                             height,
                             encoded_width,
                             band_start,
                             band_height,
                             offset_left,
                             offset_top,
                             ncolors,
                             keycolor,
                             palstate,
                             output->encode_policy,
                             output->transparent_mask);

        status = sixel_band_compose(&work,
                                    &band,
                                    output,
                                    encoded_width,
                                    ncolors,
                                    keycolor,
                                    allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }

        last_row_index = band_start + band_height - 1;
        status = sixel_band_emit(&work,
                                 &band,
                                 output,
                                 ncolors,
                                 keycolor,
                                 last_row_index);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }

        sixel_band_finish(&work, &band, encoded_width);

        if (logging_active) {
            sixel_timeline_logger_logf(logger,
                              "worker",
                              "encode",
                              "finish",
                              job_index);
        }

        band_start += band_height;
        sixel_band_state_reset(&band);
        job_index += 1;
    }

    status = SIXEL_OK;
    goto finalize;

finalize:
    if (palstate) {
        output->buffer[output->pos] = '$';
        sixel_advance(output, 1);
    }

cleanup:
    while ((np = output->node_free) != NULL) {
        output->node_free = np->next;
        sixel_allocator_free(allocator, np);
    }
    output->node_top = NULL;

    sixel_encode_work_cleanup(&work, allocator);

    return status;
}
SIXELSTATUS
sixel_encode_footer(sixel_output_t *output)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (output->pos > 0) {
        status = sixel_output_write_bytes(output,
                                          (char *)output->buffer,
                                          output->pos);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        output->pos = 0;
    }

    status = sixel_output_end_image(output);

    return status;
}

static int
sixel_encode_body_ormode_nplanes(int ncolors)
{
    int nplanes;

    for (nplanes = 8; nplanes > 1; nplanes--) {
        if (ncolors > (1 << (nplanes - 1))) {
            break;
        }
    }

    return nplanes;
}

static SIXELSTATUS
sixel_encode_body_ormode_emit_palette(unsigned char const *palette,
                                      float const *palette_float,
                                      int ncolors,
                                      int keycolor,
                                      sixel_output_t *output)
{
    SIXELSTATUS status;
    int n;

    if (palette == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    for (n = 0; n < ncolors; n++) {
        /* Native output has a destination float view for either syntax.
         * Legacy OR callers have always emitted RGB from their byte view. */
        if (palette_float != NULL &&
                output->palette_type == SIXEL_PALETTETYPE_HLS) {
            status = output_hls_palette_definition(output, palette,
                palette_float, n, keycolor);
        } else {
            status = output_rgb_palette_definition(output, palette,
                palette_float, n, keycolor);
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encode_body_ormode_band(sixel_index_t const *pixels,
                              int width,
                              int height,
                              int band_index,
                              int nplanes,
                              sixel_output_t *output)
{
    SIXELSTATUS status;
    sixel_index_t const *band_pixels;
    sixel_index_t const *column_pixels;
    sixel_index_t const *row0;
    sixel_index_t const *row1;
    sixel_index_t const *row2;
    sixel_index_t const *row3;
    sixel_index_t const *row4;
    sixel_index_t const *row5;
    int band_start;
    int band_height;
    int nwrite;
    int plane;
    int plane_bit;
    int full_mask;
    int sample_mask;
    int sample_width;
    int first_x;
    int first_pix;
    int x;
    int y;
    int pix;

    if (pixels == NULL || output == NULL || width < 1 || height < 0 ||
            band_index < 0 || nplanes < 1) {
        return SIXEL_BAD_ARGUMENT;
    }

    band_start = band_index * 6;
    if (band_start >= height) {
        return SIXEL_OK;
    }
    band_height = height - band_start;
    if (band_height > 6) {
        band_height = 6;
    }
    band_pixels = pixels + (size_t)band_start * (size_t)width;

    /*
     * Full six-row bands are the hot path for real images.  Keep the partial
     * tail on the generic loop below, but avoid the inner row loop and repeated
     * width multiplication for complete bands.
     */
    if (band_height == 6) {
        row0 = band_pixels;
        row1 = row0 + width;
        row2 = row1 + width;
        row3 = row2 + width;
        row4 = row3 + width;
        row5 = row4 + width;
        if (output->encode_policy != SIXEL_ENCODEPOLICY_SIZE) {
            for (plane = 0; plane < nplanes; plane++) {
                sixel_putc(output->buffer + output->pos, '#');
                sixel_advance(output, 1);
                nwrite = sixel_putnum((char *)output->buffer + output->pos,
                                      1 << plane);
                sixel_advance(output, nwrite);

                for (x = 0; x < width; x++) {
                    pix = (((row0[x] >> plane) & 0x1) << 0) |
                          (((row1[x] >> plane) & 0x1) << 1) |
                          (((row2[x] >> plane) & 0x1) << 2) |
                          (((row3[x] >> plane) & 0x1) << 3) |
                          (((row4[x] >> plane) & 0x1) << 4) |
                          (((row5[x] >> plane) & 0x1) << 5);
                    sixel_put_pixel(output, pix);
                }
                status = sixel_put_flash(output);
                if (SIXEL_FAILED(status)) {
                    return status;
                }

                sixel_putc(output->buffer + output->pos, '$');
                sixel_advance(output, 1);
            }
            sixel_putc(output->buffer + output->pos, '-');
            sixel_advance(output, 1);
            return SIXEL_OK;
        }

        full_mask = (1 << nplanes) - 1;
        sample_mask = 0;
        sample_width = width < 64 ? width : 64;
        for (x = 0; x < sample_width; x++) {
            sample_mask |= row0[x] | row1[x] | row2[x] |
                           row3[x] | row4[x] | row5[x];
        }

        if ((sample_mask & full_mask) == full_mask) {
            for (plane = 0; plane < nplanes; plane++) {
                plane_bit = 1 << plane;
                sixel_putc(output->buffer + output->pos, '#');
                sixel_advance(output, 1);
                nwrite = sixel_putnum((char *)output->buffer + output->pos,
                                      plane_bit);
                sixel_advance(output, nwrite);

                for (x = 0; x < width; x++) {
                    pix = ((row0[x] & plane_bit) ? 0x01 : 0) |
                          ((row1[x] & plane_bit) ? 0x02 : 0) |
                          ((row2[x] & plane_bit) ? 0x04 : 0) |
                          ((row3[x] & plane_bit) ? 0x08 : 0) |
                          ((row4[x] & plane_bit) ? 0x10 : 0) |
                          ((row5[x] & plane_bit) ? 0x20 : 0);
                    sixel_put_pixel(output, pix);
                }
                status = sixel_put_flash(output);
                if (SIXEL_FAILED(status)) {
                    return status;
                }

                sixel_putc(output->buffer + output->pos, '$');
                sixel_advance(output, 1);
            }
            sixel_putc(output->buffer + output->pos, '-');
            sixel_advance(output, 1);
            return SIXEL_OK;
        }

        for (plane = 0; plane < nplanes; plane++) {
            plane_bit = 1 << plane;
            first_x = (-1);
            pix = 0;
            for (x = 0; x < width; x++) {
                pix = ((row0[x] & plane_bit) ? 0x01 : 0) |
                      ((row1[x] & plane_bit) ? 0x02 : 0) |
                      ((row2[x] & plane_bit) ? 0x04 : 0) |
                      ((row3[x] & plane_bit) ? 0x08 : 0) |
                      ((row4[x] & plane_bit) ? 0x10 : 0) |
                      ((row5[x] & plane_bit) ? 0x20 : 0);
                if (pix != 0) {
                    first_x = x;
                    break;
                }
            }
            if (first_x < 0) {
                continue;
            }

            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos,
                                  plane_bit);
            sixel_advance(output, nwrite);

            status = sixel_emit_run(output, '?', first_x);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            sixel_put_pixel(output, pix);
            for (x = first_x + 1; x < width; x++) {
                pix = ((row0[x] & plane_bit) ? 0x01 : 0) |
                      ((row1[x] & plane_bit) ? 0x02 : 0) |
                      ((row2[x] & plane_bit) ? 0x04 : 0) |
                      ((row3[x] & plane_bit) ? 0x08 : 0) |
                      ((row4[x] & plane_bit) ? 0x10 : 0) |
                      ((row5[x] & plane_bit) ? 0x20 : 0);
                sixel_put_pixel(output, pix);
            }
            status = sixel_put_flash(output);
            if (SIXEL_FAILED(status)) {
                return status;
            }

            sixel_putc(output->buffer + output->pos, '$');
            sixel_advance(output, 1);
        }
        sixel_putc(output->buffer + output->pos, '-');
        sixel_advance(output, 1);
        return SIXEL_OK;
    }

    /*
     * OR mode composes the final color by drawing powers-of-two bit-planes.
     * Size policy may spend a little more branch work to avoid empty planes.
     * Delay the plane header until the first non-zero cell.  A plane that never
     * draws any cell cannot affect the OR-composited result, while a non-empty
     * plane keeps its horizontal position by emitting the leading zero run
     * before the first drawn cell.
     */
    if (output->encode_policy != SIXEL_ENCODEPOLICY_SIZE) {
        for (plane = 0; plane < nplanes; plane++) {
            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos,
                                  1 << plane);
            sixel_advance(output, nwrite);

            column_pixels = band_pixels;
            for (x = 0; x < width; x++, column_pixels++) {
                pix = 0;
                for (y = 0; y < band_height; y++) {
                    pix |= (((column_pixels[width * y] >> plane) & 0x1) << y);
                }
                sixel_put_pixel(output, pix);
            }
            status = sixel_put_flash(output);
            if (SIXEL_FAILED(status)) {
                return status;
            }

            sixel_putc(output->buffer + output->pos, '$');
            sixel_advance(output, 1);
        }
        return SIXEL_OK;
    }

    full_mask = (1 << nplanes) - 1;
    sample_mask = 0;
    sample_width = width < 64 ? width : 64;
    column_pixels = band_pixels;
    for (x = 0; x < sample_width; x++, column_pixels++) {
        for (y = 0; y < band_height; y++) {
            sample_mask |= column_pixels[width * y];
        }
    }

    if ((sample_mask & full_mask) == full_mask) {
        for (plane = 0; plane < nplanes; plane++) {
            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos,
                                  1 << plane);
            sixel_advance(output, nwrite);

            column_pixels = band_pixels;
            for (x = 0; x < width; x++, column_pixels++) {
                pix = 0;
                for (y = 0; y < band_height; y++) {
                    pix |= (((column_pixels[width * y] >> plane) & 0x1) << y);
                }
                sixel_put_pixel(output, pix);
            }
            status = sixel_put_flash(output);
            if (SIXEL_FAILED(status)) {
                return status;
            }

            sixel_putc(output->buffer + output->pos, '$');
            sixel_advance(output, 1);
        }
        return SIXEL_OK;
    }

    for (plane = 0; plane < nplanes; plane++) {
        plane_bit = 1 << plane;
        first_x = (-1);
        first_pix = 0;
        column_pixels = band_pixels;
        for (x = 0; x < width; x++, column_pixels++) {
            pix = 0;
            for (y = 0; y < band_height; y++) {
                pix |= (((column_pixels[width * y] >> plane) & 0x1) << y);
            }
            if (pix != 0) {
                first_x = x;
                first_pix = pix;
                break;
            }
        }
        if (first_x < 0) {
            continue;
        }

        sixel_putc(output->buffer + output->pos, '#');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos,
                              plane_bit);
        sixel_advance(output, nwrite);

        status = sixel_emit_run(output, '?', first_x);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        sixel_put_pixel(output, first_pix);
        column_pixels = band_pixels + first_x + 1;
        for (x = first_x + 1; x < width; x++, column_pixels++) {
            pix = 0;
            for (y = 0; y < band_height; y++) {
                pix |= (((column_pixels[width * y] >> plane) & 0x1) << y);
            }
            sixel_put_pixel(output, pix);
        }
        status = sixel_put_flash(output);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        sixel_putc(output->buffer + output->pos, '$');
        sixel_advance(output, 1);
    }
    if (band_height == 6) {
        sixel_putc(output->buffer + output->pos, '-');
        sixel_advance(output, 1);
    }

    return SIXEL_OK;
}

/*
 * Column bits for one OR-mode band under a transparent offset.  Columns and
 * rows outside the source image contribute no bits, which OR mode paints as
 * "leave the terminal pixel alone".  That is what makes the offset margin
 * transparent here without a separate key color.
 */
static int
sixel_encode_body_ormode_column(sixel_index_t const *pixels,
                                int width,
                                int height,
                                int band_start,
                                int band_height,
                                int x,
                                int offset_left,
                                int offset_top,
                                int plane)
{
    int source_x;
    int source_row;
    int pix;
    int y;

    source_x = x - offset_left;
    if (source_x < 0 || source_x >= width) {
        return 0;
    }

    pix = 0;
    for (y = 0; y < band_height; y++) {
        source_row = band_start + y - offset_top;
        if (source_row < 0 || source_row >= height) {
            continue;
        }
        pix |= (((pixels[(size_t)source_row * (size_t)width +
                         (size_t)source_x] >> plane) & 0x1) << y);
    }

    return pix;
}

static SIXELSTATUS
sixel_encode_body_ormode_band_offset(sixel_index_t const *pixels,
                                     int width,
                                     int height,
                                     int band_index,
                                     int nplanes,
                                     int encoded_width,
                                     int encoded_height,
                                     int offset_left,
                                     int offset_top,
                                     sixel_output_t *output)
{
    SIXELSTATUS status;
    int band_start;
    int band_height;
    int nwrite;
    int plane;
    int plane_bit;
    int first_x;
    int first_pix;
    int x;
    int pix;

    if (pixels == NULL || output == NULL || width < 1 || height < 0 ||
            band_index < 0 || nplanes < 1 || encoded_width < 1 ||
            encoded_height < 1 || offset_left < 0 || offset_top < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    band_start = band_index * 6;
    if (band_start >= encoded_height) {
        return SIXEL_OK;
    }
    band_height = encoded_height - band_start;
    if (band_height > 6) {
        band_height = 6;
    }

    for (plane = 0; plane < nplanes; plane++) {
        plane_bit = 1 << plane;
        first_x = (-1);
        first_pix = 0;
        for (x = 0; x < encoded_width; x++) {
            pix = sixel_encode_body_ormode_column(pixels,
                                                  width,
                                                  height,
                                                  band_start,
                                                  band_height,
                                                  x,
                                                  offset_left,
                                                  offset_top,
                                                  plane);
            if (pix != 0) {
                first_x = x;
                first_pix = pix;
                break;
            }
        }
        if (first_x < 0) {
            continue;
        }

        sixel_putc(output->buffer + output->pos, '#');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos,
                              plane_bit);
        sixel_advance(output, nwrite);

        status = sixel_emit_run(output, '?', first_x);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        sixel_put_pixel(output, first_pix);
        for (x = first_x + 1; x < encoded_width; x++) {
            pix = sixel_encode_body_ormode_column(pixels,
                                                  width,
                                                  height,
                                                  band_start,
                                                  band_height,
                                                  x,
                                                  offset_left,
                                                  offset_top,
                                                  plane);
            sixel_put_pixel(output, pix);
        }
        status = sixel_put_flash(output);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        sixel_putc(output->buffer + output->pos, '$');
        sixel_advance(output, 1);
    }
    if (band_height == 6) {
        sixel_putc(output->buffer + output->pos, '-');
        sixel_advance(output, 1);
    }

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encode_body_ormode(
    sixel_index_t       /* in */ *pixels,
    int                 /* in */ width,
    int                 /* in */ height,
    unsigned char       /* in */ *palette,
    float const         /* in */ *palette_float,
    int                 /* in */ ncolors,
    int                 /* in */ keycolor,
    sixel_output_t      /* in */ *output)
{
    SIXELSTATUS status;
    int encoded_width;
    int encoded_height;
    int offset_left;
    int offset_top;
    int nplanes;
    int nbands;
    int band_index;

    if (pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    offset_left = output->transparent_offset_left;
    offset_top = output->transparent_offset_top;

    status = sixel_encode_body_ormode_emit_palette(palette,
                                                   palette_float,
                                                   ncolors,
                                                   keycolor,
                                                   output);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    nplanes = sixel_encode_body_ormode_nplanes(ncolors);
    nbands = (encoded_height + 5) / 6;
    for (band_index = 0; band_index < nbands; band_index++) {
        if (offset_left == 0 && offset_top == 0) {
            status = sixel_encode_body_ormode_band(pixels,
                                                   width,
                                                   height,
                                                   band_index,
                                                   nplanes,
                                                   output);
        } else {
            status = sixel_encode_body_ormode_band_offset(pixels,
                                                          width,
                                                          height,
                                                          band_index,
                                                          nplanes,
                                                          encoded_width,
                                                          encoded_height,
                                                          offset_left,
                                                          offset_top,
                                                          output);
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_encode_dither(
    unsigned char   /* in */ *pixels,   /* pixel bytes to be encoded */
    int             /* in */ width,     /* width of source image */
    int             /* in */ height,    /* height of source image */
    sixel_dither_t  /* in */ *dither,   /* dither context */
    sixel_output_t  /* in */ *output)   /* output context */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_index_t *paletted_pixels = NULL;
    sixel_index_t *input_pixels;
    size_t bufsize;
    unsigned char *palette_entries = NULL;
    float *palette_entries_float32 = NULL;
    sixel_palette_t *palette_obj = NULL;
    size_t palette_count = 0U;
    size_t palette_float_count = 0U;
    size_t palette_bytes = 0U;
    size_t palette_float_bytes = 0U;
    size_t palette_channels = 0U;
    size_t palette_index = 0U;
    size_t pixel_count = 0U;
    int palette_source_colorspace;
    int palette_float_pixelformat;
    int output_float_pixelformat;
    int palette_float_depth;
    int pipeline_active;
    int pipeline_threads = 0;  /* set to a deterministic default before use */
    int pipeline_nbands;
    int encoded_width;
    int encoded_height;
    sixel_dither_encode_allocation_t dither_parallel;
    sixel_palette_entries_view_t palette_view;
    sixel_palette_float32_entries_view_t palette_float_view;
    sixel_timeline_logger_t *serial_logger;
    int logger_owned = 0;
    sixel_timeline_logger_t *logger = NULL;
    SIXELSTATUS close_status;
    int image_started;
    unsigned char const *output_rgb101;
    float const *output_rgb_float;
    float const *ormode_palette_float;
    unsigned int output_rgb101_count;
    int use_rgb101_palette;
    unsigned char hls_bytes[SIXEL_PALETTE_MAX * 3];

    if (output == NULL || dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_dither_prepare_standalone_palette(dither);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    dither->pipeline_6delta_rgb101 = 0;
    dither->pipeline_6delta_hls = 0;
    dither->pipeline_6delta_hls_bytes = NULL;
    dither->pipeline_6delta_hls_count = 0U;
    output_rgb101 = NULL;
    output_rgb_float = NULL;
    ormode_palette_float = NULL;
    output_rgb101_count = 0U;
    use_rgb101_palette = dither->use_rgb101_palette != 0;
    output->transparent_mask = NULL;

    /*
     * Preserve the effective geometry for the post-encode contract trace.
     * Palette application clears its temporary pipeline hints before the
     * encoder emits that trace, so the live hint fields cannot carry this
     * diagnostic state.
     */
    dither->pipeline_last_mode = SIXEL_DITHER_ENCODE_SERIAL;
    dither->pipeline_last_band_height = 0;
    dither->pipeline_last_band_overlap = 0;
    dither->pipeline_last_dither_threads = 0;
    dither->pipeline_last_encode_threads = sixel_threads_resolve();

    serial_logger = NULL;
    close_status = SIXEL_FALSE;
    image_started = 0;
    palette_source_colorspace = SIXEL_COLORSPACE_GAMMA;
    if (width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / (size_t)height) {
        sixel_helper_set_additional_message(
            "sixel_encode_dither: image size overflow.");
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (sixel_output_has_transparent_offset(output) != 0) {
        if (output->transparent_policy !=
                SIXEL_ALPHA_POLICY_KEEP) {
            sixel_helper_set_additional_message(
                "transparent-offset requires alpha-policy=keep.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }
    status = sixel_output_compute_transparent_extent(output,
                                                     width,
                                                     height,
                                                     &encoded_width,
                                                     &encoded_height);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (dither->keycolor < 0 && dither->pipeline_transparent_mask != NULL) {
        if (output->ormode ||
                dither->pipeline_transparent_mask_size < pixel_count) {
            sixel_helper_set_additional_message(
                "separate output mask requires normal output and full size.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        /* Palette application may clear its pipeline hints. Retain the
         * frame-owned view until all band workers have finished, then clear
         * it on both success and failure before output reuse. */
        output->transparent_mask = dither->pipeline_transparent_mask;
    }
    if (use_rgb101_palette && dither->keycolor < 0 &&
            dither->pipeline_6delta_enabled &&
            SIXEL_PIXELFORMAT_IS_FLOAT32(dither->pixelformat)) {
        /* Reserve the shared mask before pipeline workers copy output views.
         * PaletteApply reuses this storage and publishes each row only after
         * its keep decisions. Fixed-table index zero remains a real color;
         * only mask bits distinguish it from an omission placeholder. */
        if (output->ormode) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        status = sixel_dither_prepare_delta_mask(dither,
            dither->pipeline_transparent_mask,
            dither->pipeline_transparent_mask_size, pixel_count);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (dither->pipeline_accumulation_result_mask_size == pixel_count) {
            output->transparent_mask =
                dither->pipeline_accumulation_result_mask;
        }
    }
    palette_float_pixelformat =
        sixel_palette_float_pixelformat_for_colorspace(
            palette_source_colorspace);
    palette_float_depth =
        sixel_helper_compute_depth(palette_float_pixelformat);
    output_float_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    memset(&palette_view, 0, sizeof(palette_view));
    memset(&palette_float_view, 0, sizeof(palette_float_view));
    palette_obj = dither->palette;
    if (palette_obj == NULL || palette_obj->vtbl == NULL ||
            palette_obj->vtbl->get_entries == NULL ||
            palette_obj->vtbl->get_entries_float32 == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_dither: palette acquisition failed.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (use_rgb101_palette &&
            (dither->pipeline_6delta_enabled ||
             dither->pipeline_accumulation_result_enabled) &&
            output->palette_type == SIXEL_PALETTETYPE_HLS) {
        /* Publish before workers, including the first frame: capture is
         * enabled before valid history exists for a keep comparison. Both
         * candidate comparison and captured history borrow this view. */
        status = sixel_palette_prepare_hls(palette_obj);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        dither->pipeline_6delta_hls = 1;
    }
    if (dither->pipeline_6delta_enabled &&
            sixel_encode_delta_is_legacy(dither, output) &&
            output->source_colorspace == SIXEL_COLORSPACE_GAMMA) {
        status = palette_obj->vtbl->get_entries_float32(palette_obj,
            &palette_float_view);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        /* Byte entries alone determine these emitted colors. Legacy
         * float-backed palettes need a separate authoritative comparison.
         * Bind before either serial application or worker copies, and clear
         * after their lifetime even if encoding fails. */
        if (palette_float_view.entries == NULL &&
                output->palette_type == SIXEL_PALETTETYPE_HLS) {
            status = palette_obj->vtbl->get_entries(palette_obj, &palette_view);
            if (SIXEL_FAILED(status) || palette_view.entries == NULL ||
                    palette_view.depth != 3 ||
                    palette_view.entry_count > SIXEL_PALETTE_MAX) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            memcpy(hls_bytes, palette_view.entries,
                (size_t)palette_view.entry_count * 3U);
            /* Exact snap, if requested, also runs on the writer's copy.
             * Do not change the lookup table or enable snap implicitly. */
            if (dither->palette_snap_exact) {
                sixel_palette_snap_output(hls_bytes, NULL,
                    palette_view.entry_count);
            }
            sixel_encode_hls_history(hls_bytes, hls_bytes, NULL,
                palette_view.entry_count);
            dither->pipeline_6delta_hls_bytes = hls_bytes;
            dither->pipeline_6delta_hls_count = palette_view.entry_count;
        } else {
            dither->pipeline_6delta_rgb101 =
                palette_float_view.entries == NULL;
        }
    }

    pipeline_active = 0;
    /*
     * Normalize the planner-provided pinning request so both palette and
     * encode workers see the same 0/1 flag.
     */
    dither->pipeline_pin_threads =
        dither->pipeline_pin_threads != 0 ? 1 : 0;
    switch (dither->pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_G1:
    case SIXEL_PIXELFORMAT_G2:
    case SIXEL_PIXELFORMAT_G4:
        bufsize = (sizeof(sixel_index_t) * (size_t)width * (size_t)height * 3UL);
        paletted_pixels = (sixel_index_t *)sixel_allocator_malloc(dither->allocator, bufsize);
        if (paletted_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_dither: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_helper_normalize_pixelformat(paletted_pixels,
                                                    &dither->pixelformat,
                                                    pixels,
                                                    dither->pixelformat,
                                                    width, height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        input_pixels = paletted_pixels;
        break;
    case SIXEL_PIXELFORMAT_PAL8:
    case SIXEL_PIXELFORMAT_G8:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
        input_pixels = pixels;
        break;
    default:
        /* apply palette */
        dither->pipeline_last_dither_threads = 1;
        pipeline_threads = sixel_threads_resolve();
        pipeline_nbands = sixel_count_sixel_bands(encoded_height);
        /*
         * Pipeline mode lets PaletteApply produce contiguous index rows while
         * band workers consume completed six-line slices. OR mode now has its
         * own band encoder, so it can share this producer/writer path without
         * dereferencing input_pixels on the caller side.
         */
        if (pipeline_threads > 1 && pipeline_nbands > 1 &&
                !(output->ormode != 0 &&
                  sixel_output_has_transparent_offset(output) != 0)) {
            pipeline_active = 1;
            input_pixels = NULL;
        } else {
            paletted_pixels = sixel_dither_apply_palette(dither, pixels,
                                                         width, height);
            if (paletted_pixels == NULL) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            input_pixels = paletted_pixels;
        }
        break;
    }

    if (pipeline_active) {
        sixel_parallel_dither_configure(height, pipeline_threads,
                                        pixel_count, dither,
                                        &dither_parallel);
        dither->pipeline_parallel_active =
            dither_parallel.mode != SIXEL_DITHER_ENCODE_SERIAL;
        dither->pipeline_band_height = dither_parallel.band_height;
        dither->pipeline_band_overlap = dither_parallel.overlap;
        dither->pipeline_dither_threads = dither_parallel.dither_threads;
        dither->pipeline_last_mode = (int)dither_parallel.mode;
        dither->pipeline_last_band_height = dither_parallel.band_height;
        dither->pipeline_last_band_overlap = dither_parallel.overlap;
        dither->pipeline_last_dither_threads = dither_parallel.dither_threads;
        dither->pipeline_last_encode_threads = dither_parallel.encode_threads;
        pipeline_active =
            dither_parallel.mode == SIXEL_DITHER_ENCODE_PIPELINE;
        pipeline_threads = dither_parallel.encode_threads;
        if (!pipeline_active) {
            /* A staged allocation keeps band application enabled without
             * row-pipeline submission. Application joins every dither
             * worker before the body encoder claims the full budget. */
            paletted_pixels = sixel_dither_apply_palette(dither, pixels,
                                                         width, height);
            if (paletted_pixels == NULL) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            input_pixels = paletted_pixels;
        }
    }

    /* Keep serial body intervals available in builds without worker pools. */
    if (!pipeline_active) {
        logger = dither->pipeline_logger;
        if (logger == NULL) {
            sixel_timeline_logger_prepare_default(dither->allocator,
                                                  &serial_logger);
            if (serial_logger != NULL) {
                logger_owned = 1;
                dither->pipeline_logger = serial_logger;
                logger = serial_logger;
            } else {
                logger = NULL;
            }
        }
        if (logger != NULL) {
            sixel_timeline_logger_logf(logger,
                              "controller",
                              "pipeline",
                              "configure",
                              -1);
        }
    }

    if (output != NULL) {
        palette_source_colorspace = output->source_colorspace;
        palette_float_pixelformat =
            sixel_palette_float_pixelformat_for_colorspace(
                palette_source_colorspace);
        palette_float_depth =
            sixel_helper_compute_depth(palette_float_pixelformat);
    }

    if (!use_rgb101_palette) {
        /* Indexed input can bypass application. Observe a byte assignment
         * here too, before copying the authoritative working float view. */
        status = sixel_palette_reconcile_bytes(palette_obj,
            palette_float_pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = palette_obj->vtbl->get_entries(palette_obj, &palette_view);
    if (SIXEL_FAILED(status) || palette_view.entries == NULL ||
            palette_view.depth != 3 || palette_view.entry_count == 0U) {
        sixel_helper_set_additional_message(
            "sixel_encode_dither: palette view failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    palette_count = palette_view.entry_count;
    palette_bytes = palette_count * 3U;
    palette_entries = (unsigned char *)sixel_allocator_malloc(
        dither->allocator,
        palette_bytes);
    if (palette_entries == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_dither: palette copy allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(palette_entries, palette_view.entries, palette_bytes);

    status = palette_obj->vtbl->get_entries_float32(palette_obj,
                                                    &palette_float_view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (palette_float_view.entries != NULL &&
            palette_float_view.entry_count == palette_count &&
            palette_float_view.depth == palette_float_depth) {
        palette_float_count = palette_float_view.entry_count;
        palette_float_bytes =
            palette_float_count * (size_t)palette_float_view.depth;
        palette_entries_float32 = (float *)sixel_allocator_malloc(
            dither->allocator,
            palette_float_bytes);
        if (palette_entries_float32 == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encode_dither: float palette copy allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(palette_entries_float32,
               palette_float_view.entries,
               palette_float_bytes);
    }
    if (use_rgb101_palette) {
        status = sixel_palette_get_rgb101(palette_obj, &output_rgb101,
                                           &output_rgb101_count);
        if (SIXEL_FAILED(status) || palette_entries_float32 == NULL ||
                output_rgb101_count != palette_count ||
                palette_float_count != palette_count) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (output->palette_type == SIXEL_PALETTETYPE_HLS) {
            status = sixel_palette_get_output_rgb(palette_obj,
                &output_rgb_float, &output_rgb101_count);
            if (SIXEL_FAILED(status) || output_rgb_float == NULL ||
                    output_rgb101_count != palette_count) {
                status = SIXEL_LOGIC_ERROR;
                goto end;
            }
        }
        if (dither->pipeline_6delta_hls) {
            status = sixel_palette_get_hls_rgb101(palette_obj,
                &output_rgb101, &output_rgb101_count);
            if (SIXEL_FAILED(status) || output_rgb101_count != palette_count) {
                status = SIXEL_LOGIC_ERROR;
                goto end;
            }
        }
        /* Lookup used the unrounded working palette. RGB serialization uses
         * cached percentages; HLS uses destination RGB before rounding to
         * avoid a second quantization. The byte copy models painted RGB;
         * HLS delta uses its decoded percentage approximation. Neither copy
         * feeds back into ordinary lookup or enables palette snapping. */
        for (palette_index = 0U; palette_index < palette_count * 3U;
                ++palette_index) {
            palette_entries_float32[palette_index] =
                output_rgb_float != NULL ? output_rgb_float[palette_index] :
                (float)output_rgb101[palette_index] / 100.0f;
            palette_entries[palette_index] = (unsigned char)(
                (output_rgb101[palette_index] * 255U + 50U) / 100U);
        }
    }
    /* OR shares the destination view selected above, never working RGB.
     * Legacy OR callers retain their existing byte contract. */
    if (use_rgb101_palette) {
        ormode_palette_float = palette_entries_float32;
    }
    if (!use_rgb101_palette && palette_entries != NULL && palette_count > 0U
            && output != NULL
            && output->source_colorspace != output->colorspace) {
        palette_bytes = palette_count * 3U;
        if (palette_entries_float32 != NULL
                && palette_float_count == palette_count) {
            /*
             * Use the higher-precision palette to change color spaces once and
             * then quantize those float channels down to bytes.  The previous
             * implementation converted the 8bit entries before overwriting
             * them from float again, doubling the amount of work and rounding
             * the palette twice.
             */
            palette_float_bytes = palette_bytes * sizeof(float);
            status = sixel_helper_convert_colorspace(
                (unsigned char *)palette_entries_float32,
                palette_float_bytes,
                palette_float_pixelformat,
                output->source_colorspace,
                output->colorspace);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_encode_dither: float palette colorspace conversion failed.");
                goto end;
            }
            output_float_pixelformat =
                sixel_palette_float_pixelformat_for_colorspace(
                    output->colorspace);
            palette_channels = palette_count * 3U;
            for (palette_index = 0U; palette_index < palette_channels;
                    ++palette_index) {
                int channel;

                channel = (int)(palette_index % 3U);
                palette_entries[palette_index] =
                    sixel_pixelformat_float_channel_to_byte(
                        output_float_pixelformat,
                        channel,
                        palette_entries_float32[palette_index]);
            }
        } else {
            status = sixel_helper_convert_colorspace(palette_entries,
                                                     palette_bytes,
                                                     SIXEL_PIXELFORMAT_RGB888,
                                                     output->source_colorspace,
                                                     output->colorspace);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_encode_dither: palette colorspace "
                    "conversion failed.");
                goto end;
            }
        }
    }
    if (!use_rgb101_palette && dither->palette_snap_exact != 0) {
        sixel_palette_snap_output(palette_entries, palette_entries_float32,
                                   (unsigned int)palette_count);
    }
    if (SIXEL_FAILED(status) || palette_entries == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_dither: palette copy failed.");
        goto end;
    }

    if (input_pixels != NULL &&
            dither->pipeline_accumulation_result_enabled != 0) {
        status = sixel_encode_capture_delta(
            dither, input_pixels, pixel_count, palette_entries,
            palette_entries_float32, palette_count, output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /*
     * A key the encoder reserved for itself (6delta appends one to every
     * frame) must not trigger the legacy two-color mono shortcut below.  Refresh
     * it per encode so a recycled output object cannot carry a stale value.
     */
    output->keycolor_reserved = dither->keycolor_reserved != 0 ? 1 : 0;

    status = sixel_encode_header(encoded_width,
                                 encoded_height,
                                 dither->keycolor,
                                 output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    image_started = 1;

    /* The pipeline owns its envelope and logger inside the body helper.
     * Serial application has already released its borrowed logger hint. */
    if (!pipeline_active) {
        sixel_timeline_logger_logf(logger,
            "controller", "encode", "body_start", -1);
    }
    if (pipeline_active) {
        if (output->ormode) {
            status = sixel_encode_body_ormode_pipeline(pixels,
                                                       width,
                                                       height,
                                                       palette_entries,
                                                       ormode_palette_float,
                                                       dither,
                                                       output,
                                                       pipeline_threads);
        } else {
            status = sixel_encode_body_pipeline(pixels,
                                                width,
                                                height,
                                                palette_entries,
                                                palette_entries_float32,
                                                dither,
                                                output,
                                                pipeline_threads);
        }
    } else if (output->ormode) {
        status = sixel_encode_body_ormode(input_pixels,
                                          width,
                                          height,
                                          palette_entries,
                                          ormode_palette_float,
                                          dither->ncolors,
                                          dither->keycolor,
                                          output);
    } else {
        status = sixel_encode_body(input_pixels,
                                   width,
                                   height,
                                   palette_entries,
                                   palette_entries_float32,
                                   dither->ncolors,
                                   dither->keycolor,
                                   dither->bodyonly,
                                   output,
                                   NULL,
                                   dither->allocator,
                                   dither->pipeline_pin_threads,
                                   logger != NULL ?
                                       logger :
                                       NULL);
    }

    if (!pipeline_active) {
        sixel_timeline_logger_logf(logger,
            "controller", "encode", "body_finish", -1);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encode_footer(output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    image_started = 0;

end:
    dither->pipeline_6delta_rgb101 = 0;
    dither->pipeline_6delta_hls = 0;
    dither->pipeline_6delta_hls_bytes = NULL;
    dither->pipeline_6delta_hls_count = 0U;
    if (image_started != 0 && SIXEL_FAILED(status)) {
        /*
         * Keep terminals from staying in DCS/SIXEL string state after a
         * late body error.  The original failure remains authoritative; the
         * best-effort close is only a stream recovery guard.
         */
        close_status = sixel_output_end_image(output);
        (void)close_status;
    }
    if (logger_owned) {
        dither->pipeline_logger = NULL;
        sixel_timeline_logger_unref(serial_logger);
    }
    if (palette_entries != NULL) {
        sixel_allocator_free(dither->allocator, palette_entries);
    }
    if (palette_entries_float32 != NULL) {
        sixel_allocator_free(dither->allocator, palette_entries_float32);
    }
    sixel_allocator_free(dither->allocator, paletted_pixels);
    output->transparent_mask = NULL;

    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_encode_dispatch(
    sixel_encoder_core_encode_request_t const *request)
{
    SIXELSTATUS status;

    if (request == NULL || request->pixels == NULL ||
        request->dither == NULL || request->output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->width < 1 || request->height < 1) {
        return SIXEL_BAD_INPUT;
    }

    (void)request->depth;
    if (request->dither->quality_mode == SIXEL_QUALITY_HIGHCOLOR) {
        status = sixel_encode_highcolor(request->pixels,
                                        request->width,
                                        request->height,
                                        request->dither,
                                        request->output);
    } else {
        status = sixel_encode_dither(request->pixels,
                                     request->width,
                                     request->height,
                                     request->dither,
                                     request->output);
    }

    return status;
}

SIXELAPI SIXELSTATUS
sixel_encode(
    unsigned char  /* in */ *pixels,   /* pixel bytes */
    int            /* in */ width,     /* image width */
    int            /* in */ height,    /* image height */
    int const      /* in */ depth,     /* color depth */
    sixel_dither_t /* in */ *dither,   /* dither context */
    sixel_output_t /* in */ *output)   /* output context */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_core_t *core;
    sixel_encoder_core_encode_request_t request;

    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad pixels parameter."
            " (pixels == NULL)");
        return SIXEL_BAD_ARGUMENT;
    }

    if (dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad dither parameter."
            " (dither == NULL)");
        return SIXEL_BAD_ARGUMENT;
    }

    if (output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad output parameter."
            " (output == NULL)");
        return SIXEL_BAD_ARGUMENT;
    }

    /* TODO: reference counting should be thread-safe */
    sixel_dither_ref(dither);
    sixel_output_ref(output);

    if (width < 1) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad width parameter."
            " (width < 1)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (height < 1) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad height parameter."
            " (height < 1)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    core = sixel_output_as_encoder_core(output);
    if (core == NULL || core->vtbl == NULL ||
        core->vtbl->encode == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    request.pixels = pixels;
    request.width = width;
    request.height = height;
    request.depth = depth;
    request.dither = dither;
    request.output = output;
    status = core->vtbl->encode(core, &request);

end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);

    return status;
}


/* A staged indexed packet uses the ordinary non-overpainting bit planes and
 * RLE encoder, but checkpoints classification and emission so small caller
 * budgets do not repeatedly restart a band. It never uses size-policy fill. */
enum {
    SIXEL_PACKET_USAGE,
    SIXEL_PACKET_HEADER,
    SIXEL_PACKET_DEFINITIONS,
    SIXEL_PACKET_CLEAR,
    SIXEL_PACKET_CLASSIFY,
    SIXEL_PACKET_COLOR,
    SIXEL_PACKET_COLUMNS,
    SIXEL_PACKET_FOOTER,
    SIXEL_PACKET_DONE
};

static int
sixel_indexed_packet_write(char *data, int size, void *priv)
{
    sixel_indexed_packet_t *packet;
    size_t copy;

    packet = priv;
    if (size < 0 || (size_t)size > SIZE_MAX - packet->bytes) {
        packet->byte_overflow = 1;
        return -1;
    }
    if (packet->bytes < packet->request.byte_limit) {
        copy = packet->request.byte_limit - packet->bytes;
        if (copy > (size_t)size) {
            copy = (size_t)size;
        }
        memcpy(packet->data + packet->bytes, data, copy);
    }
    packet->bytes += (size_t)size;
    return 0;
}

void
sixel_indexed_packet_free(sixel_indexed_packet_t *packet)
{
    sixel_allocator_t *allocator;

    if (packet == NULL) {
        return;
    }
    allocator = packet->allocator;
    sixel_output_unref(packet->output);
    sixel_allocator_free(allocator, packet->data);
    sixel_allocator_free(allocator, packet->map);
    sixel_allocator_free(allocator, packet);
    sixel_allocator_unref(allocator);
}

SIXELSTATUS
sixel_indexed_packet_new(sixel_indexed_packet_t **packet,
    sixel_indexed_packet_request_t const *request,
    sixel_allocator_t *allocator)
{
    sixel_indexed_packet_t *created;
    sixel_writer_controls_t controls;
    SIXELSTATUS status;

    if (packet == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *packet = NULL;
    if (request == NULL || allocator == NULL || request->width <= 0 ||
            request->height <= 0 || request->color_count == 0 ||
            request->color_count > 255 || request->indices == NULL ||
            request->paint == NULL || request->colors == NULL ||
            request->palette == NULL || request->byte_limit == 0 ||
            request->byte_limit > INT_MAX || (size_t)request->width >
                SIZE_MAX / (size_t)request->height ||
            (size_t)request->width > SIZE_MAX / request->color_count) {
        return SIXEL_BAD_ARGUMENT;
    }
    created = sixel_allocator_calloc(allocator, 1, sizeof(*created));
    if (created == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_allocator_ref(allocator);
    created->allocator = allocator;
    created->request = *request;
    created->count = (size_t)request->width * request->height;
    created->data = sixel_allocator_malloc(allocator, request->byte_limit);
    created->map = sixel_allocator_malloc(allocator,
        (size_t)request->width * request->color_count);
    if (created->data == NULL || created->map == NULL) {
        sixel_indexed_packet_free(created);
        return SIXEL_BAD_ALLOCATION;
    }
    status = sixel_encoder_core_create_output_from_factory(
        &created->output, sixel_indexed_packet_write,
                              created, allocator);
    if (status == SIXEL_OK) {
        memset(&controls, 0, sizeof(controls));
        controls.has_8bit_control = request->has_8bit_control != 0;
        status = sixel_output_set_writer_controls(created->output, &controls);
        created->output->has_gri_arg_limit = request->has_gri_arg_limit != 0;
    }
    if (status != SIXEL_OK) {
        sixel_indexed_packet_free(created);
        return status;
    }
    *packet = created;
    return SIXEL_OK;
}

static void
sixel_indexed_packet_literal(sixel_output_t *output, char const *text,
                              int length)
{
    int i;

    /* Keep the core buffer's reserved tail independent of a definition's
     * length. Each ordinary flush calls only the private counting writer. */
    for (i = 0; i < length; ++i) {
        sixel_putc(output->buffer + output->pos, (unsigned char)text[i]);
        sixel_advance(output, 1);
    }
}

SIXELSTATUS
sixel_indexed_packet_advance(sixel_indexed_packet_t *packet,
    size_t visit_limit, size_t *visits, int *done)
{
    sixel_indexed_packet_request_t const *request;
    sixel_palette_emitted_color_t const *color;
    sixel_output_t *output;
    size_t position;
    size_t map_size;
    size_t remaining;
    unsigned int index;
    unsigned int chunk;
    int rows;
    int x;
    int length;
    char definition[96];
    SIXELSTATUS status;

    if (packet == NULL || visits == NULL || done == NULL || visit_limit == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    *done = 0;
    request = &packet->request;
    output = packet->output;
    status = SIXEL_OK;
    for (chunk = 0; chunk < 256; ++chunk) {
        if (packet->stage == SIXEL_PACKET_DONE) {
            *done = 1;
            return SIXEL_OK;
        }
        if (*visits >= visit_limit) {
            return SIXEL_OK;
        }
        ++*visits;
        switch (packet->stage) {
        case SIXEL_PACKET_USAGE:
            position = packet->cursor++;
            if (request->paint[position]) {
                index = request->indices[position];
                if (index >= request->color_count) {
                    return SIXEL_BAD_INPUT;
                }
                packet->used[index] = 1;
                ++packet->painted;
            }
            if (packet->cursor == packet->count) {
                packet->cursor = 0;
                packet->stage = packet->painted ? SIXEL_PACKET_HEADER
                    : SIXEL_PACKET_DONE;
            }
            break;
        case SIXEL_PACKET_HEADER:
            status = sixel_output_begin_image(output, request->width,
                request->height, 0, 1, 0, 2, 1);
            packet->stage = SIXEL_PACKET_DEFINITIONS;
            break;
        case SIXEL_PACKET_DEFINITIONS:
            index = packet->color++;
            if (packet->used[index]) {
                packet->remap[index] = (unsigned char)packet->colors;
                color = request->colors + request->palette[index];
                length = snprintf(definition, sizeof(definition),
                    "#%u;%d;%d;%d;%d", packet->colors, color->type,
                    color->fields[0], color->fields[1], color->fields[2]);
                if (length < 0 || (size_t)length >= sizeof(definition)) {
                    return SIXEL_BAD_INPUT;
                }
                sixel_indexed_packet_literal(output, definition, length);
                ++packet->colors;
            }
            if (packet->color == request->color_count) {
                packet->stage = SIXEL_PACKET_CLEAR;
            }
            break;
        case SIXEL_PACKET_CLEAR:
            /* Charge clearing to the same visit budget, including empty
             * columns. The packet byte cap alone does not bound scan work. */
            map_size = (size_t)request->width * packet->colors;
            packet->map[packet->cursor++] = 0;
            if (packet->cursor == map_size) {
                for (index = 0; index < packet->colors; ++index) {
                    packet->maximum[index] = -1;
                }
                packet->cursor = 0;
                packet->stage = SIXEL_PACKET_CLASSIFY;
            }
            break;
        case SIXEL_PACKET_CLASSIFY:
            rows = request->height - packet->band;
            if (rows > 6) {
                rows = 6;
            }
            position = (size_t)packet->band * request->width + packet->cursor;
            if (request->paint[position]) {
                index = packet->remap[request->indices[position]];
                x = (int)(packet->cursor % request->width);
                packet->map[(size_t)index * request->width + x] |=
                    (unsigned char)(1U << (packet->cursor / request->width));
                if (x > packet->maximum[index]) {
                    packet->maximum[index] = x;
                }
            }
            if (++packet->cursor == (size_t)rows * request->width) {
                packet->cursor = 0;
                packet->color = 0;
                packet->first_color = 1;
                packet->stage = SIXEL_PACKET_COLOR;
            }
            break;
        case SIXEL_PACKET_COLOR:
            if (packet->color == packet->colors) {
                remaining = (size_t)(request->height - packet->band);
                if (remaining <= 6) {
                    packet->stage = SIXEL_PACKET_FOOTER;
                } else {
                    packet->band += 6;
                    sixel_indexed_packet_literal(output, "-", 1);
                    packet->stage = SIXEL_PACKET_CLEAR;
                }
            } else if (packet->maximum[packet->color] < 0) {
                ++packet->color;
            } else {
                if (!packet->first_color) {
                    sixel_indexed_packet_literal(output, "$", 1);
                }
                packet->first_color = 0;
                length = snprintf(definition, sizeof(definition),
                                   "#%u", packet->color);
                sixel_indexed_packet_literal(output, definition, length);
                packet->column = 0;
                packet->stage = SIXEL_PACKET_COLUMNS;
            }
            break;
        case SIXEL_PACKET_COLUMNS:
            status = sixel_put_pixel(output, packet->map[
                (size_t)packet->color * request->width + packet->column]);
            if (++packet->column > packet->maximum[packet->color]) {
                if (status == SIXEL_OK) {
                    status = sixel_put_flash(output);
                }
                ++packet->color;
                packet->stage = SIXEL_PACKET_COLOR;
            }
            break;
        case SIXEL_PACKET_FOOTER:
            status = sixel_encode_footer(output);
            packet->stage = SIXEL_PACKET_DONE;
            break;
        default:
            return SIXEL_LOGIC_ERROR;
        }
        if (status != SIXEL_OK || packet->byte_overflow) {
            return status != SIXEL_OK ? status : SIXEL_BAD_INTEGER_OVERFLOW;
        }
    }
    *done = packet->stage == SIXEL_PACKET_DONE;
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
