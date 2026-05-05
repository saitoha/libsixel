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
#include "output-factory.h"
#include "dither.h"
#include "pixelformat.h"
#include "timeline-logger.h"
#include "threading.h"
#include "encoder-core-highcolor.h"
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

typedef struct sixel_parallel_dither_config {
    int enabled;
    int band_height;
    int overlap;
    int dither_threads;
    int encode_threads;
    int dither_env_override;
    int pin_threads;
} sixel_parallel_dither_config_t;

typedef struct sixel_encode_work {
    char *map;
    size_t map_size;
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
    int active_color_count;
} sixel_band_state_t;

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
    int ncolors;
    int keycolor;
    unsigned char *palstate;
    int encode_policy;
    sixel_allocator_t *allocator;
    sixel_output_t *output;
    int thread_count;
    int band_count;
    sixel_parallel_band_buffer_t *bands;
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
                              sixel_band_state_t *state);
static void sixel_band_clear_map(sixel_encode_work_t *work);
static SIXELSTATUS sixel_band_classify_row(sixel_encode_work_t *work,
                                           sixel_band_state_t *state,
                                           sixel_index_t *pixels,
                                           int width,
                                           int absolute_row,
                                           int ncolors,
                                           int keycolor,
                                           unsigned char *palstate,
                                           int encode_policy);
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

#if SIXEL_ENABLE_THREADS
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
#endif

static void
sixel_parallel_dither_configure(int height,
                                int ncolors,
                                int pipeline_threads,
                                int pin_threads,
                                sixel_parallel_dither_config_t *config)
{
    char const *text;
    long parsed;
    char *endptr;
    int band_height;
    int overlap;
    int dither_threads;
    int encode_threads;
    int dither_env_override;

    if (config == NULL) {
        return;
    }

    config->enabled = 0;
    config->band_height = 0;
    config->overlap = 0;
    config->dither_threads = 0;
    config->encode_threads = pipeline_threads;
    config->pin_threads = (pin_threads != 0) ? 1 : 0;

    if (pipeline_threads <= 1 || height <= 0) {
        return;
    }

    dither_env_override = 0;
    dither_threads = (pipeline_threads * 7 + 9) / 10;
    text = sixel_compat_getenv("SIXEL_DITHER_PARALLEL_THREADS_MAX");
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE && parsed > 0) {
            if (parsed > INT_MAX) {
                parsed = INT_MAX;
            }
            dither_threads = (int)parsed;
            dither_env_override = 1;
        }
    }
    if (dither_threads < 1) {
        dither_threads = 1;
    }
    if (dither_threads > pipeline_threads) {
        dither_threads = pipeline_threads;
    }

    if (!dither_env_override && pipeline_threads >= 4 && dither_threads < 2) {
        /*
         * When the total budget is ample, keep at least two dither workers so
         * the banded producer can feed the encoder fast enough to pipeline.
         */
        dither_threads = pipeline_threads - 2;
    }

    encode_threads = pipeline_threads - dither_threads;
    if (encode_threads < 2 && pipeline_threads > 2) {
        /*
         * Preserve a minimal pair of encoder workers to keep the pipeline
         * alive while leaving the rest to dithering. Small budgets fall back
         * to the serial encoder path later in the caller.
         */
        encode_threads = 2;
        dither_threads = pipeline_threads - encode_threads;
    }
    if (encode_threads < 1) {
        encode_threads = 1;
        dither_threads = pipeline_threads - encode_threads;
    }
    if (dither_threads < 1) {
        return;
    }

    /*
     * Choose the band height from the environment when present. Otherwise
     * split the image across the initial dither workers so each thread starts
     * with a single band. The result is rounded to a six-line multiple to
     * stay aligned with the encoder's natural cadence.
     */
    band_height = 0;
    text = sixel_compat_getenv("SIXEL_DITHER_PARALLEL_BAND_WIDTH");
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE && parsed > 0) {
            if (parsed > INT_MAX) {
                parsed = INT_MAX;
            }
            band_height = (int)parsed;
        }
    }
    if (band_height <= 0) {
        band_height = (height + dither_threads - 1) / dither_threads;
    }
    if (band_height < 6) {
        band_height = 6;
    }
    if ((band_height % 6) != 0) {
        band_height = ((band_height + 5) / 6) * 6;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_PARALLEL_BAND_OVERWRAP");
    /*
     * Default overlap favors quality for small palettes and speed when
     * colors are plentiful. The environment can override this policy.
     */
    if (ncolors <= 32) {
        overlap = 6;
    } else {
        overlap = 0;
    }
    if (text != NULL && text[0] != '\0') {
        errno = 0;
        parsed = strtol(text, &endptr, 10);
        if (endptr != text && errno != ERANGE && parsed >= 0) {
            if (parsed > INT_MAX) {
                parsed = INT_MAX;
            }
            overlap = (int)parsed;
        }
    }
    if (overlap < 0) {
        overlap = 0;
    }
    if (overlap > band_height / 2) {
        overlap = band_height / 2;
    }

    config->enabled = 1;
    config->band_height = band_height;
    config->overlap = overlap;
    config->dither_threads = dither_threads;
    config->encode_threads = encode_threads;
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
static SIXELSTATUS sixel_parallel_context_wait(sixel_parallel_context_t *ctx,
                                               int force_abort);
static void sixel_parallel_palette_row_ready(void *priv, int row_index);
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
    sixel_band_clear_map(&state->work);
    /* Parallel workers reset band-local buffers and output. */
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

    status = sixel_encode_work_allocate(&state->work,
                                        ctx->width,
                                        ctx->ncolors,
                                        ctx->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_encoder_core_create_output_from_factory(&state->output,
                                              sixel_parallel_band_writer,
                                              state,
                                              ctx->allocator);
    if (SIXEL_FAILED(status)) {
        sixel_encode_work_cleanup(&state->work, ctx->allocator);
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
    int threads;
    int i;

    if (ctx == NULL || pixels == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    ctx->pixels = pixels;
    ctx->width = width;
    ctx->height = height;
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

    nbands = (height + 5) / 6;
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

    notifier = (sixel_parallel_row_notifier_t *)priv;
    if (notifier == NULL) {
        return;
    }
    ctx = notifier->context;
    logger = notifier->logger;
    if (ctx == NULL || ctx->band_count <= 0 || ctx->height <= 0) {
        return;
    }
    if (row_index < 0) {
        return;
    }
    band_height = notifier->band_height;
    if (band_height < 1) {
        band_height = 6;
    }
    if ((row_index % band_height) != band_height - 1
            && row_index != ctx->height - 1) {
        return;
    }

    band_index = row_index / band_height;
    if (band_index >= ctx->band_count) {
        band_index = ctx->band_count - 1;
    }
    if (band_index < 0) {
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
    int row_index;
    int absolute_row;
    int last_row_index;

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
    band_height = ctx->height - band_start;
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

    for (row_index = 0; row_index < band_height; row_index++) {
        absolute_row = band_start + row_index;
        status = sixel_band_classify_row(&state->work,
                                         &state->band,
                                         ctx->pixels,
                                         ctx->width,
                                         absolute_row,
                                         ctx->ncolors,
                                         ctx->keycolor,
                                         ctx->palstate,
                                         ctx->encode_policy);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    status = sixel_band_compose(&state->work,
                                &state->band,
                                state->output,
                                ctx->width,
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

    if (state->output->pos > 0) {
        state->writer_error = sixel_output_write_bytes(
            state->output,
            (char *)state->output->buffer,
            state->output->pos);
        state->output->pos = 0;
    }
    if (state->writer_error != SIXEL_OK) {
        status = state->writer_error;
        goto cleanup;
    }

    sixel_band_finish(&state->work, &state->band);
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
    sixel_timeline_logger_t *logger;

    sixel_parallel_context_init(&ctx);
    sixel_timeline_logger_prepare_default(allocator, &logger);
    nbands = (height + 5) / 6;
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
    notifier.context = &ctx;
    notifier.logger = logger;
    notifier.band_height = 6;
    notifier.image_height = height;
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
        /*
         * Record the thread split and band geometry before spawning workers.
         * This clarifies why only a subset of hardware threads might appear
         * in the log when the encoder side is clamped to keep the pipeline
         * draining.
         */
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

    result = sixel_dither_apply_palette(dither, pixels, width, height);
    if (result == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }
    if (result != indexes) {
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
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
    if (!waited && ctx.pool != NULL) {
        wait_status = sixel_parallel_context_wait(&ctx, status != SIXEL_OK);
        if (status == SIXEL_OK) {
            status = wait_status;
        }
    }
    sixel_parallel_context_cleanup(&ctx);
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

/*
 * Compose helpers accelerate palette sweeps by skipping zero columns in
 * word-sized chunks.
 */

static int
sixel_compose_find_run_start(unsigned char const *row,
                             int width,
                             int start)
{
    int idx;
    size_t chunk_size;
    unsigned char const *cursor;
    unsigned long block;

    idx = start;
    chunk_size = sizeof(unsigned long);
    cursor = row + start;

    while ((width - idx) >= (int)chunk_size) {
        memcpy(&block, cursor, chunk_size);
        if (block != 0UL) {
            break;
        }
        idx += (int)chunk_size;
        cursor += chunk_size;
    }

    while (idx < width) {
        if (*cursor != 0) {
            break;
        }
        idx += 1;
        cursor += 1;
    }

    return idx;
}


static int
sixel_compose_measure_gap(unsigned char const *row,
                          int width,
                          int start,
                          int *reached_end)
{
    int gap;
    size_t chunk_size;
    unsigned char const *cursor;
    unsigned long block;
    int remaining;

    gap = 0;
    *reached_end = 0;
    if (start >= width) {
        *reached_end = 1;
        return gap;
    }

    chunk_size = sizeof(unsigned long);
    cursor = row + start;
    remaining = width - start;

    while (remaining >= (int)chunk_size) {
        memcpy(&block, cursor, chunk_size);
        if (block != 0UL) {
            break;
        }
        gap += (int)chunk_size;
        cursor += chunk_size;
        remaining -= (int)chunk_size;
    }

    while (remaining > 0) {
        if (*cursor != 0) {
            return gap;
        }
        gap += 1;
        cursor += 1;
        remaining -= 1;
    }

    *reached_end = 1;
    return gap;
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


/*
 * Walk a composed node and coalesce identical columns into runs so the
 * encoder core touches the repeat accumulator only once per symbol.
 */
static SIXELSTATUS
sixel_emit_span_from_map(sixel_output_t *output,
                         unsigned char const *map,
                         int length)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int index;
    int run_length;
    unsigned char value;
    size_t chunk_size;
    unsigned long pattern;
    unsigned long block;
    int chunk_mismatch;
    int remain;
    int byte_index;

    if (length <= 0) {
        return SIXEL_OK;
    }

    for (index = 0; index < length; index += run_length) {
        value = map[index];
        if (value > '?') {
            value = 0;
        }

        run_length = 1;
        chunk_size = sizeof(unsigned long);
        chunk_mismatch = 0;
        if (chunk_size > 1) {
            remain = length - (index + run_length);
            pattern = (~0UL / 0xffUL) * (unsigned long)value;

            while (remain >= (int)chunk_size) {
                memcpy(&block,
                       map + index + run_length,
                       chunk_size);
                block ^= pattern;
                if (block != 0UL) {
                    for (byte_index = 0;
                         byte_index < (int)chunk_size;
                         byte_index++) {
                        if ((block & 0xffUL) != 0UL) {
                            chunk_mismatch = 1;
                            break;
                        }
                        block >>= 8;
                        run_length += 1;
                    }
                    break;
                }
                run_length += (int)chunk_size;
                remain -= (int)chunk_size;
            }
        }

        if (!chunk_mismatch) {
            while (index + run_length < length) {
                unsigned char next;

                next = map[index + run_length];
                if (next > '?') {
                    next = 0;
                }
                if (next != value) {
                    break;
                }
                run_length += 1;
            }
        }

        status = sixel_emit_run(output,
                                 (int)value + '?',
                                 run_length);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

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

static void
sixel_node_del(sixel_output_t *output, sixel_node_t *np)
{
    sixel_node_t *tp;

    if ((tp = output->node_top) == np) {
        output->node_top = np->next;
    } else {
        while (tp->next != NULL) {
            if (tp->next == np) {
                tp->next = np->next;
                break;
            }
            tp = tp->next;
        }
    }

    np->next = output->node_free;
    output->node_free = np;
}


static SIXELSTATUS
sixel_put_node(
    sixel_output_t /* in */     *output,  /* output context */
    int            /* in/out */ *x,       /* header position */
    sixel_node_t   /* in */     *np,      /* node object */
    int            /* in */     ncolors,  /* number of palette colors */
    int            /* in */     keycolor) /* transparent color number */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int nwrite;

    if (ncolors != 2 || keycolor == (-1)) {
        /* designate palette index */
        if (output->active_palette != np->pal) {
            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos, np->pal);
            sixel_advance(output, nwrite);
            output->active_palette = np->pal;
        }
    }

    if (*x < np->sx) {
        int span;

        span = np->sx - *x;
        status = sixel_emit_run(output, '?', span);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        *x = np->sx;
    }

    if (*x < np->mx) {
        int span;

        span = np->mx - *x;
        status = sixel_emit_span_from_map(output,
                                          (unsigned char const *)np->map + *x,
                                          span);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        *x = np->mx;
    }

    status = sixel_put_flash(output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return status;
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
    } else if (keycolor >= 0) {
        /*
         * When a transparent keycolor is in use, request transparent
         * background mode so untouched pixels keep the terminal background.
         */
        p[1] = 1;
    }

    output->pos = 0;

    if (p[2] == 0) {
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
sixel_palette_float32_matches_u8(unsigned char const *palette,
                                 float const *palette_float,
                                 size_t count,
                                 int float_pixelformat)
{
    size_t index;
    size_t limit;
    unsigned char expected;
    int channel;

    if (palette == NULL || palette_float == NULL || count == 0U) {
        return 1;
    }
    if (count > SIZE_MAX / 3U) {
        return 0;
    }
    limit = count * 3U;
    for (index = 0U; index < limit; ++index) {
        channel = (int)(index % 3U);
        expected = sixel_pixelformat_float_channel_to_byte(
            float_pixelformat,
            channel,
            palette_float[index]);
        if (palette[index] != expected) {
            return 0;
        }
    }
    return 1;
}

static void
sixel_palette_sync_float32_from_u8(unsigned char const *palette,
                                   float *palette_float,
                                   size_t count,
                                   int float_pixelformat)
{
    size_t index;
    size_t limit;
    int channel;

    if (palette == NULL || palette_float == NULL || count == 0U) {
        return;
    }
    if (count > SIZE_MAX / 3U) {
        return;
    }
    limit = count * 3U;
    for (index = 0U; index < limit; ++index) {
        channel = (int)(index % 3U);
        palette_float[index] = sixel_pixelformat_byte_to_float(
            float_pixelformat,
            channel,
            palette[index]);
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
    double r;
    double g;
    double b;
    double maxc;
    double minc;
    double lightness;
    double saturation;
    double hue;
    double diff;
    int h;
    int l;
    int s;
    int nwrite;

    if (n != keycolor) {
        r = sixel_output_palette_channel_to_float(palette,
                                                  palette_float,
                                                  n,
                                                  0);
        g = sixel_output_palette_channel_to_float(palette,
                                                  palette_float,
                                                  n,
                                                  1);
        b = sixel_output_palette_channel_to_float(palette,
                                                  palette_float,
                                                  n,
                                                  2);
        maxc = r > g ? (r > b ? r : b) : (g > b ? g : b);
        minc = r < g ? (r < b ? r : b) : (g < b ? g : b);
        lightness = (maxc + minc) * 0.5;
        saturation = 0.0;
        hue = 0.0;
        diff = maxc - minc;
        if (diff <= 0.0) {
            h = 0;
            s = 0;
        } else {
            if (lightness < 0.5) {
                saturation = diff / (maxc + minc);
            } else {
                saturation = diff / (2.0 - maxc - minc);
            }
            if (maxc == r) {
                hue = (g - b) / diff;
            } else if (maxc == g) {
                hue = 2.0 + (b - r) / diff;
            } else {
                hue = 4.0 + (r - g) / diff;
            }
            hue *= 60.0;
            if (hue < 0.0) {
                hue += 360.0;
            }
            if (hue >= 360.0) {
                hue -= 360.0;
            }
            /*
             * The DEC HLS color wheel used by DECGCI considers
             * hue==0 to be blue instead of red.  Rotate the hue by
             * +120 degrees so that RGB primaries continue to match
             * the historical img2sixel output and VT340 behavior.
             */
            hue += 120.0;
            while (hue >= 360.0) {
                hue -= 360.0;
            }
            while (hue < 0.0) {
                hue += 360.0;
            }
            s = (int)(saturation * 100.0 + 0.5);
            if (s < 0) {
                s = 0;
            } else if (s > 100) {
                s = 100;
            }
            h = (int)(hue + 0.5);
            if (h >= 360) {
                h -= 360;
            } else if (h < 0) {
                h = 0;
            }
        }
        l = (int)(lightness * 100.0 + 0.5);
        if (l < 0) {
            l = 0;
        } else if (l > 100) {
            l = 100;
        }
        /* DECGCI Graphics Color Introducer  # Pc ; Pu; Px; Py; Pz */
        sixel_putc(output->buffer + output->pos, '#');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, n);
        sixel_advance(output, nwrite);
        sixel_puts(output->buffer + output->pos, ";1;", 3);
        sixel_advance(output, 3);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, h);
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos, ';');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, l);
        sixel_advance(output, nwrite);
        sixel_putc(output->buffer + output->pos, ';');
        sixel_advance(output, 1);
        nwrite = sixel_putnum((char *)output->buffer + output->pos, s);
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
    size_t active_colors_size;
    size_t active_color_index_size;

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
    state->active_color_count = 0;
}

static void
sixel_band_finish(sixel_encode_work_t *work, sixel_band_state_t *state)
{
    int color_index;
    int c;

    if (work->active_colors == NULL
        || work->active_color_index == NULL) {
        state->active_color_count = 0;
        return;
    }

    for (color_index = 0;
         color_index < state->active_color_count;
         color_index++) {
        c = work->active_color_index[color_index];
        if (c >= 0
            && (size_t)c < work->active_colors_size) {
            work->active_colors[c] = 0;
        }
    }
    state->active_color_count = 0;
}

static void
sixel_band_clear_map(sixel_encode_work_t *work)
{
    if (work->map != NULL && work->map_size > 0) {
        memset(work->map, 0, work->map_size);
    }
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

    if (bodyonly || (ncolors == 2 && keycolor != (-1))) {
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

static SIXELSTATUS
sixel_band_classify_row(sixel_encode_work_t *work,
                        sixel_band_state_t *state,
                        sixel_index_t *pixels,
                        int width,
                        int absolute_row,
                        int ncolors,
                        int keycolor,
                        unsigned char *palstate,
                        int encode_policy)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int row_bit;
    int band_start;
    int pix;
    int x;
    int check_integer_overflow;
    char *map;
    unsigned char *active_colors;
    int *active_color_index;

    map = work->map;
    active_colors = work->active_colors;
    active_color_index = work->active_color_index;
    row_bit = state->row_in_band;
    band_start = absolute_row - row_bit;

    if (row_bit == 0) {
        if (encode_policy != SIXEL_ENCODEPOLICY_SIZE) {
            state->fillable = 0;
        } else if (palstate) {
            if (width > 0) {
                pix = pixels[band_start * width];
                if (pix >= ncolors) {
                    state->fillable = 0;
                } else {
                    state->fillable = 1;
                }
            } else {
                state->fillable = 0;
            }
        } else {
            state->fillable = 1;
        }
        state->active_color_count = 0;
    }

    for (x = 0; x < width; x++) {
        if (absolute_row > INT_MAX / width) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: integer overflow detected."
                " (y > INT_MAX)");
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        check_integer_overflow = absolute_row * width;
        if (check_integer_overflow > INT_MAX - x) {
            sixel_helper_set_additional_message(
                "sixel_encode_body: integer overflow detected."
                " (y * width > INT_MAX - x)");
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        pix = pixels[check_integer_overflow + x];
        if (pix >= 0 && pix < ncolors && pix != keycolor) {
            if (pix > INT_MAX / width) {
                sixel_helper_set_additional_message(
                    "sixel_encode_body: integer overflow detected."
                    " (pix > INT_MAX / width)");
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto end;
            }
            check_integer_overflow = pix * width;
            if (check_integer_overflow > INT_MAX - x) {
                sixel_helper_set_additional_message(
                    "sixel_encode_body: integer overflow detected."
                    " (pix * width > INT_MAX - x)");
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto end;
            }
            map[pix * width + x] |= (1 << row_bit);
            if (active_colors != NULL && active_colors[pix] == 0) {
                active_colors[pix] = 1;
                if (state->active_color_count < ncolors
                    && active_color_index != NULL) {
                    active_color_index[state->active_color_count] = pix;
                    state->active_color_count += 1;
                }
            }
        } else if (!palstate) {
            state->fillable = 0;
        }
    }

    state->row_in_band += 1;
    status = SIXEL_OK;

end:
    return status;
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
    int gap;
    int gap_reached_end;
    sixel_node_t *np;
    sixel_node_t *column_head;
    sixel_node_t *column_tail;
    sixel_node_t top;

    (void)ncolors;
    (void)keycolor;
    row = NULL;
    gap_reached_end = 0;
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
        sx = 0;
        while (sx < width) {
            sx = sixel_compose_find_run_start(
                row,
                width,
                sx);
            if (sx >= width) {
                break;
            }

            mx = sx + 1;
            while (mx < width) {
                if (row[mx] != 0) {
                    mx += 1;
                    continue;
                }

                gap = sixel_compose_measure_gap(
                    row,
                    width,
                    mx + 1,
                    &gap_reached_end);
                if (gap >= 9 || gap_reached_end) {
                    break;
                }
                mx += gap + 1;
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

            sx = mx;
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
                   (1 << state->row_in_band) - 1,
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
        next = np->next;
        sixel_node_del(output, np);
        np = next;

        while (np != NULL) {
            if (np->sx < x) {
                np = np->next;
                continue;
            }

            if (state->fillable) {
                memset(np->map + np->sx,
                       (1 << state->row_in_band) - 1,
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
            next = np->next;
            sixel_node_del(output, np);
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
    int row_index;
    int absolute_row;
    int last_row_index;
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

        nbands = (height + 5) / 6;
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
                                        width,
                                        ncolors,
                                        allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    band_start = 0;
    while (band_start < height) {
        band_height = height - band_start;
        if (band_height > 6) {
            band_height = 6;
        }

        band.row_in_band = 0;
        band.fillable = 0;
        band.active_color_count = 0;

        if (logging_active) {
            sixel_timeline_logger_logf(logger,
                              "worker",
                              "encode",
                              "start",
                              job_index);
        }

        for (row_index = 0; row_index < band_height; row_index++) {
            absolute_row = band_start + row_index;
            status = sixel_band_classify_row(&work,
                                             &band,
                                             pixels,
                                             width,
                                             absolute_row,
                                             ncolors,
                                             keycolor,
                                             palstate,
                                             output->encode_policy);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
        }

        status = sixel_band_compose(&work,
                                    &band,
                                    output,
                                    width,
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

        sixel_band_finish(&work, &band);

        sixel_band_clear_map(&work);

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

static SIXELSTATUS
sixel_encode_body_ormode(
    uint8_t             /* in */ *pixels,
    int                 /* in */ width,
    int                 /* in */ height,
    unsigned char       /* in */ *palette,
    int                 /* in */ ncolors,
    int                 /* in */ keycolor,
    sixel_output_t      /* in */ *output)
{
    SIXELSTATUS status;
    int n;
    int nplanes;
    uint8_t *buf;
    uint8_t *buf_p;
    int x;
    int cur_h;
    int nwrite;
    int plane;

    if (pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    buf = pixels;

    for (n = 0; n < ncolors; n++) {
        status = output_rgb_palette_definition(output, palette, NULL, n, keycolor);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    for (nplanes = 8; nplanes > 1; nplanes--) {
        if (ncolors > (1 << (nplanes - 1))) {
            break;
        }
    }

    for (cur_h = 6; cur_h <= height; cur_h += 6) {
        for (plane = 0; plane < nplanes; plane++) {
            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos, 1 << plane);
            sixel_advance(output, nwrite);

            buf_p = buf;
            for (x = 0; x < width; x++, buf_p++) {
                sixel_put_pixel(output,
                                ((buf_p[0] >> plane) & 0x1) |
                                (((buf_p[width] >> plane) << 1) & 0x2) |
                                (((buf_p[width * 2] >> plane) << 2) & 0x4) |
                                (((buf_p[width * 3] >> plane) << 3) & 0x8) |
                                (((buf_p[width * 4] >> plane) << 4) & 0x10) |
                                (((buf_p[width * 5] >> plane) << 5) & 0x20));
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
        buf += (width * 6);
	}

    if (cur_h - height < 6) {
        for (plane = 0; plane < nplanes; plane++) {
            sixel_putc(output->buffer + output->pos, '#');
            sixel_advance(output, 1);
            nwrite = sixel_putnum((char *)output->buffer + output->pos, 1 << plane);
            sixel_advance(output, nwrite);

            buf_p = buf;
            for (x = 0; x < width; x++, buf_p++) {
                int pix = ((buf_p[0] >> plane) & 0x1);

                switch(cur_h - height) {
                case 1:
                    pix |= (((buf_p[width * 4] >> plane) << 4) & 0x10);
                    /* Fall through */
                case 2:
                    pix |= (((buf_p[width * 3] >> plane) << 3) & 0x8);
                    /* Fall through */
                case 3:
                    pix |= (((buf_p[width * 2] >> plane) << 2) & 0x4);
                    /* Fall through */
                case 4:
                    pix |= (((buf_p[width] >> plane) << 1) & 0x2);
                    /* Fall through */
                default:
                    break;
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
    }

    return 0;
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
    int palette_source_colorspace;
    int palette_float_pixelformat;
    int output_float_pixelformat;
    int palette_float_depth;
    int pipeline_active;
    int pipeline_threads = 0;  /* set to a deterministic default before use */
    int pipeline_nbands;
    sixel_parallel_dither_config_t dither_parallel;
    char const *band_env_text;
    int ormode_enabled;
    sixel_palette_entries_view_t palette_view;
    sixel_palette_float32_entries_view_t palette_float_view;
#if SIXEL_ENABLE_THREADS
    sixel_timeline_logger_t *serial_logger;
    int logger_owned = 0;
#endif  /* SIXEL_ENABLE_THREADS */
    sixel_timeline_logger_t *logger = NULL;

    if (output == NULL || dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

#if SIXEL_ENABLE_THREADS
    serial_logger = NULL;
#endif  /* SIXEL_ENABLE_THREADS */
    palette_source_colorspace = SIXEL_COLORSPACE_GAMMA;
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

    pipeline_active = 0;
#if SIXEL_ENABLE_THREADS
    #endif
    dither_parallel.enabled = 0;
    dither_parallel.band_height = 0;
    dither_parallel.overlap = 0;
    dither_parallel.dither_threads = 0;
    dither_parallel.encode_threads = 0;
    ormode_enabled = output != NULL && output->ormode != 0;
    /*
     * Normalize the planner-provided pinning request so both palette and
     * encode workers see the same 0/1 flag.
     */
    dither->pipeline_pin_threads =
        dither->pipeline_pin_threads != 0 ? 1 : 0;
    dither_parallel.pin_threads = dither->pipeline_pin_threads;
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
        pipeline_threads = sixel_threads_resolve();
        band_env_text = sixel_compat_getenv(
            "SIXEL_DITHER_PARALLEL_BAND_WIDTH");
        if (pipeline_threads <= 1 && band_env_text != NULL
                && band_env_text[0] != '\0') {
            /*
             * Parallel band dithering was explicitly requested via the
             * environment.  When SIXEL_THREADS is absent, prefer hardware
             * concurrency instead of silently running a single worker so
             * that multiple dither jobs appear in the log.
             */
            pipeline_threads = sixel_threads_normalize(0);
        }
        pipeline_nbands = (height + 5) / 6;
        /*
         * Ormode encoding expects a palette-index buffer in a single,
         * contiguous memory block. The palette-apply pipeline emits
         * six-line jobs and leaves input_pixels unset until the encode
         * stage pulls from the job queue, so forcing it on would leave a
         * NULL input buffer and can crash when multiple encode threads
         * run. Disable the pipeline when ormode is active to keep the
         * data flow deterministic and thread-safe.
         */
        if (!ormode_enabled
                && pipeline_threads > 1
                && pipeline_nbands > 1) {
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
        sixel_parallel_dither_configure(height,
                                        dither->ncolors,
                                        pipeline_threads,
                                        dither->pipeline_pin_threads,
                                        &dither_parallel);
        if (dither_parallel.enabled) {
            dither->pipeline_parallel_active = 1;
            dither->pipeline_band_height = dither_parallel.band_height;
            dither->pipeline_band_overlap = dither_parallel.overlap;
            dither->pipeline_dither_threads =
                dither_parallel.dither_threads;
            pipeline_threads = dither_parallel.encode_threads;
        }
        if (pipeline_threads <= 1) {
            /*
             * Disable the pipeline when the encode side cannot spawn at
             * least two workers.  A single encode thread cannot consume the
             * six-line jobs that PaletteApply produces, so fall back to the
             * serialized encoder path.
             */
            pipeline_active = 0;
            dither->pipeline_parallel_active = 0;
            if (paletted_pixels == NULL) {
                paletted_pixels = sixel_dither_apply_palette(dither, pixels,
                                                             width, height);
                if (paletted_pixels == NULL) {
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
            }
            input_pixels = paletted_pixels;
        }
    }

#if SIXEL_ENABLE_THREADS
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
                              -1,
                              -1,
                              0,
                              height,
                              0,
                              height,
                              "serial path threads=1");
        }
    }
#endif

    if (output != NULL) {
        palette_source_colorspace = output->source_colorspace;
        palette_float_pixelformat =
            sixel_palette_float_pixelformat_for_colorspace(
                palette_source_colorspace);
        palette_float_depth =
            sixel_helper_compute_depth(palette_float_pixelformat);
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
    if (palette_entries != NULL && palette_entries_float32 != NULL
            && palette_count == palette_float_count
            && palette_count > 0U
            && !sixel_palette_float32_matches_u8(
                    palette_entries,
                    palette_entries_float32,
                    palette_count,
                    palette_float_pixelformat)) {
        sixel_palette_sync_float32_from_u8(palette_entries,
                                           palette_entries_float32,
                                           palette_count,
                                           palette_float_pixelformat);
    }
    if (palette_entries != NULL && palette_count > 0U
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
    if (SIXEL_FAILED(status) || palette_entries == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encode_dither: palette copy failed.");
        goto end;
    }

    status = sixel_encode_header(width, height, dither->keycolor, output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (output->ormode) {
        status = sixel_encode_body_ormode(input_pixels,
                                          width,
                                          height,
                                          palette_entries,
                                          dither->ncolors,
                                          dither->keycolor,
                                          output);
    } else if (pipeline_active) {
        status = sixel_encode_body_pipeline(pixels,
                                            width,
                                            height,
                                            palette_entries,
                                            palette_entries_float32,
                                            dither,
                                            output,
                                            pipeline_threads);
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

    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encode_footer(output);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
#if SIXEL_ENABLE_THREADS
    if (logger_owned) {
        dither->pipeline_logger = NULL;
        sixel_timeline_logger_unref(serial_logger);
    }
#endif
    if (palette_entries != NULL) {
        sixel_allocator_free(dither->allocator, palette_entries);
    }
    if (palette_entries_float32 != NULL) {
        sixel_allocator_free(dither->allocator, palette_entries_float32);
    }
    sixel_allocator_free(dither->allocator, paletted_pixels);

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

    (void) depth;

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

    if (dither->quality_mode == SIXEL_QUALITY_HIGHCOLOR) {
        status = sixel_encode_highcolor(pixels, width, height,
                                        dither, output);
    } else {
        status = sixel_encode_dither(pixels, width, height,
                                     dither, output);
    }

end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);

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
