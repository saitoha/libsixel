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
# include "config.h"
#endif
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "filter-pipeline.h"
#include "frame-factory.h"
#include "pixelformat.h"
#include "threading.h"

#define SIXEL_FILTER_TILE_PIXELS 131072u
#define SIXEL_FILTER_TILE_QUEUE 2

int
sixel_filter_pipeline_eligible(int source, int target, size_t count,
    int threads)
{
#if SIXEL_ENABLE_THREADS
    return threads > 1 && count >= 2u * SIXEL_FILTER_TILE_PIXELS &&
        SIXEL_PIXELFORMAT_IS_FLOAT32(source) &&
        SIXEL_PIXELFORMAT_IS_FLOAT32(target);
#else
    (void)source;
    (void)target;
    (void)count;
    (void)threads;
    return 0;
#endif
}

#if SIXEL_ENABLE_THREADS
typedef struct sixel_filter_pipeline_context {
    sixel_filter_t consumer;
    sixel_frame_t *consumer_frame;
    sixel_frame_transparency_t transparency;
    sixel_allocator_t *allocator;
    sixel_timeline_logger_t *logger;
    float *pixels;
    size_t count;
    int pixelformat;
    int colorspace;
} sixel_filter_pipeline_context_t;

/* One consumer owns this frame and filter. The producer never reuses its
 * frame object, and each queued range remains disjoint until join. */
static int
sixel_filter_pipeline_consume(sixel_thread_pool_job_t job,
    void *userdata, void *workspace)
{
    sixel_filter_pipeline_context_t *ctx;
    sixel_frame_interface_t *object;
    sixel_frame_transparency_t transparency;
    sixel_thread_budget_scope_t budget;
    SIXELSTATUS status;
    size_t first, count, mask_count;
    unsigned char *mask;

    (void)workspace;
    ctx = userdata;
    first = (size_t)job.band_index * SIXEL_FILTER_TILE_PIXELS;
    count = ctx->count - first;
    if (count > SIXEL_FILTER_TILE_PIXELS) {
        count = SIXEL_FILTER_TILE_PIXELS;
    }
    status = sixel_frame_init_borrowed(ctx->consumer_frame,
        ctx->pixels + first * 3u, (int)count, 1,
        ctx->pixelformat, NULL, 0);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_frame_set_colorspace(ctx->consumer_frame, ctx->colorspace);
    /* The original mask is immutable. A tile's frame owns only its copied
     * mask prefix; samples beyond a short original mask remain visible. */
    if (ctx->transparency.transparent_mask != NULL &&
            first < ctx->transparency.transparent_mask_size) {
        mask_count = ctx->transparency.transparent_mask_size - first;
        if (mask_count > count) {
            mask_count = count;
        }
        mask = sixel_allocator_malloc(ctx->allocator, mask_count);
        if (mask == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(mask, ctx->transparency.transparent_mask + first, mask_count);
        transparency = ctx->transparency;
        transparency.transparent_mask = mask;
        transparency.transparent_mask_size = mask_count;
        object = (sixel_frame_interface_t *)ctx->consumer_frame;
        status = object->vtbl->set_transparency(object, &transparency);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(ctx->allocator, mask);
            return status;
        }
    }
    sixel_timeline_logger_logf(ctx->logger, "worker", "filter-pipeline",
        "consume-start", job.band_index);
    sixel_thread_budget_enter(&budget, 1);
    status = sixel_filter_run(&ctx->consumer, ctx->allocator, NULL);
    sixel_thread_budget_leave(&budget);
    sixel_timeline_logger_logf(ctx->logger, "worker", "filter-pipeline",
        "consume-finish", job.band_index);
    return status;
}
#endif

SIXELSTATUS
sixel_filter_pipeline_run(sixel_filter_t *first, sixel_filter_t *second,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger)
{
#if SIXEL_ENABLE_THREADS
    sixel_filter_pipeline_context_t ctx;
    sixel_filter_t producer;
    sixel_frame_t *frame, *producer_frame;
    sixel_frame_interface_t *object;
    sixel_frame_pixels_view_t view;
    sixel_frame_alpha_t alpha;
    sixel_thread_pool_create_request_t request;
    sixel_thread_pool_job_t job;
    sixel_threadpool_service_t *service;
    sixel_thread_pool_t *pool;
    sixel_thread_budget_scope_t budget;
    sixel_timeline_logger_t *owned_logger;
    void *service_object;
    SIXELSTATUS status, worker_status;
    size_t count, offset, length;
    int threads;

    if (first == NULL || second == NULL || allocator == NULL ||
            !(first->flags & SIXEL_FILTER_FLAG_FLOAT32_TILES) ||
            !(second->flags & SIXEL_FILTER_FLAG_FLOAT32_TILES) ||
            first->progress_cb != NULL || second->progress_cb != NULL ||
            first->input.slot == NULL || first->output.slot == NULL ||
            second->input.slot != first->output.slot ||
            second->output.slot != second->input.slot ||
            first->input.slot != first->output.slot ||
            first->output.pixelformat != second->input.pixelformat ||
            first->output.colorspace != second->input.colorspace ||
            second->output.pixelformat != first->output.pixelformat ||
            second->output.colorspace != first->output.colorspace) {
        return SIXEL_FALSE;
    }
    frame = *first->input.slot;
    if (frame == NULL) {
        return SIXEL_FALSE;
    }
    object = (sixel_frame_interface_t *)frame;
    if (SIXEL_FAILED(object->vtbl->get_pixels(object, &view)) ||
            SIXEL_FAILED(object->vtbl->get_alpha(object, &alpha)) ||
            alpha.values != NULL || view.width <= 0 || view.height <= 0 ||
            (size_t)view.width > SIZE_MAX / (size_t)view.height) {
        return SIXEL_FALSE;
    }
    count = (size_t)view.width * (size_t)view.height;
    threads = sixel_threads_resolve_fanout();
    if (!sixel_filter_pipeline_eligible(view.pixelformat,
            first->output.pixelformat, count, threads) ||
            view.pixels_float32 == NULL ||
            count > SIZE_MAX / (3u * sizeof(float)) ||
            count / SIXEL_FILTER_TILE_PIXELS > INT_MAX ||
            first->output.colorspace != sixel_pixelformat_get_colorspace(
                first->output.pixelformat)) {
        return SIXEL_FALSE;
    }
    memset(&ctx, 0, sizeof(ctx));
    producer_frame = NULL;
    pool = NULL;
    service_object = NULL;
    ctx.allocator = allocator;
    ctx.logger = logger;
    ctx.count = count;
    ctx.pixelformat = first->output.pixelformat;
    ctx.colorspace = first->output.colorspace;
    status = object->vtbl->get_transparency(object, &ctx.transparency);
    if (SIXEL_FAILED(status)) {
        return SIXEL_FALSE;
    }
    ctx.pixels = sixel_allocator_malloc(allocator,
        count * 3u * sizeof(float));
    if (ctx.pixels == NULL) {
        return SIXEL_FALSE;
    }
    /* Allocate before launching: frame references remain thread-local.
     * Scratch isolates the transaction while the two filters overlap. */
    status = sixel_frame_create_from_factory(&producer_frame, allocator);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_create_from_factory(&ctx.consumer_frame,
            allocator);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_components_getservice("services/threadpool",
            &service_object);
    }
    if (SIXEL_SUCCEEDED(status) && service_object != NULL) {
        service = service_object;
        memset(&request, 0, sizeof(request));
        request.threads = 1;
        request.queue_size = SIXEL_FILTER_TILE_QUEUE;
        request.worker = sixel_filter_pipeline_consume;
        request.userdata = &ctx;
        status = service->vtbl->create_pool(service, &request, &pool);
        service->vtbl->unref(service);
    }
    if (SIXEL_FAILED(status) || pool == NULL) {
        sixel_frame_unref(producer_frame);
        sixel_frame_unref(ctx.consumer_frame);
        sixel_allocator_free(allocator, ctx.pixels);
        return SIXEL_FALSE;
    }
    /* Copy only binding/progress state; pointwise filter userdata is immutable
     * and remains owned by the original components. Never dispose copies. */
    producer = *first;
    ctx.consumer = *second;
    sixel_filter_bind_input(&producer, &producer_frame,
        view.pixelformat, view.colorspace);
    sixel_filter_bind_output(&producer, &producer_frame,
        ctx.pixelformat, ctx.colorspace);
    sixel_filter_bind_input(&ctx.consumer, &ctx.consumer_frame,
        ctx.pixelformat, ctx.colorspace);
    sixel_filter_bind_output(&ctx.consumer, &ctx.consumer_frame,
        ctx.pixelformat, ctx.colorspace);
    /* Playback preparation can run without an encoder-local logger. Honor
     * the shared timeline service just as ordinary color conversion does. */
    owned_logger = NULL;
    if (logger == NULL) {
        (void)sixel_timeline_logger_prepare_env(allocator, &owned_logger);
        logger = owned_logger;
        ctx.logger = logger;
    }
    sixel_timeline_logger_logf(logger, "controller", "filter-pipeline",
        "start", threads);
    /* Reserve one existing slot for the consumer; conversion may fan out
     * only inside the remaining budget. No child can reclaim that slot. */
    sixel_thread_budget_enter(&budget, threads - 1);
    for (offset = 0; offset < count; offset += length) {
        length = count - offset;
        if (length > SIXEL_FILTER_TILE_PIXELS) {
            length = SIXEL_FILTER_TILE_PIXELS;
        }
        memcpy(ctx.pixels + offset * 3u, view.pixels_float32 + offset * 3u,
            length * 3u * sizeof(float));
        status = sixel_frame_init_borrowed(producer_frame,
            ctx.pixels + offset * 3u, (int)length, 1,
            view.pixelformat, NULL, 0);
        if (SIXEL_FAILED(status)) {
            break;
        }
        sixel_frame_set_colorspace(producer_frame, view.colorspace);
        job.band_index = (int)(offset / SIXEL_FILTER_TILE_PIXELS);
        sixel_timeline_logger_logf(logger, "controller", "filter-pipeline",
            "produce-start", job.band_index);
        status = sixel_filter_run(&producer, allocator, NULL);
        sixel_timeline_logger_logf(logger, "controller", "filter-pipeline",
            "produce-finish", job.band_index);
        if (SIXEL_FAILED(status)) {
            break;
        }
        status = pool->vtbl->push(pool, job);
        if (SIXEL_FAILED(status)) {
            break;
        }
    }
    pool->vtbl->finish(pool);
    worker_status = pool->vtbl->get_error(pool);
    pool->vtbl->unref(pool);
    sixel_thread_budget_leave(&budget);
    if (SIXEL_SUCCEEDED(status)) {
        status = worker_status;
    }
    sixel_frame_unref(producer_frame);
    sixel_frame_unref(ctx.consumer_frame);
    if (SIXEL_SUCCEEDED(status)) {
        /* Same float storage: after validation this setter only installs the
         * format tag. Conversion sees identical source/target coordinates. */
        sixel_frame_set_colorspace(frame, ctx.colorspace);
        status = sixel_frame_set_pixelformat(frame, ctx.pixelformat);
        if (SIXEL_SUCCEEDED(status)) {
            memcpy(view.pixels_float32, ctx.pixels,
                count * 3u * sizeof(float));
            first->progress.total_units = view.height;
            second->progress.total_units = view.height;
            sixel_filter_update_progress(first, view.height);
            sixel_filter_update_progress(second, view.height);
        } else {
            sixel_frame_set_colorspace(frame, view.colorspace);
        }
    }
    sixel_allocator_free(allocator, ctx.pixels);
    sixel_timeline_logger_logf(logger, "controller", "filter-pipeline",
        "finish", threads);
    if (SIXEL_FAILED(status)) {
        /* Replay only on failure, after join, from the untouched original.
         * This restores serial error precedence and caller-local details. */
        status = sixel_filter_run(first, allocator, logger);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_filter_run(second, allocator, logger);
        }
    }
    sixel_timeline_logger_unref(owned_logger);
    return status;
#else
    (void)first;
    (void)second;
    (void)allocator;
    (void)logger;
    return SIXEL_FALSE;
#endif
}
