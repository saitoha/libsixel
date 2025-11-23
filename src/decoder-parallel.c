/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#include "config.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "assessment.h"
#include "decoder-image.h"
#include "decoder-parallel.h"
#include "decoder-prescan.h"
#include "fillrun.h"
#include "output.h"
#if SIXEL_ENABLE_THREADS
# include "sixel_threading.h"
# include "threadpool.h"
#endif

#define SIXEL_PARALLEL_MIN_BYTES   2048
#define SIXEL_PARALLEL_MAX_REPEAT  0xffff
#define SIXEL_PARALLEL_PALVAL(n, a, m) \
    (((n) * (a) + ((m) / 2)) / (m))
#define SIXEL_PARALLEL_RGBA(r, g, b, a) \
    (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | \
     ((uint32_t)(b) << 8) | ((uint32_t)(a)))
#define SIXEL_PARALLEL_XRGB(r, g, b) \
    SIXEL_PARALLEL_RGBA(SIXEL_PARALLEL_PALVAL((r), 255, 100), \
                        SIXEL_PARALLEL_PALVAL((g), 255, 100), \
                        SIXEL_PARALLEL_PALVAL((b), 255, 100), \
                        255)
#define SIXEL_PARALLEL_CHECKPOINT_INTERVAL 8
#define SIXEL_PARALLEL_COALESCE_ALPHA 2
#define SIXEL_PARALLEL_COALESCE_MIN_TARGET 64

typedef struct sixel_decoder_thread_config {
    int env_checked;
    int env_valid;
    int env_threads;
    int override_active;
    int override_threads;
} sixel_decoder_thread_config_t;

static sixel_decoder_thread_config_t g_decoder_threads = {
    0,
    0,
    1,
    0,
    1
};

typedef struct sixel_parallel_metrics {
    int enabled;
    int need_lock;
    int lock_initialized;
    double started_at;
    double prescan_done_at;
    double prepare_done_at;
    double dispatch_started_at;
    double dispatch_done_at;
    double wait_done_at;
#if SIXEL_ENABLE_THREADS
    sixel_mutex_t lock;
#endif
    double decode_ms_sum;
    double decode_ms_peak;
} sixel_parallel_metrics_t;

typedef struct sixel_parallel_checkpoint {
    int band_index;
    sixel_prescan_band_state_t state;
} sixel_parallel_checkpoint_t;

typedef struct sixel_parallel_band_plan sixel_parallel_band_plan_t;
typedef struct sixel_parallel_decode_plan sixel_parallel_decode_plan_t;

typedef struct sixel_parallel_band_meta {
    size_t start_offset;
    size_t end_offset;
    int checkpoint_index;
    int event_start;
    int event_count;
    int weight_tokens;
    int weight_repeats;
} sixel_parallel_band_meta_t;

struct sixel_parallel_band_plan {
    sixel_parallel_checkpoint_t *checkpoints;
    sixel_band_event_t *events;
    sixel_parallel_band_meta_t *bands;
    int checkpoint_capacity;
    int event_capacity;
    int band_capacity;
    int checkpoint_count;
    int event_count;
    int band_count;
    int checkpoint_interval;
};

typedef struct sixel_parallel_coalesced_job sixel_parallel_coalesced_job_t;

typedef struct sixel_parallel_plan_inputs {
    sixel_parallel_band_plan_t *band_plan;
    sixel_parallel_coalesced_job_t *jobs;
    sixel_prescan_t *prescan;
    sixel_prescan_band_state_t final_state;
    int width;
    int height;
    int bgindex;
    int band_count;
    int job_count;
    int have_final_state;
    int flags;
} sixel_parallel_plan_inputs_t;

struct sixel_parallel_coalesced_job {
    int first_band;
    int band_count;
    int weight_tokens;
    int weight_repeats;
};

typedef struct sixel_parallel_coalescer_state {
    sixel_parallel_coalesced_job_t *jobs;
    int job_capacity;
    int job_count;
    int owns_jobs;
    int target_weight;
    int outstanding_goal;
    sixel_allocator_t *allocator;
    int job_start;
    int job_bands;
    int job_tokens;
    int job_repeats;
    int job_weight;
} sixel_parallel_coalescer_state_t;

static SIXELSTATUS sixel_parallel_coalescer_pad_jobs(
    sixel_parallel_coalescer_state_t *coalescer,
    sixel_parallel_band_plan_t const *plan,
    int job_goal,
    sixel_allocator_t *allocator);

struct sixel_parallel_decode_plan {
    unsigned char *data;
    int len;
    image_buffer_t *image;
    sixel_allocator_t *allocator;
    sixel_prescan_t *prescan;
    sixel_parallel_band_plan_t *band_plan;
    sixel_parallel_coalesced_job_t *jobs;
    sixel_parallel_metrics_t *metrics;
    sixel_prescan_band_state_t const *final_state;
    int depth;
    int threads;
    int direct_mode;
    int width;
    int height;
    int *band_color_max;
    int band_count;
    int job_count;
    int jobs_owned;
};

typedef SIXELSTATUS (*sixel_parallel_plan_builder_t)(
    unsigned char *p,
    int len,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_metrics_t *metrics,
    sixel_parallel_plan_inputs_t *inputs);

static SIXELSTATUS sixel_parallel_safe_addition(
    sixel_prescan_band_state_t *state,
    unsigned char value);

static uint32_t sixel_parallel_hls_to_rgba(int hue, int lum, int sat);
static int sixel_parallel_env_force_prescan(void);
static void sixel_parallel_band_plan_destroy(
    sixel_parallel_band_plan_t *plan,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_band_plan_from_prescan(
    sixel_prescan_t *prescan,
    sixel_allocator_t *allocator,
    sixel_parallel_band_plan_t **out_plan);
static SIXELSTATUS sixel_parallel_plan_onepass(
    unsigned char *p,
    int len,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_metrics_t *metrics,
    sixel_parallel_plan_inputs_t *inputs);
static SIXELSTATUS sixel_parallel_band_plan_create(
    int checkpoint_capacity,
    int event_capacity,
    int band_capacity,
    int checkpoint_interval,
    sixel_allocator_t *allocator,
    sixel_parallel_band_plan_t **out_plan);
static SIXELSTATUS sixel_parallel_band_plan_append_event(
    sixel_parallel_band_plan_t *plan,
    sixel_band_event_t const *event,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_band_plan_append_checkpoint(
    sixel_parallel_band_plan_t *plan,
    sixel_prescan_band_state_t const *state,
    int band_index,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_band_plan_adopt_events(
    sixel_parallel_band_plan_t *plan,
    sixel_prescan_t *prescan,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_band_plan_append_band(
    sixel_parallel_band_plan_t *plan,
    size_t start_offset,
    size_t end_offset,
    int checkpoint_index,
    int event_start,
    int event_count,
    int weight_tokens,
    int weight_repeats,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_coalescer_init(
    sixel_parallel_coalescer_state_t *state,
    int target_weight,
    int outstanding_goal,
    sixel_parallel_coalesced_job_t *jobs,
    int job_capacity,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_coalesce_once(
    sixel_parallel_band_plan_t *band_plan,
    int target_weight,
    sixel_parallel_coalesced_job_t *jobs,
    int job_capacity,
    int *out_job_count,
    sixel_allocator_t *allocator);
static int sixel_parallel_coalescer_initial_target(int len, int threads);
static SIXELSTATUS sixel_parallel_coalescer_push(
    sixel_parallel_coalescer_state_t *state,
    sixel_parallel_band_meta_t *meta,
    int band_index);
static SIXELSTATUS sixel_parallel_coalescer_flush(
    sixel_parallel_coalescer_state_t *state);
static void sixel_parallel_coalescer_destroy(
    sixel_parallel_coalescer_state_t *state,
    sixel_allocator_t *allocator);
static SIXELSTATUS sixel_parallel_coalesce_jobs(
    sixel_parallel_band_plan_t *band_plan,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_coalesced_job_t **out_jobs,
    int *out_job_count);
static SIXELSTATUS sixel_parallel_run_workers(
    sixel_parallel_decode_plan_t *plan,
    sixel_parallel_metrics_t *metrics);
static SIXELSTATUS sixel_parallel_finalize_palette(image_buffer_t *image,
                                                   int *band_color_max,
                                                   int band_count);
static void sixel_parallel_apply_final_palette(
    image_buffer_t *image,
    sixel_prescan_band_state_t const *state);
static SIXELSTATUS sixel_parallel_plan_from_prescan(
    unsigned char *p,
    int len,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_metrics_t *metrics,
    sixel_parallel_plan_inputs_t *inputs);
static SIXELSTATUS sixel_parallel_decode_execute(
    sixel_parallel_plan_inputs_t *inputs,
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int threads,
    sixel_parallel_metrics_t *metrics,
    int *used_parallel);
static void sixel_parallel_plan_inputs_reset(
    sixel_parallel_plan_inputs_t *inputs);
static void sixel_parallel_plan_inputs_destroy(
    sixel_parallel_plan_inputs_t *inputs,
    sixel_allocator_t *allocator);

static int
sixel_decoder_threads_token_is_auto(char const *text)
{
    if (text == NULL) {
        return 0;
    }
    if ((text[0] == 'a' || text[0] == 'A') &&
            (text[1] == 'u' || text[1] == 'U') &&
            (text[2] == 't' || text[2] == 'T') &&
            (text[3] == 'o' || text[3] == 'O') &&
            text[4] == '\0') {
        return 1;
    }
    return 0;
}

static int
sixel_decoder_threads_normalize(int requested)
{
    int normalized;

#if SIXEL_ENABLE_THREADS
    int hw_threads;

    if (requested <= 0) {
        hw_threads = sixel_get_hw_threads();
        if (hw_threads < 1) {
            hw_threads = 1;
        }
        normalized = hw_threads;
    } else {
        normalized = requested;
    }
#else
    (void)requested;
    normalized = 1;
#endif
    if (normalized < 1) {
        normalized = 1;
    }
    return normalized;
}

static int
sixel_decoder_threads_parse_value(char const *text, int *value)
{
    long parsed;
    char *endptr;
    int normalized;

    if (text == NULL || value == NULL) {
        return 0;
    }
    if (sixel_decoder_threads_token_is_auto(text)) {
        normalized = sixel_decoder_threads_normalize(0);
        *value = normalized;
        return 1;
    }
    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || errno == ERANGE) {
        return 0;
    }
    if (parsed < 1) {
        normalized = sixel_decoder_threads_normalize(1);
    } else if (parsed > INT_MAX) {
        normalized = sixel_decoder_threads_normalize(INT_MAX);
    } else {
        normalized = sixel_decoder_threads_normalize((int)parsed);
    }
    *value = normalized;
    return 1;
}

/*
 * Honor SIXEL_DECODE_PRESCAN=1 as a regression switch to keep using the
 * prescan-based parallel path even after the one-pass orchestrator is
 * introduced.
 */
static int
sixel_parallel_env_force_prescan(void)
{
    char const *text;
    char *endptr;
    long parsed;

    text = getenv("SIXEL_DECODE_PRESCAN");
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    if (text[0] == '0' && text[1] == '\0') {
        return 0;
    }
    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (endptr != text && *endptr == '\0' && errno == 0) {
        return parsed != 0;
    }
    return 1;
}

static void
sixel_parallel_plan_inputs_reset(sixel_parallel_plan_inputs_t *inputs)
{
    if (inputs == NULL) {
        return;
    }
    memset(inputs, 0, sizeof(*inputs));
    inputs->flags = 0;
}

static void
sixel_parallel_plan_inputs_destroy(sixel_parallel_plan_inputs_t *inputs,
                                   sixel_allocator_t *allocator)
{
    if (inputs == NULL || allocator == NULL) {
        return;
    }
    if (inputs->band_plan != NULL) {
        sixel_parallel_band_plan_destroy(inputs->band_plan, allocator);
        inputs->band_plan = NULL;
    }
    if (inputs->prescan != NULL) {
        sixel_prescan_destroy(inputs->prescan, allocator);
        inputs->prescan = NULL;
    }
    if (inputs->jobs != NULL) {
        sixel_allocator_free(allocator, inputs->jobs);
        inputs->jobs = NULL;
    }
    inputs->width = 0;
    inputs->height = 0;
    inputs->bgindex = 0;
    inputs->band_count = 0;
    inputs->job_count = 0;
}

static SIXELSTATUS sixel_parallel_plan_from_prescan(
    unsigned char *p,
    int len,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_metrics_t *metrics,
    sixel_parallel_plan_inputs_t *inputs)
{
    SIXELSTATUS status;
    sixel_prescan_t *prescan;
    int band_count;
    int width;
    int height;

    status = SIXEL_FALSE;
    prescan = NULL;
    (void)threads;
    if (inputs == NULL || allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_prescan_run(p, len, &prescan, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (metrics != NULL && metrics->enabled) {
        metrics->prescan_done_at = sixel_assessment_timer_now();
    }
    band_count = prescan->band_count;
    if (band_count < 1) {
        status = SIXEL_OK;
        goto end;
    }
    status = sixel_parallel_band_plan_from_prescan(prescan,
                                                   allocator,
                                                   &inputs->band_plan);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    width = prescan->width;
    height = prescan->height;
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    inputs->prescan = prescan;
    inputs->width = width;
    inputs->height = height;
    inputs->bgindex = prescan->band_states[0].bgindex;
    inputs->band_count = band_count;
    inputs->final_state = prescan->final_state;
    inputs->have_final_state = 1;
    inputs->flags = prescan->flags;
    prescan = NULL;
    status = SIXEL_OK;
end:
    if (prescan != NULL) {
        sixel_prescan_destroy(prescan, allocator);
    }
    return status;
}

/*
 * Used to mirror prescan results directly into the band plan via the
 * prescan callback hook, avoiding a second sweep over the prescan tables.
 */
typedef struct sixel_parallel_plan_build_context {
    sixel_parallel_band_plan_t *plan;
    sixel_allocator_t *allocator;
    sixel_parallel_coalescer_state_t coalescer;
    int checkpoint_interval;
    int checkpoint_stride;
    int checkpoint_index;
    int checkpoint_event_start;
    int mirrored_events;
    int event_count;
    int width;
    int height;
    int bgindex;
    int band_count;
    int threads;
    int coalescer_initialized;
    int have_geometry;
    int have_final_state;
    unsigned int flags;
    sixel_prescan_band_state_t final_state;
} sixel_parallel_plan_build_context_t;

static SIXELSTATUS
sixel_parallel_plan_on_prescan_band(sixel_prescan_t *prescan,
                                    int band_index,
                                    sixel_prescan_band_state_t const *band_state,
                                    size_t start_offset,
                                    size_t end_offset,
                                    int event_prefix,
                                    int weight_tokens,
                                    int weight_repeats,
                                    void *user_data)
{
    SIXELSTATUS status;
    sixel_parallel_plan_build_context_t *ctx;
    sixel_parallel_band_plan_t *plan;
    int checkpoint_event_start;
    int event_start;
    int event_count;
    sixel_parallel_band_meta_t *meta;

    status = SIXEL_BAD_ARGUMENT;
    ctx = (sixel_parallel_plan_build_context_t *)user_data;
    if (ctx == NULL || prescan == NULL || band_state == NULL) {
        goto end;
    }
    plan = ctx->plan;
    if (plan == NULL || ctx->allocator == NULL) {
        goto end;
    }
    /*
     * The prescan supplies the band start offset, end offset, token and
     * repeat weights, and the prefix event count at the start of this band.
     * Checkpoints capture the palette/pen/raster snapshot; the event span is
     * derived from the difference between the current band prefix and the
     * prefix stored with the most recent checkpoint.
     */
    if (band_index == 0) {
        status = sixel_parallel_band_plan_append_checkpoint(plan,
                                                            band_state,
                                                            band_index,
                                                            ctx->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        ctx->checkpoint_index = plan->checkpoint_count - 1;
        ctx->checkpoint_stride = 0;
        ctx->checkpoint_event_start = event_prefix;
    } else {
        ctx->checkpoint_stride += 1;
        if (ctx->checkpoint_stride >= ctx->checkpoint_interval) {
            ctx->checkpoint_stride = 0;
            ctx->checkpoint_index = plan->checkpoint_count;
            status = sixel_parallel_band_plan_append_checkpoint(plan,
                                                                band_state,
                                                                band_index,
                                                                ctx->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            ctx->checkpoint_event_start = event_prefix;
        }
    }
    checkpoint_event_start = ctx->checkpoint_event_start;
    event_start = event_prefix;
    if (checkpoint_event_start < 0 || event_start < checkpoint_event_start ||
            event_start > ctx->event_count) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "decoder-parallel: invalid prescan event span.");
        goto end;
    }
    event_count = event_start - checkpoint_event_start;
    status = sixel_parallel_band_plan_append_band(plan,
                                                  start_offset,
                                                  end_offset,
                                                  ctx->checkpoint_index,
                                                  checkpoint_event_start,
                                                  event_count,
                                                  weight_tokens,
                                                  weight_repeats,
                                                  ctx->allocator);
    if (SIXEL_SUCCEEDED(status)) {
        ctx->band_count = plan->band_count;
        meta = &plan->bands[ctx->band_count - 1];
        if (ctx->coalescer_initialized) {
            status = sixel_parallel_coalescer_push(&ctx->coalescer,
                                                   meta,
                                                   band_index);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    }
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_plan_on_prescan_event(sixel_prescan_t *prescan,
                                     int event_index,
                                     sixel_band_event_t const *event,
                                     void *user_data)
{
    SIXELSTATUS status;
    sixel_parallel_plan_build_context_t *ctx;

    status = SIXEL_BAD_ARGUMENT;
    ctx = (sixel_parallel_plan_build_context_t *)user_data;
    if (ctx == NULL || prescan == NULL || event == NULL) {
        goto end;
    }
    if (ctx->plan == NULL || ctx->allocator == NULL) {
        goto end;
    }
    ctx->mirrored_events = 1;
    if (ctx->event_count >= INT_MAX) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "decoder-parallel: event count overflow.");
        goto end;
    }
    ctx->event_count += 1;
    (void)event_index;
    status = sixel_parallel_band_plan_append_event(ctx->plan,
                                                   event,
                                                   ctx->allocator);
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_plan_on_prescan_finish(
    sixel_prescan_t *prescan,
    sixel_prescan_band_state_t const *final_state,
    int width,
    int height,
    int bgindex,
    int band_count,
    unsigned int flags,
    void *user_data)
{
    SIXELSTATUS status;
    sixel_parallel_plan_build_context_t *ctx;

    status = SIXEL_BAD_ARGUMENT;
    ctx = (sixel_parallel_plan_build_context_t *)user_data;
    if (ctx == NULL || prescan == NULL || final_state == NULL) {
        goto end;
    }
    ctx->width = width;
    ctx->height = height;
    ctx->bgindex = bgindex;
    ctx->band_count = band_count;
    ctx->flags = flags;
    ctx->final_state = *final_state;
    ctx->have_geometry = 1;
    ctx->have_final_state = 1;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS sixel_parallel_plan_onepass(
    unsigned char *p,
    int len,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_metrics_t *metrics,
    sixel_parallel_plan_inputs_t *inputs)
{
    SIXELSTATUS status;
    sixel_parallel_band_plan_t *plan;
    sixel_parallel_plan_build_context_t builder;
    sixel_prescan_callbacks_t callbacks;
    int band_count;
    int width;
    int height;
    int job_goal;
    int job_capacity;
    int target_weight;
    int outstanding_goal;

    status = SIXEL_FALSE;
    plan = NULL;
    memset(&builder, 0, sizeof(builder));
    memset(&callbacks, 0, sizeof(callbacks));
    if (inputs == NULL || allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_parallel_band_plan_create(
        SIXEL_PARALLEL_CHECKPOINT_INTERVAL + 1,
        SIXEL_PARALLEL_CHECKPOINT_INTERVAL * 4,
        SIXEL_PARALLEL_CHECKPOINT_INTERVAL * 2,
        SIXEL_PARALLEL_CHECKPOINT_INTERVAL,
        allocator,
        &plan);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    builder.plan = plan;
    builder.allocator = allocator;
    builder.threads = threads;
    builder.checkpoint_interval = SIXEL_PARALLEL_CHECKPOINT_INTERVAL;
    builder.checkpoint_stride = 0;
    builder.checkpoint_index = -1;
    /*
     * Build the coalesced job list while prescanning so the planner can
     * forward bands immediately without a secondary sweep. The initial
     * capacity favors a handful of jobs per checkpoint stride and can grow
     * dynamically when the prescan reports more bands than expected.
     */
    target_weight = sixel_parallel_coalescer_initial_target(len, threads);
    outstanding_goal = threads * 2;
    job_capacity = SIXEL_PARALLEL_CHECKPOINT_INTERVAL * 4;
    if (job_capacity < outstanding_goal) {
        job_capacity = outstanding_goal;
    }
    status = sixel_parallel_coalescer_init(&builder.coalescer,
                                           target_weight,
                                           outstanding_goal,
                                           NULL,
                                           job_capacity,
                                           allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    builder.coalescer_initialized = 1;
    /*
     * Mirror events into the band plan via callbacks and skip prescan-owned
     * storage for both the event log and post-band snapshots to avoid
     * duplicating metadata the planner already owns.
     */
    callbacks.on_band = sixel_parallel_plan_on_prescan_band;
    callbacks.on_event = sixel_parallel_plan_on_prescan_event;
    callbacks.on_finish = sixel_parallel_plan_on_prescan_finish;
    callbacks.discard_bands = 1;
    callbacks.discard_events = 1;
    callbacks.user_data = &builder;
    status = sixel_prescan_run(p, len, NULL, allocator, &callbacks);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (metrics != NULL && metrics->enabled) {
        metrics->prescan_done_at = sixel_assessment_timer_now();
    }
    if (builder.coalescer_initialized) {
        status = sixel_parallel_coalescer_flush(&builder.coalescer);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    band_count = builder.band_count;
    if (band_count < 1) {
        status = SIXEL_OK;
        goto end;
    }
    if (builder.event_count > 0) {
        if (!builder.mirrored_events ||
                plan->event_count != builder.event_count) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "decoder-parallel: mirrored event count mismatch.");
            goto end;
        }
    }
    /*
     * The one-pass planner relies on callbacks for geometry and the final
     * palette/pen state so it can discard the prescan tables entirely. If
     * either callback is missing, fail fast to avoid silently rebuilding
     * dimensions from prescan storage.
     */
    if (!builder.have_geometry || !builder.have_final_state) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "decoder-parallel: prescan callbacks did not supply geometry "
            "or final state.");
        goto end;
    }
    inputs->band_plan = plan;
    plan = NULL;
    inputs->band_count = band_count;
    /*
     * Ensure the job queue remains deep enough for the worker count even
     * when early coalescing produced a handful of large batches. If the
     * streaming sweep under-filled the queue, pad it by re-coalescing
     * with smaller targets so threadpool workers stay busy.
     */
    job_goal = builder.coalescer.outstanding_goal;
    if (job_goal < threads * 2) {
        job_goal = threads * 2;
    }
    if (builder.coalescer_initialized) {
        if (builder.coalescer.job_count > 0 &&
                builder.coalescer.job_count < job_goal) {
            status = sixel_parallel_coalescer_pad_jobs(&builder.coalescer,
                                                       inputs->band_plan,
                                                       job_goal,
                                                       allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        inputs->jobs = builder.coalescer.jobs;
        inputs->job_count = builder.coalescer.job_count;
        builder.coalescer.jobs = NULL;
        builder.coalescer.job_capacity = 0;
        builder.coalescer.job_count = 0;
        builder.coalescer.owns_jobs = 0;
    }
    inputs->bgindex = builder.bgindex;
    width = builder.width;
    height = builder.height;
    inputs->have_final_state = 1;
    inputs->final_state = builder.final_state;
    inputs->flags = builder.flags;
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    inputs->width = width;
    inputs->height = height;
    status = SIXEL_OK;
end:
    if (builder.coalescer_initialized) {
        sixel_parallel_coalescer_destroy(&builder.coalescer, allocator);
    }
    if (plan != NULL) {
        sixel_parallel_band_plan_destroy(plan, allocator);
    }
    return status;
}

static SIXELSTATUS sixel_parallel_decode_execute(
    sixel_parallel_plan_inputs_t *inputs,
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int threads,
    sixel_parallel_metrics_t *metrics,
    int *used_parallel)
{
    SIXELSTATUS status;
    sixel_parallel_decode_plan_t plan;
    int band_count;
    int bgindex;
    int width;
    int height;

    status = SIXEL_FALSE;
    memset(&plan, 0, sizeof(plan));
    if (inputs == NULL || image == NULL || allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    band_count = inputs->band_count;
    if (band_count < 1) {
        status = SIXEL_OK;
        goto end;
    }
    width = inputs->width;
    height = inputs->height;
    bgindex = inputs->bgindex;
    status = image_buffer_init(image,
                               width,
                               height,
                               bgindex,
                               depth,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (metrics != NULL && metrics->enabled) {
        metrics->prepare_done_at = sixel_assessment_timer_now();
    }
    plan.data = p;
    plan.len = len;
    plan.image = image;
    plan.allocator = allocator;
    plan.prescan = inputs->prescan;
    plan.band_plan = inputs->band_plan;
    plan.jobs = NULL;
    plan.metrics = metrics;
    plan.final_state = NULL;
    plan.depth = depth;
    plan.threads = threads;
    plan.direct_mode = (depth == 4);
    plan.width = width;
    plan.height = height;
    plan.band_color_max = NULL;
    plan.band_count = band_count;
    plan.job_count = 0;
    plan.jobs_owned = 0;
    if (inputs->jobs != NULL && inputs->job_count > 0) {
        plan.jobs = inputs->jobs;
        plan.job_count = inputs->job_count;
        inputs->jobs = NULL;
        inputs->job_count = 0;
        plan.jobs_owned = 1;
    } else {
        status = sixel_parallel_coalesce_jobs(plan.band_plan,
                                              plan.threads,
                                              allocator,
                                              &plan.jobs,
                                              &plan.job_count);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        plan.jobs_owned = 1;
    }
    plan.band_color_max = (int *)sixel_allocator_calloc(allocator,
                                                        (size_t)band_count,
                                                        sizeof(int));
    if (plan.band_color_max == NULL) {
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to allocate band metadata.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    /*
     * Prefer a final palette snapshot supplied by the plan builder so
     * one-pass builders can drop the prescan structure entirely. The
     * prescan fallback remains for backward compatibility while the
     * streaming parser is wired in.
     */
    if (inputs->have_final_state) {
        plan.final_state = &inputs->final_state;
    } else if (plan.prescan != NULL) {
        plan.final_state = &plan.prescan->final_state;
    }
    status = sixel_parallel_run_workers(&plan, metrics);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (!plan.direct_mode) {
        status = sixel_parallel_finalize_palette(image,
                                                 plan.band_color_max,
                                                 band_count);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (plan.final_state != NULL) {
            sixel_parallel_apply_final_palette(image, plan.final_state);
        }
    } else {
        image->ncolors = 0;
    }
    if (used_parallel != NULL) {
        *used_parallel = 1;
    }
    status = SIXEL_OK;
end:
    if (plan.jobs != NULL && plan.jobs_owned) {
        sixel_allocator_free(allocator, plan.jobs);
    }
    if (plan.band_color_max != NULL) {
        sixel_allocator_free(allocator, plan.band_color_max);
    }
    if (metrics != NULL && metrics->enabled) {
        if (metrics->wait_done_at == 0.0) {
            metrics->wait_done_at = sixel_assessment_timer_now();
        }
    }
    return status;
}

static void
sixel_decoder_threads_load_env(void)
{
    char const *text;
    int parsed;

    if (g_decoder_threads.env_checked) {
        return;
    }
    g_decoder_threads.env_checked = 1;
    g_decoder_threads.env_valid = 0;
    text = getenv("SIXEL_THREADS");
    if (text == NULL || text[0] == '\0') {
        return;
    }
    if (sixel_decoder_threads_parse_value(text, &parsed)) {
        g_decoder_threads.env_threads = parsed;
        g_decoder_threads.env_valid = 1;
    }
}

static void
sixel_parallel_metrics_init(sixel_parallel_metrics_t *metrics, int threads)
{
    char const *env;
    int rc;

    if (metrics == NULL) {
        return;
    }
    memset(metrics, 0, sizeof(*metrics));
    env = getenv("SIXEL_DEBUG_PARALLEL_TIMING");
    if (env != NULL && env[0] != '\0') {
        metrics->enabled = 1;
    }
    metrics->need_lock = (threads > 1);
#if SIXEL_ENABLE_THREADS
    if (metrics->enabled && metrics->need_lock) {
        rc = sixel_mutex_init(&metrics->lock);
        if (rc == SIXEL_OK) {
            metrics->lock_initialized = 1;
        } else {
            metrics->enabled = 0;
        }
    }
#else
    (void)rc;
#endif
    metrics->started_at = sixel_assessment_timer_now();
}

static void
sixel_parallel_metrics_destroy(sixel_parallel_metrics_t *metrics)
{
#if SIXEL_ENABLE_THREADS
    if (metrics == NULL) {
        return;
    }
    if (metrics->lock_initialized) {
        sixel_mutex_destroy(&metrics->lock);
        metrics->lock_initialized = 0;
    }
#else
    (void)metrics;
#endif
}

static void
sixel_parallel_metrics_record_decode(sixel_parallel_metrics_t *metrics,
                                     double elapsed_ms)
{
    /*
     * Accumulate per-band decode durations. A mutex protects concurrent
     * updates when the threadpool path is active.
     */
    if (metrics == NULL || !metrics->enabled) {
        return;
    }
    if (!metrics->need_lock) {
        metrics->decode_ms_sum += elapsed_ms;
        if (elapsed_ms > metrics->decode_ms_peak) {
            metrics->decode_ms_peak = elapsed_ms;
        }
        return;
    }
#if SIXEL_ENABLE_THREADS
    if (!metrics->lock_initialized) {
        return;
    }
    sixel_mutex_lock(&metrics->lock);
    metrics->decode_ms_sum += elapsed_ms;
    if (elapsed_ms > metrics->decode_ms_peak) {
        metrics->decode_ms_peak = elapsed_ms;
    }
    sixel_mutex_unlock(&metrics->lock);
#else
    (void)elapsed_ms;
#endif
}

static void
sixel_parallel_metrics_log(sixel_parallel_metrics_t *metrics)
{
    double now;
    double parse_ms;
    double prepare_ms;
    double dispatch_ms;
    double wait_ms;
    double total_ms;
    double prescan_done;
    double prepare_done;
    double dispatch_start;
    double dispatch_done;
    double wait_done;

    if (metrics == NULL || !metrics->enabled) {
        return;
    }
    now = metrics->wait_done_at;
    if (now == 0.0) {
        now = sixel_assessment_timer_now();
    }
    prescan_done = metrics->prescan_done_at;
    if (prescan_done < metrics->started_at) {
        prescan_done = metrics->started_at;
    }
    prepare_done = metrics->prepare_done_at;
    if (prepare_done < prescan_done) {
        prepare_done = prescan_done;
    }
    dispatch_start = metrics->dispatch_started_at;
    if (dispatch_start < prepare_done) {
        dispatch_start = prepare_done;
    }
    dispatch_done = metrics->dispatch_done_at;
    if (dispatch_done < dispatch_start) {
        dispatch_done = dispatch_start;
    }
    wait_done = metrics->wait_done_at;
    if (wait_done < dispatch_done) {
        wait_done = dispatch_done;
    }
    parse_ms = (prescan_done - metrics->started_at) * 1000.0;
    prepare_ms = (prepare_done - prescan_done) * 1000.0;
    dispatch_ms = (dispatch_done - dispatch_start) * 1000.0;
    wait_ms = (wait_done - dispatch_done) * 1000.0;
    total_ms = (now - metrics->started_at) * 1000.0;
    fprintf(stderr,
            "decoder-parallel timing: parse=%.3fms prepare=%.3fms "
            "dispatch=%.3fms decode_sum=%.3fms decode_peak=%.3fms "
            "wait=%.3fms total=%.3fms\n",
            parse_ms,
            prepare_ms,
            dispatch_ms,
            metrics->decode_ms_sum,
            metrics->decode_ms_peak,
            wait_ms,
            total_ms);
}

/*
 * Apply a single checkpoint delta event to the reconstructed band state.
 * The event stream is built by comparing each band with its nearest
 * checkpoint during prescan, so the worker can replay it to obtain the
 * correct starting parser state without holding the full per-band snapshot.
 */
static void
sixel_parallel_band_apply_event(sixel_band_event_t const *event,
                                sixel_prescan_band_state_t *state)
{
    if (event == NULL || state == NULL) {
        return;
    }
    switch (event->type) {
    case SIXEL_BAND_EVENT_PEN:
        state->color_index = event->color_index;
        break;
    case SIXEL_BAND_EVENT_BACKGROUND:
        state->bgindex = event->color_index;
        state->p2_background = event->pad;
        break;
    case SIXEL_BAND_EVENT_RASTER_ATTR:
        state->attributed_pad = event->pad;
        state->attributed_pan = event->pan;
        state->attributed_ph = event->ph;
        state->attributed_pv = event->pv;
        break;
    case SIXEL_BAND_EVENT_PALETTE:
        if (event->color_index >= 0 &&
                event->color_index < SIXEL_PRESCAN_PALETTE_MAX) {
            state->palette[event->color_index] = event->color;
        }
        break;
    default:
        break;
    }
}

/*
 * Restore the band start state from the nearest checkpoint plus the
 * recorded delta events. This keeps the worker reconstruction cost small
 * while still allowing the prescan to discard intermediate snapshots.
 */
static SIXELSTATUS sixel_parallel_band_plan_replay(
    sixel_parallel_band_plan_t *plan,
    int band_index,
    sixel_prescan_band_state_t *out_state)
{
    SIXELSTATUS status;
    sixel_parallel_checkpoint_t *checkpoint;
    sixel_parallel_band_meta_t *meta;
    int checkpoint_index;
    int event_index;
    int event_end;

    status = SIXEL_BAD_ARGUMENT;
    if (plan == NULL || out_state == NULL || band_index < 0 ||
            band_index >= plan->band_count) {
        goto end;
    }
    meta = &plan->bands[band_index];
    checkpoint_index = meta->checkpoint_index;
    if (checkpoint_index < 0 ||
            checkpoint_index >= plan->checkpoint_count) {
        sixel_helper_set_additional_message(
            "decoder-parallel: invalid checkpoint index.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    checkpoint = &plan->checkpoints[checkpoint_index];
    memcpy(out_state, &checkpoint->state, sizeof(*out_state));
    out_state->state = SIXEL_PRESCAN_PS_DECSIXEL;
    out_state->param = 0;
    out_state->nparams = 0;
    out_state->repeat_count = 1;
    out_state->pos_x = 0;
    out_state->pos_y = band_index * 6;
    event_end = meta->event_start + meta->event_count;
    if (meta->event_start < 0 || meta->event_count < 0 ||
            event_end < meta->event_start ||
            event_end > plan->event_count) {
        sixel_helper_set_additional_message(
            "decoder-parallel: invalid event span for band.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    for (event_index = meta->event_start; event_index < event_end;
            ++event_index) {
        sixel_parallel_band_apply_event(&plan->events[event_index],
                                        out_state);
    }
    status = SIXEL_OK;
end:
    return status;
}

static void
sixel_parallel_band_plan_destroy(sixel_parallel_band_plan_t *plan,
                                 sixel_allocator_t *allocator)
{
    if (plan == NULL) {
        return;
    }
    if (plan->checkpoints != NULL) {
        sixel_allocator_free(allocator, plan->checkpoints);
    }
    if (plan->events != NULL) {
        sixel_allocator_free(allocator, plan->events);
    }
    if (plan->bands != NULL) {
        sixel_allocator_free(allocator, plan->bands);
    }
    sixel_allocator_free(allocator, plan);
}

static SIXELSTATUS
sixel_parallel_band_plan_create(int checkpoint_capacity,
                                int event_capacity,
                                int band_capacity,
                                int checkpoint_interval,
                                sixel_allocator_t *allocator,
                                sixel_parallel_band_plan_t **out_plan)
{
    SIXELSTATUS status;
    sixel_parallel_band_plan_t *plan;

    status = SIXEL_BAD_ARGUMENT;
    plan = NULL;
    if (allocator == NULL || out_plan == NULL ||
            checkpoint_capacity < 0 || event_capacity < 0 ||
            band_capacity < 0 || checkpoint_interval < 1) {
        goto end;
    }
    plan = (sixel_parallel_band_plan_t *)sixel_allocator_calloc(
        allocator,
        1,
        sizeof(*plan));
    if (plan == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to allocate band plan.");
        goto end;
    }
    plan->checkpoints = (sixel_parallel_checkpoint_t *)
        sixel_allocator_calloc(allocator,
                               (size_t)checkpoint_capacity,
                               sizeof(*plan->checkpoints));
    plan->events = (sixel_band_event_t *)sixel_allocator_calloc(
        allocator,
        (size_t)event_capacity,
        sizeof(*plan->events));
    plan->bands = (sixel_parallel_band_meta_t *)sixel_allocator_calloc(
        allocator,
        (size_t)band_capacity,
        sizeof(*plan->bands));
    if ((checkpoint_capacity > 0 && plan->checkpoints == NULL) ||
            (event_capacity > 0 && plan->events == NULL) ||
            (band_capacity > 0 && plan->bands == NULL)) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to allocate band plan tables.");
        goto end;
    }
    plan->checkpoint_capacity = checkpoint_capacity;
    plan->event_capacity = event_capacity;
    plan->band_capacity = band_capacity;
    plan->checkpoint_interval = checkpoint_interval;
    plan->checkpoint_count = 0;
    plan->event_count = 0;
    plan->band_count = 0;
    *out_plan = plan;
    status = SIXEL_OK;
    goto end;
end:
    if (SIXEL_FAILED(status) && plan != NULL) {
        sixel_parallel_band_plan_destroy(plan, allocator);
    }
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_reserve_events(
    sixel_parallel_band_plan_t *plan,
    int required,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    size_t needed;
    size_t capacity;
    size_t new_capacity;
    sixel_band_event_t *grown;

    status = SIXEL_BAD_ARGUMENT;
    if (plan == NULL || allocator == NULL || required < 0) {
        goto end;
    }
    needed = (size_t)plan->event_count + (size_t)required;
    capacity = (size_t)plan->event_capacity;
    if (needed <= capacity) {
        status = SIXEL_OK;
        goto end;
    }
    if (plan->event_capacity == 0) {
        new_capacity = 32u;
    } else {
        new_capacity = capacity * 2u;
    }
    while (new_capacity < needed) {
        new_capacity *= 2u;
    }
    grown = (sixel_band_event_t *)sixel_allocator_realloc(
        allocator,
        plan->events,
        new_capacity * sizeof(*plan->events));
    if (grown == NULL) {
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to grow event table.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    plan->events = grown;
    plan->event_capacity = (int)new_capacity;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_append_event(
    sixel_parallel_band_plan_t *plan,
    sixel_band_event_t const *event,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_band_event_t *slot;

    status = SIXEL_BAD_ARGUMENT;
    if (plan == NULL || allocator == NULL || event == NULL) {
        goto end;
    }
    status = sixel_parallel_band_plan_reserve_events(plan, 1, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    slot = &plan->events[plan->event_count];
    *slot = *event;
    plan->event_count += 1;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_reserve_checkpoints(
    sixel_parallel_band_plan_t *plan,
    int required,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_parallel_checkpoint_t *resized;
    size_t new_capacity;

    status = SIXEL_BAD_ARGUMENT;
    resized = NULL;
    if (plan == NULL || allocator == NULL || required < 0) {
        goto end;
    }
    if (required <= plan->checkpoint_capacity) {
        status = SIXEL_OK;
        goto end;
    }
    new_capacity = plan->checkpoint_capacity > 0 ?
        (size_t)plan->checkpoint_capacity : 1u;
    while (new_capacity < (size_t)required) {
        if (new_capacity > SIZE_MAX / 2u) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "decoder-parallel: checkpoint capacity overflow.");
            goto end;
        }
        new_capacity *= 2u;
    }
    resized = (sixel_parallel_checkpoint_t *)sixel_allocator_realloc(
        allocator,
        plan->checkpoints,
        new_capacity * sizeof(*resized));
    if (resized == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to grow checkpoints.");
        goto end;
    }
    plan->checkpoints = resized;
    plan->checkpoint_capacity = (int)new_capacity;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_reserve_bands(sixel_parallel_band_plan_t *plan,
                                       int required,
                                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_parallel_band_meta_t *resized;
    size_t new_capacity;

    status = SIXEL_BAD_ARGUMENT;
    resized = NULL;
    if (plan == NULL || allocator == NULL || required < 0) {
        goto end;
    }
    if (required <= plan->band_capacity) {
        status = SIXEL_OK;
        goto end;
    }
    new_capacity = plan->band_capacity > 0 ?
        (size_t)plan->band_capacity : 1u;
    while (new_capacity < (size_t)required) {
        if (new_capacity > SIZE_MAX / 2u) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "decoder-parallel: band capacity overflow.");
            goto end;
        }
        new_capacity *= 2u;
    }
    resized = (sixel_parallel_band_meta_t *)sixel_allocator_realloc(
        allocator,
        plan->bands,
        new_capacity * sizeof(*resized));
    if (resized == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to grow band table.");
        goto end;
    }
    plan->bands = resized;
    plan->band_capacity = (int)new_capacity;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_append_checkpoint(
    sixel_parallel_band_plan_t *plan,
    sixel_prescan_band_state_t const *state,
    int band_index,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    int slot;

    status = SIXEL_BAD_ARGUMENT;
    if (plan == NULL || allocator == NULL || state == NULL ||
            band_index < 0) {
        goto end;
    }
    status = sixel_parallel_band_plan_reserve_checkpoints(plan,
                                                          plan->checkpoint_count
                                                          + 1,
                                                          allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    slot = plan->checkpoint_count;
    plan->checkpoints[slot].band_index = band_index;
    memcpy(&plan->checkpoints[slot].state,
           state,
           sizeof(plan->checkpoints[slot].state));
    plan->checkpoint_count += 1;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_adopt_events(sixel_parallel_band_plan_t *plan,
                                     sixel_prescan_t *prescan,
                                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ARGUMENT;
    if (plan == NULL || prescan == NULL || allocator == NULL) {
        goto end;
    }
    if (prescan->event_count <= 0 || prescan->events == NULL) {
        status = SIXEL_OK;
        goto end;
    }
    if (plan->event_count != 0) {
        sixel_helper_set_additional_message(
            "decoder-parallel: event buffer already populated.");
        goto end;
    }
    if (plan->events != NULL) {
        sixel_allocator_free(allocator, plan->events);
        plan->events = NULL;
        plan->event_capacity = 0;
    }
    plan->events = prescan->events;
    plan->event_capacity = prescan->event_capacity;
    plan->event_count = prescan->event_count;
    prescan->events = NULL;
    prescan->event_capacity = 0;
    prescan->event_count = 0;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_append_band(sixel_parallel_band_plan_t *plan,
                                     size_t start_offset,
                                     size_t end_offset,
                                     int checkpoint_index,
                                     int event_start,
                                     int event_count,
                                     int weight_tokens,
                                     int weight_repeats,
                                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_parallel_band_meta_t *meta;
    int slot;

    status = SIXEL_BAD_ARGUMENT;
    meta = NULL;
    if (plan == NULL || allocator == NULL || checkpoint_index < 0 ||
            event_start < 0 || event_count < 0) {
        goto end;
    }
    if (end_offset < start_offset) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_parallel_band_plan_reserve_bands(plan,
                                                    plan->band_count + 1,
                                                    allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    slot = plan->band_count;
    meta = &plan->bands[slot];
    meta->start_offset = start_offset;
    meta->end_offset = end_offset;
    meta->checkpoint_index = checkpoint_index;
    meta->event_start = event_start;
    meta->event_count = event_count;
    meta->weight_tokens = weight_tokens;
    meta->weight_repeats = weight_repeats;
    plan->band_count += 1;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_band_plan_from_prescan(sixel_prescan_t *prescan,
                                      sixel_allocator_t *allocator,
                                      sixel_parallel_band_plan_t **out_plan)
{
    SIXELSTATUS status;
    sixel_parallel_band_plan_t *plan;
    sixel_prescan_band_state_t *band_state;
    size_t checkpoint_capacity;
    int band_index;
    int event_start;
    int event_count;
    int checkpoint_interval;
    int checkpoint_index;
    int checkpoint_stride;
    int band_count;
    status = SIXEL_BAD_ARGUMENT;
    plan = NULL;
    if (prescan == NULL || allocator == NULL || out_plan == NULL) {
        goto end;
    }
    band_count = prescan->band_count;
    if (band_count <= 0) {
        goto end;
    }
    checkpoint_interval = SIXEL_PARALLEL_CHECKPOINT_INTERVAL;
    checkpoint_capacity = (size_t)band_count /
        (size_t)checkpoint_interval + 2u;
    status = sixel_parallel_band_plan_create((int)checkpoint_capacity,
                                             prescan->event_capacity,
                                             band_count,
                                             checkpoint_interval,
                                             allocator,
                                             &plan);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (prescan->event_count > 0 && prescan->events != NULL) {
        status = sixel_parallel_band_plan_adopt_events(plan,
                                                      prescan,
                                                      allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    band_state = &prescan->band_states[0];
    status = sixel_parallel_band_plan_append_checkpoint(plan,
                                                        band_state,
                                                        0,
                                                        allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    checkpoint_index = 0;
    checkpoint_stride = 0;
    for (band_index = 0; band_index < band_count; ++band_index) {
        size_t start_offset;
        size_t end_offset;
        int checkpoint_band;
        int source_start;
        int source_end;
        int weight_tokens;
        int weight_repeats;

        if (band_index != 0) {
            checkpoint_stride += 1;
            if (checkpoint_stride >= checkpoint_interval) {
                checkpoint_stride = 0;
                checkpoint_index = plan->checkpoint_count;
                band_state = &prescan->band_states[band_index];
                status = sixel_parallel_band_plan_append_checkpoint(
                    plan,
                    band_state,
                    band_index,
                    allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        }
        start_offset = prescan->band_start_offsets[band_index];
        end_offset = prescan->band_end_offsets[band_index];
        event_start = 0;
        event_count = 0;
        if (prescan->band_event_starts != NULL &&
                plan->event_count > 0) {
            checkpoint_band = plan->checkpoints[checkpoint_index].band_index;
            source_start = prescan->band_event_starts[checkpoint_band];
            source_end = prescan->band_event_starts[band_index];
            if (source_start < 0 || source_end < source_start ||
                    source_end > plan->event_count) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "decoder-parallel: invalid band event span.");
                goto end;
            }
            event_start = source_start;
            event_count = source_end - source_start;
        }
        if (prescan->band_token_counts != NULL) {
            weight_tokens = prescan->band_token_counts[band_index];
        } else {
            weight_tokens = (int)(end_offset - start_offset);
        }
        weight_repeats = prescan->band_repeat_sums[band_index];
        status = sixel_parallel_band_plan_append_band(plan,
                                                      start_offset,
                                                      end_offset,
                                                      checkpoint_index,
                                                      event_start,
                                                      event_count,
                                                      weight_tokens,
                                                      weight_repeats,
                                                      allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    *out_plan = plan;
    status = SIXEL_OK;
end:
    if (SIXEL_FAILED(status) && plan != NULL) {
        sixel_parallel_band_plan_destroy(plan, allocator);
    }
    return status;
}

static int
sixel_parallel_band_weight(sixel_parallel_band_meta_t *meta)
{
    int weight;

    /*
     * Combine token bytes and repeat hints so the coalescer can balance
     * work per job. The repeat contribution can be tuned with
     * SIXEL_PARALLEL_COALESCE_ALPHA.
     */
    weight = 1;
    if (meta != NULL) {
        weight = meta->weight_tokens +
            SIXEL_PARALLEL_COALESCE_ALPHA * meta->weight_repeats;
        if (weight < 1) {
            weight = 1;
        }
    }
    return weight;
}

static int
sixel_parallel_coalescer_initial_target(int len, int threads)
{
    int target;

    if (threads < 1) {
        threads = 1;
    }
    target = len / (threads * 6);
    if (target < SIXEL_PARALLEL_COALESCE_MIN_TARGET) {
        target = SIXEL_PARALLEL_COALESCE_MIN_TARGET;
    }
    return target;
}

static void
sixel_parallel_coalescer_reset_job(sixel_parallel_coalescer_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->job_start = 0;
    state->job_bands = 0;
    state->job_tokens = 0;
    state->job_repeats = 0;
    state->job_weight = 0;
}

static SIXELSTATUS
sixel_parallel_coalescer_grow_jobs(sixel_parallel_coalescer_state_t *state)
{
    SIXELSTATUS status;
    sixel_parallel_coalesced_job_t *resized;
    size_t new_capacity;

    status = SIXEL_BAD_ARGUMENT;
    if (state == NULL || state->allocator == NULL || state->job_capacity < 1) {
        goto end;
    }
    new_capacity = (size_t)state->job_capacity * 2u;
    if (new_capacity < (size_t)state->job_capacity + 8u) {
        new_capacity = (size_t)state->job_capacity + 8u;
    }
    if (new_capacity > (size_t)INT_MAX) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: job table growth overflow.");
        goto end;
    }
    resized = (sixel_parallel_coalesced_job_t *)sixel_allocator_realloc(
        state->allocator,
        state->jobs,
        new_capacity * sizeof(*state->jobs));
    if (resized == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to grow job table.");
        goto end;
    }
    state->jobs = resized;
    state->job_capacity = (int)new_capacity;
    state->owns_jobs = 1;
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS
sixel_parallel_coalescer_emit(sixel_parallel_coalescer_state_t *state)
{
    SIXELSTATUS status;
    sixel_parallel_coalesced_job_t *job;

    status = SIXEL_BAD_ARGUMENT;
    if (state == NULL) {
        goto end;
    }
    if (state->job_bands <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    if (state->job_count >= state->job_capacity) {
        status = sixel_parallel_coalescer_grow_jobs(state);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    job = &state->jobs[state->job_count];
    job->first_band = state->job_start;
    job->band_count = state->job_bands;
    job->weight_tokens = state->job_tokens;
    job->weight_repeats = state->job_repeats;
    state->job_count += 1;
    sixel_parallel_coalescer_reset_job(state);
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS sixel_parallel_coalescer_init(
    sixel_parallel_coalescer_state_t *state,
    int target_weight,
    int outstanding_goal,
    sixel_parallel_coalesced_job_t *jobs,
    int job_capacity,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ARGUMENT;
    if (state == NULL || allocator == NULL || target_weight < 1 ||
            job_capacity < 1) {
        goto end;
    }
    memset(state, 0, sizeof(*state));
    state->target_weight = target_weight;
    state->outstanding_goal = outstanding_goal;
    state->job_capacity = job_capacity;
    state->jobs = jobs;
    state->allocator = allocator;
    state->owns_jobs = 0;
    if (state->jobs == NULL) {
        state->jobs =
            (sixel_parallel_coalesced_job_t *)sixel_allocator_calloc(
                allocator,
                (size_t)job_capacity,
                sizeof(*state->jobs));
        if (state->jobs == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "decoder-parallel: failed to allocate job table.");
            goto end;
        }
        state->owns_jobs = 1;
    }
    sixel_parallel_coalescer_reset_job(state);
    status = SIXEL_OK;
end:
    return status;
}

static void sixel_parallel_coalescer_destroy(
    sixel_parallel_coalescer_state_t *state,
    sixel_allocator_t *allocator)
{
    if (state == NULL) {
        return;
    }
    if (allocator == NULL) {
        allocator = state->allocator;
    }
    if (state->jobs != NULL && state->owns_jobs && allocator != NULL) {
        sixel_allocator_free(allocator, state->jobs);
    }
    memset(state, 0, sizeof(*state));
}

static SIXELSTATUS sixel_parallel_coalescer_push(
    sixel_parallel_coalescer_state_t *state,
    sixel_parallel_band_meta_t *meta,
    int band_index)
{
    SIXELSTATUS status;
    int band_weight;
    int projected;

    status = SIXEL_BAD_ARGUMENT;
    if (state == NULL || meta == NULL) {
        goto end;
    }
    band_weight = sixel_parallel_band_weight(meta);
    if (state->job_bands == 0) {
        state->job_start = band_index;
        state->job_bands = 1;
        state->job_tokens = meta->weight_tokens;
        state->job_repeats = meta->weight_repeats;
        state->job_weight = band_weight;
        if (state->job_weight >= state->target_weight) {
            status = sixel_parallel_coalescer_emit(state);
        } else {
            status = SIXEL_OK;
        }
        goto end;
    }
    projected = state->job_weight + band_weight;
    if (projected > state->target_weight * 2) {
        status = sixel_parallel_coalescer_emit(state);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        state->job_start = band_index;
        state->job_bands = 1;
        state->job_tokens = meta->weight_tokens;
        state->job_repeats = meta->weight_repeats;
        state->job_weight = band_weight;
        if (state->job_weight >= state->target_weight) {
            status = sixel_parallel_coalescer_emit(state);
        } else {
            status = SIXEL_OK;
        }
        goto end;
    }
    state->job_bands += 1;
    state->job_tokens += meta->weight_tokens;
    state->job_repeats += meta->weight_repeats;
    state->job_weight = projected;
    if (state->job_weight >= state->target_weight) {
        status = sixel_parallel_coalescer_emit(state);
        goto end;
    }
    status = SIXEL_OK;
end:
    return status;
}

static SIXELSTATUS sixel_parallel_coalescer_flush(
    sixel_parallel_coalescer_state_t *state)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ARGUMENT;
    if (state == NULL) {
        goto end;
    }
    status = sixel_parallel_coalescer_emit(state);
end:
    return status;
}

static SIXELSTATUS sixel_parallel_coalescer_pad_jobs(
    sixel_parallel_coalescer_state_t *coalescer,
    sixel_parallel_band_plan_t const *plan,
    int job_goal,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_parallel_coalesced_job_t *expanded;
    int padded_jobs;
    int target;
    int capacity;

    status = SIXEL_BAD_ARGUMENT;
    expanded = NULL;
    padded_jobs = 0;
    target = 0;
    capacity = 0;
    if (coalescer == NULL || plan == NULL || allocator == NULL) {
        goto end;
    }
    if (job_goal < 1 || plan->band_count < 1) {
        status = SIXEL_OK;
        goto end;
    }
    if (coalescer->jobs == NULL || coalescer->job_count < 1) {
        status = SIXEL_OK;
        goto end;
    }
    if (coalescer->job_count >= job_goal) {
        status = SIXEL_OK;
        goto end;
    }
    target = coalescer->target_weight;
    if (target < 1) {
        target = SIXEL_PARALLEL_COALESCE_MIN_TARGET;
    }
    capacity = job_goal;
    if (capacity < coalescer->job_capacity) {
        capacity = coalescer->job_capacity;
    }
    if (capacity < plan->band_count) {
        capacity = plan->band_count;
    }
    expanded = (sixel_parallel_coalesced_job_t *)sixel_allocator_malloc(
        allocator,
        (size_t)capacity * sizeof(*expanded));
    if (expanded == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to grow job table for padding.");
        goto end;
    }
    /*
     * Re-coalesce bands with progressively smaller targets until the job
     * goal is satisfied. Halving the target weight nudges the greedy
     * packing toward finer batches without losing band order.
     */
    while (1) {
        status = sixel_parallel_coalesce_once(
            (sixel_parallel_band_plan_t *)plan,
            target,
            expanded,
            capacity,
            &padded_jobs,
            allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (padded_jobs >= job_goal || target <= 1) {
            break;
        }
        target /= 2;
        if (target < 1) {
            target = 1;
        }
    }
    if (padded_jobs < job_goal) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "decoder-parallel: coalescer failed to reach job target.");
        goto end;
    }
    if (coalescer->owns_jobs && coalescer->jobs != NULL) {
        sixel_allocator_free(allocator, coalescer->jobs);
    }
    coalescer->jobs = expanded;
    expanded = NULL;
    coalescer->job_capacity = capacity;
    coalescer->job_count = padded_jobs;
    coalescer->owns_jobs = 1;
    coalescer->target_weight = target;
    status = SIXEL_OK;
end:
    if (expanded != NULL) {
        sixel_allocator_free(allocator, expanded);
    }
    return status;
}

static SIXELSTATUS sixel_parallel_coalesce_once(
    sixel_parallel_band_plan_t *band_plan,
    int target_weight,
    sixel_parallel_coalesced_job_t *jobs,
    int job_capacity,
    int *out_job_count,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_parallel_coalescer_state_t coalescer;
    int band_index;

    /*
     * Greedy sweep that packs adjacent bands until the accumulated
     * weight reaches target_weight. A single job is capped at twice the
     * target to avoid overgrown batches while keeping ordering intact.
     */
    status = SIXEL_BAD_ARGUMENT;
    memset(&coalescer, 0, sizeof(coalescer));
    if (band_plan == NULL || jobs == NULL || out_job_count == NULL ||
            target_weight < 1 || job_capacity < band_plan->band_count ||
            allocator == NULL) {
        goto end;
    }
    status = sixel_parallel_coalescer_init(&coalescer,
                                           target_weight,
                                           0,
                                           jobs,
                                           job_capacity,
                                           allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (band_index = 0; band_index < band_plan->band_count; ++band_index) {
        sixel_parallel_band_meta_t *meta;

        meta = &band_plan->bands[band_index];
        status = sixel_parallel_coalescer_push(&coalescer,
                                               meta,
                                               band_index);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    status = sixel_parallel_coalescer_flush(&coalescer);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    *out_job_count = coalescer.job_count;
    status = SIXEL_OK;
end:
    sixel_parallel_coalescer_destroy(&coalescer, allocator);
    return status;
}

static SIXELSTATUS sixel_parallel_coalesce_jobs(
    sixel_parallel_band_plan_t *band_plan,
    int threads,
    sixel_allocator_t *allocator,
    sixel_parallel_coalesced_job_t **out_jobs,
    int *out_job_count)
{
    SIXELSTATUS status;
    sixel_parallel_coalesced_job_t *jobs;
    int job_capacity;
    int attempt;
    int band_index;
    int target_weight;
    int total_weight;
    int min_weight;
    int job_count;
    int factors[4];

    /*
     * Build a coalesced job list that keeps at least threads*2 jobs
     * outstanding. The target weight starts optimistic and is lowered
     * gradually to increase job count until the queue depth looks
     * healthy or we hit the minimum weight.
     */
    status = SIXEL_BAD_ARGUMENT;
    jobs = NULL;
    if (band_plan == NULL || allocator == NULL || out_jobs == NULL ||
            out_job_count == NULL || threads < 1) {
        goto end;
    }
    total_weight = 0;
    min_weight = INT_MAX;
    for (band_index = 0; band_index < band_plan->band_count; ++band_index) {
        int weight;

        weight = sixel_parallel_band_weight(&band_plan->bands[band_index]);
        total_weight += weight;
        if (weight < min_weight) {
            min_weight = weight;
        }
    }
    if (min_weight == INT_MAX) {
        min_weight = 1;
    }
    if (total_weight < SIXEL_PARALLEL_COALESCE_MIN_TARGET) {
        total_weight = SIXEL_PARALLEL_COALESCE_MIN_TARGET;
    }
    job_capacity = band_plan->band_count;
    jobs = (sixel_parallel_coalesced_job_t *)sixel_allocator_calloc(
        allocator,
        (size_t)job_capacity,
        sizeof(*jobs));
    if (jobs == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to allocate job table.");
        goto end;
    }
    factors[0] = 4;
    factors[1] = 6;
    factors[2] = 8;
    factors[3] = 12;
    job_count = 0;
    for (attempt = 0; attempt < 4; ++attempt) {
        target_weight = total_weight / (threads * factors[attempt]);
        if (target_weight < SIXEL_PARALLEL_COALESCE_MIN_TARGET) {
            target_weight = SIXEL_PARALLEL_COALESCE_MIN_TARGET;
        }
        if (target_weight < min_weight) {
            target_weight = min_weight;
        }
        status = sixel_parallel_coalesce_once(band_plan,
                                              target_weight,
                                              jobs,
                                              job_capacity,
                                              &job_count,
                                              allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (job_count >= threads * 2 || target_weight == min_weight) {
            break;
        }
    }
    if (job_count <= 0) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to create any jobs.");
        goto end;
    }
    status = SIXEL_OK;
    *out_jobs = jobs;
    *out_job_count = job_count;
    goto end;
end:
    if (SIXEL_FAILED(status) && jobs != NULL) {
        sixel_allocator_free(allocator, jobs);
    }
    return status;
}

SIXELSTATUS
sixel_decoder_parallel_override_threads(char const *text)
{
    SIXELSTATUS status;
    int parsed;

    status = SIXEL_BAD_ARGUMENT;
    if (text == NULL || text[0] == '\0') {
        sixel_helper_set_additional_message(
            "decoder: missing thread count after -=/--threads.");
        goto end;
    }
    if (!sixel_decoder_threads_parse_value(text, &parsed)) {
        sixel_helper_set_additional_message(
            "decoder: threads must be a positive integer or 'auto'.");
        goto end;
    }
    g_decoder_threads.override_active = 1;
    g_decoder_threads.override_threads = parsed;
    status = SIXEL_OK;
end:
    return status;
}

#if SIXEL_ENABLE_THREADS

static SIXELSTATUS sixel_parallel_safe_addition(
    sixel_prescan_band_state_t *state,
    unsigned char value)
{
    SIXELSTATUS status;
    int digit;

    status = SIXEL_FALSE;
    digit = (int)value - '0';
    if ((state->param > INT_MAX / 10) ||
            (digit > INT_MAX - state->param * 10)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        sixel_helper_set_additional_message(
            "decoder-parallel: integer overflow in parameter.");
        goto end;
    }
    state->param = state->param * 10 + digit;
    status = SIXEL_OK;
end:
    return status;
}

static uint32_t sixel_parallel_hls_to_rgba(int hue, int lum, int sat)
{
    double min;
    double max;
    int r;
    int g;
    int b;

    if (sat == 0) {
        r = g = b = lum;
    }
    max = lum + sat *
        (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0) :
                - (2 * (lum / 100.0) - 1.0))) / 2.0;
    min = lum - sat *
        (1.0 - (lum > 50 ? (2 * (lum / 100.0) - 1.0) :
                - (2 * (lum / 100.0) - 1.0))) / 2.0;
    hue = (hue + 240) % 360;
    switch (hue / 60) {
    case 0:
        r = max;
        g = (min + (max - min) * (hue / 60.0));
        b = min;
        break;
    case 1:
        r = min + (max - min) * ((120 - hue) / 60.0);
        g = max;
        b = min;
        break;
    case 2:
        r = min;
        g = max;
        b = (min + (max - min) * ((hue - 120) / 60.0));
        break;
    case 3:
        r = min;
        g = (min + (max - min) * ((240 - hue) / 60.0));
        b = max;
        break;
    case 4:
        r = (min + (max - min) * ((hue - 240) / 60.0));
        g = min;
        b = max;
        break;
    case 5:
        r = max;
        g = min;
        b = (min + (max - min) * ((360 - hue) / 60.0));
        break;
    default:
#if HAVE___BUILTIN_UNREACHABLE
        __builtin_unreachable();
#endif
        r = g = b = 0;
        break;
    }
    return SIXEL_PARALLEL_RGBA(r, g, b, 255);
}

static void sixel_parallel_store_indexed(image_buffer_t *image,
                                         int x,
                                         int y,
                                         int repeat,
                                         int color_index)
{
    size_t offset;

    if (repeat <= 0 || image == NULL) {
        return;
    }
    offset = (size_t)image->width * (size_t)y + (size_t)x;
    memset(image->pixels.in_bytes + offset,
           color_index,
           (size_t)repeat);
}

static void sixel_parallel_store_rgba(image_buffer_t *image,
                                      int x,
                                      int y,
                                      int repeat,
                                      uint32_t rgba,
                                      int use_non_temporal)
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char *dst;
    size_t offset;
    size_t byte_count;
    int allow_nt;
    uint32_t packed;

    if (repeat <= 0 || image == NULL || x < 0 || y < 0) {
        return;
    }
    offset = ((size_t)image->width * (size_t)y + (size_t)x) * 4u;
    dst = image->pixels.in_bytes + offset;
    r = (unsigned char)(rgba >> 24);
    g = (unsigned char)((rgba >> 16) & 0xffu);
    b = (unsigned char)((rgba >> 8) & 0xffu);
    a = (unsigned char)(rgba & 0xffu);
    packed = (uint32_t)r |
        ((uint32_t)g << 8) |
        ((uint32_t)b << 16) |
        ((uint32_t)a << 24);
    byte_count = (size_t)repeat * 4u;
    allow_nt = 0;
    if (use_non_temporal && byte_count >= 128u &&
            (((size_t)dst & 0x3u) == 0u)) {
        allow_nt = 1;
    }
    sixel_fillrun_store_rgba(dst, repeat, packed, allow_nt);
}

static int sixel_parallel_fill_span(sixel_parallel_decode_plan_t *plan,
                                    sixel_prescan_band_state_t *state,
                                    int y,
                                    int x,
                                    int repeat,
                                    int color_index)
{
    int clamped_index;
    int drawable;
    int use_non_temporal;
    uint32_t rgba;

    drawable = 0;
    if (plan == NULL || state == NULL) {
        return drawable;
    }
    if (repeat <= 0 || y < 0 || y >= plan->height) {
        return drawable;
    }
    if (x < 0 || x >= plan->width) {
        return drawable;
    }
    if (repeat > plan->width - x) {
        repeat = plan->width - x;
    }
    /*
     * When the DECGRA P2 flag requests a transparent background, skip
     * writes that only repaint the background color and advance the
     * cursor in the caller instead.
     */
    if (state->p2_background == 1 && color_index == state->bgindex) {
        return drawable;
    }
    clamped_index = color_index;
    if (clamped_index < 0) {
        clamped_index = 0;
    }
    if (clamped_index >= SIXEL_PRESCAN_PALETTE_MAX) {
        clamped_index = SIXEL_PRESCAN_PALETTE_MAX - 1;
    }
    use_non_temporal = repeat >= 32;
    if (plan->direct_mode) {
        rgba = state->palette[clamped_index];
        sixel_parallel_store_rgba(plan->image,
                                  x,
                                  y,
                                  repeat,
                                  rgba,
                                  use_non_temporal);
    } else {
        sixel_parallel_store_indexed(plan->image,
                                     x,
                                     y,
                                     repeat,
                                     clamped_index);
    }
    drawable = 1;
    return drawable;
}

static void sixel_parallel_track_color(int color_index,
                                       int *local_max_color)
{
    if (color_index > *local_max_color) {
        *local_max_color = color_index;
    }
}

static SIXELSTATUS sixel_parallel_decode_band(
    sixel_parallel_decode_plan_t *plan,
    int band_index)
{
    SIXELSTATUS status;
    sixel_parallel_band_plan_t *band_plan;
    sixel_parallel_band_meta_t *meta;
    sixel_prescan_band_state_t state;
    unsigned char *cursor;
    unsigned char *end;
    char detail[128];
    int bits;
    int mask;
    int i;
    int local_max_color;

    status = SIXEL_FALSE;
    band_plan = plan->band_plan;
    meta = NULL;
    if (band_plan != NULL) {
        meta = &band_plan->bands[band_index];
        status = sixel_parallel_band_plan_replay(band_plan,
                                                 band_index,
                                                 &state);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        state = plan->prescan->band_states[band_index];
    }
    if (meta != NULL) {
        if ((size_t)plan->len < meta->start_offset ||
                (size_t)plan->len < meta->end_offset ||
                meta->end_offset < meta->start_offset) {
            sixel_helper_set_additional_message(
                "decoder-parallel: invalid band offsets.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        cursor = plan->data + meta->start_offset;
        end = plan->data + meta->end_offset;
    } else if ((size_t)plan->len <
            plan->prescan->band_start_offsets[band_index] ||
            (size_t)plan->len < plan->prescan->band_end_offsets[band_index] ||
            plan->prescan->band_end_offsets[band_index] <
                plan->prescan->band_start_offsets[band_index]) {
        sixel_helper_set_additional_message(
            "decoder-parallel: invalid band boundaries detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    } else {
        cursor = plan->data + plan->prescan->band_start_offsets[band_index];
        end = plan->data + plan->prescan->band_end_offsets[band_index];
    }
    local_max_color = -1;

    while (cursor < end) {
        switch (state.state) {
        case SIXEL_PRESCAN_PS_GROUND:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case 0x90:
                state.state = SIXEL_PRESCAN_PS_DCS;
                break;
            case 0x9c:
                cursor = end;
                continue;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_ESC:
            switch (*cursor) {
            case '\\':
            case 0x9c:
                cursor = end;
                continue;
            case 'P':
                state.param = -1;
                state.state = SIXEL_PRESCAN_PS_DCS;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DCS:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (state.param < 0) {
                    state.param = 0;
                }
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state.param < 0) {
                    state.param = 0;
                }
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                break;
            case 'q':
                if (state.param >= 0 &&
                        state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                break;
            default:
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECSIXEL:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '"':
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECGRA;
                break;
            case '!':
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECGRI;
                break;
            case '#':
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECGCI;
                break;
            case '$':
                state.pos_x = 0;
                break;
            case '-':
                state.pos_x = 0;
                if (state.pos_y > INT_MAX - 6) {
                    snprintf(detail,
                             sizeof(detail),
                             "decoder-parallel: row advance overflow "
                             "(band=%d y=%d offset=%zu)",
                             band_index,
                             state.pos_y,
                             (size_t)(cursor - plan->data));
                    status = SIXEL_BAD_INPUT;
                    sixel_helper_set_additional_message(detail);
                    goto end;
                }
                state.pos_y += 6;
                break;
            default:
                if (*cursor >= '?' && *cursor <= '~') {
                    bits = *cursor - '?';
                    if (state.pos_x < 0 || state.pos_y < 0) {
                        snprintf(detail,
                                 sizeof(detail),
                                 "decoder-parallel: negative draw position "
                                 "(band=%d x=%d y=%d repeat=%d offset=%zu)",
                                 band_index,
                                 state.pos_x,
                                 state.pos_y,
                                 state.repeat_count,
                                 (size_t)(cursor - plan->data));
                        status = SIXEL_BAD_INPUT;
                        sixel_helper_set_additional_message(detail);
                        goto end;
                    }
                    if (bits == 0) {
                        if (state.repeat_count < 0 ||
                                state.pos_x > INT_MAX - state.repeat_count) {
                            snprintf(detail,
                                     sizeof(detail),
                                     "decoder-parallel: draw position "
                                     "overflow (band=%d x=%d repeat=%d "
                                     "offset=%zu)",
                                     band_index,
                                     state.pos_x,
                                     state.repeat_count,
                                     (size_t)(cursor - plan->data));
                            status = SIXEL_BAD_INPUT;
                            sixel_helper_set_additional_message(detail);
                            goto end;
                        }
                        state.pos_x += state.repeat_count;
                    } else {
                        mask = 0x01;
                        if (state.repeat_count <= 1) {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & mask) != 0) {
                                    if (sixel_parallel_fill_span(
                                            plan,
                                            &state,
                                            state.pos_y + i,
                                            state.pos_x,
                                            1,
                                            state.color_index)) {
                                        sixel_parallel_track_color(
                                            state.color_index,
                                            &local_max_color);
                                    }
                                }
                                mask <<= 1;
                            }
                            state.pos_x += 1;
                        } else {
                            for (i = 0; i < 6; ++i) {
                                if ((bits & mask) != 0) {
                                    int c;
                                    int run_span;
                                    int row;

                                    c = mask << 1;
                                    run_span = 1;
                                    while ((i + run_span) < 6 &&
                                            (bits & c) != 0) {
                                        c <<= 1;
                                        run_span += 1;
                                    }
                                    for (row = 0; row < run_span; ++row) {
                                        if (sixel_parallel_fill_span(
                                                plan,
                                                &state,
                                                state.pos_y + i + row,
                                                state.pos_x,
                                                state.repeat_count,
                                                state.color_index)) {
                                            sixel_parallel_track_color(
                                                state.color_index,
                                                &local_max_color);
                                        }
                                    }
                                    i += run_span - 1;
                                    mask <<= run_span - 1;
                                }
                                mask <<= 1;
                            }
                            if (state.repeat_count < 0 ||
                                    state.pos_x > INT_MAX -
                                    state.repeat_count) {
                                snprintf(detail,
                                         sizeof(detail),
                                         "decoder-parallel: draw position "
                                         "overflow (band=%d x=%d repeat=%d "
                                         "offset=%zu)",
                                         band_index,
                                         state.pos_x,
                                         state.repeat_count,
                                         (size_t)(cursor - plan->data));
                                status = SIXEL_BAD_INPUT;
                                sixel_helper_set_additional_message(detail);
                                goto end;
                            }
                            state.pos_x += state.repeat_count;
                        }
                    }
                    state.repeat_count = 1;
                }
                break;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGRA:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                break;
            default:
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                if (state.nparams > 0) {
                    state.attributed_pad = state.params[0];
                }
                if (state.nparams > 1) {
                    state.attributed_pan = state.params[1];
                }
                if (state.nparams > 2 && state.params[2] > 0) {
                    state.attributed_ph = state.params[2];
                }
                if (state.nparams > 3 && state.params[3] > 0) {
                    state.attributed_pv = state.params[3];
                }
                if (state.attributed_pan <= 0) {
                    state.attributed_pan = 1;
                }
                if (state.attributed_pad <= 0) {
                    state.attributed_pad = 1;
                }
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGRI:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                break;
            default:
                state.repeat_count = state.param;
                if (state.repeat_count == 0) {
                    state.repeat_count = 1;
                }
                if (state.repeat_count > SIXEL_PARALLEL_MAX_REPEAT) {
                    status = SIXEL_BAD_INPUT;
                    sixel_helper_set_additional_message(
                        "decoder-parallel: repeat parameter too large.");
                    goto end;
                }
                state.param = 0;
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
            }
            break;
        case SIXEL_PRESCAN_PS_DECGCI:
            switch (*cursor) {
            case 0x1b:
                state.state = SIXEL_PRESCAN_PS_ESC;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                status = sixel_parallel_safe_addition(&state, *cursor);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                break;
            case ';':
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                break;
            default:
                if (state.nparams < DECSIXEL_PARAMS_MAX) {
                    state.params[state.nparams++] = state.param;
                }
                state.param = 0;
                if (state.nparams > 0) {
                    state.color_index = state.params[0];
                    if (state.color_index < 0) {
                        state.color_index = 0;
                    } else if (state.color_index >=
                            SIXEL_PRESCAN_PALETTE_MAX) {
                        state.color_index = SIXEL_PRESCAN_PALETTE_MAX - 1;
                    }
                }
                if (state.nparams > 4) {
                    if (state.params[1] == 1) {
                        if (state.params[2] > 360) {
                            state.params[2] = 360;
                        }
                        if (state.params[3] > 100) {
                            state.params[3] = 100;
                        }
                        if (state.params[4] > 100) {
                            state.params[4] = 100;
                        }
                        state.palette[state.color_index] =
                            sixel_parallel_hls_to_rgba(state.params[2],
                                                       state.params[3],
                                                       state.params[4]);
                    } else if (state.params[1] == 2) {
                        if (state.params[2] > 100) {
                            state.params[2] = 100;
                        }
                        if (state.params[3] > 100) {
                            state.params[3] = 100;
                        }
                        if (state.params[4] > 100) {
                            state.params[4] = 100;
                        }
                        state.palette[state.color_index] =
                            SIXEL_PARALLEL_XRGB(state.params[2],
                                                state.params[3],
                                                state.params[4]);
                    }
                }
                state.nparams = 0;
                state.state = SIXEL_PRESCAN_PS_DECSIXEL;
                continue;
            }
            break;
        default:
            break;
        }
        cursor++;
    }
    plan->band_color_max[band_index] = local_max_color;
    status = SIXEL_OK;
end:
    return status;
}

static int sixel_parallel_worker(tp_job_t job,
                                 void *userdata,
                                 void *workspace)
{
    sixel_parallel_decode_plan_t *plan;
    sixel_parallel_coalesced_job_t *spec;
    SIXELSTATUS status;
    int band_offset;
    int band_index;
    double started;
    double elapsed_ms;

    (void)workspace;
    plan = (sixel_parallel_decode_plan_t *)userdata;
    if (job.band_index < 0 || job.band_index >= plan->job_count) {
        return -1;
    }
    spec = &plan->jobs[job.band_index];
    for (band_offset = 0; band_offset < spec->band_count; ++band_offset) {
        started = 0.0;
        elapsed_ms = 0.0;
        band_index = spec->first_band + band_offset;
        if (band_index < 0 || band_index >= plan->band_count) {
            sixel_helper_set_additional_message(
                "decoder-parallel: job band index out of range.");
            return -1;
        }
        if (plan->metrics != NULL && plan->metrics->enabled) {
            started = sixel_assessment_timer_now();
        }
        status = sixel_parallel_decode_band(plan, band_index);
        if (SIXEL_FAILED(status)) {
            return -1;
        }
        if (plan->metrics != NULL && plan->metrics->enabled) {
            elapsed_ms =
                (sixel_assessment_timer_now() - started) * 1000.0;
            sixel_parallel_metrics_record_decode(plan->metrics, elapsed_ms);
        }
    }
    return 0;
}

static SIXELSTATUS sixel_parallel_run_workers(
    sixel_parallel_decode_plan_t *plan,
    sixel_parallel_metrics_t *metrics)
{
    SIXELSTATUS status;
    threadpool_t *pool;
    int worker_threads;
    int job_index;
    double now;
    double started;
    double elapsed_ms;

    status = SIXEL_FALSE;
    pool = NULL;
    worker_threads = plan->threads;
    if (worker_threads > plan->job_count) {
        worker_threads = plan->job_count;
    }
    if (worker_threads < 1) {
        worker_threads = 1;
    }
    if (worker_threads == 1) {
        if (metrics != NULL && metrics->enabled) {
            now = sixel_assessment_timer_now();
            metrics->dispatch_started_at = now;
        }
        for (job_index = 0; job_index < plan->job_count; ++job_index) {
            sixel_parallel_coalesced_job_t *spec;
            int band_offset;

            spec = &plan->jobs[job_index];
            for (band_offset = 0; band_offset < spec->band_count;
                    ++band_offset) {
                int band_index;

                started = 0.0;
                elapsed_ms = 0.0;
                band_index = spec->first_band + band_offset;
                if (band_index < 0 || band_index >= plan->band_count) {
                    sixel_helper_set_additional_message(
                        "decoder-parallel: job band index out of range.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                if (metrics != NULL && metrics->enabled) {
                    started = sixel_assessment_timer_now();
                }
                status = sixel_parallel_decode_band(plan, band_index);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                if (metrics != NULL && metrics->enabled) {
                    elapsed_ms = (sixel_assessment_timer_now() - started) *
                        1000.0;
                    sixel_parallel_metrics_record_decode(metrics,
                                                        elapsed_ms);
                }
            }
        }
        if (metrics != NULL && metrics->enabled) {
            now = sixel_assessment_timer_now();
            metrics->dispatch_done_at = now;
            metrics->wait_done_at = now;
        }
        status = SIXEL_OK;
        goto end;
    }
    pool = threadpool_create(worker_threads,
                             plan->job_count,
                             0,
                             sixel_parallel_worker,
                             plan);
    if (pool == NULL) {
        sixel_helper_set_additional_message(
            "decoder-parallel: failed to create threadpool.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (metrics != NULL && metrics->enabled) {
        metrics->dispatch_started_at = sixel_assessment_timer_now();
    }
    for (job_index = 0; job_index < plan->job_count; ++job_index) {
        threadpool_push(pool, (tp_job_t){ job_index });
    }
    if (metrics != NULL && metrics->enabled) {
        metrics->dispatch_done_at = sixel_assessment_timer_now();
    }
    threadpool_finish(pool);
    if (metrics != NULL && metrics->enabled) {
        metrics->wait_done_at = sixel_assessment_timer_now();
    }
    if (threadpool_get_error(pool) != 0) {
        sixel_helper_set_additional_message(
            "decoder-parallel: worker reported an error.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    status = SIXEL_OK;
end:
    if (pool != NULL) {
        threadpool_destroy(pool);
    }
    return status;
}

static int sixel_parallel_should_attempt_inputs(
    sixel_parallel_plan_inputs_t *inputs,
    int len,
    int threads)
{
    if (inputs == NULL) {
        return 0;
    }
    if (threads < 2) {
        return 0;
    }
    if (len < SIXEL_PARALLEL_MIN_BYTES) {
        return 0;
    }
    if (inputs->flags != 0) {
        return 0;
    }
    if (inputs->band_count <= 0) {
        return 0;
    }
    return 1;
}

static SIXELSTATUS sixel_parallel_finalize_palette(image_buffer_t *image,
                                                   int *band_color_max,
                                                   int band_count)
{
    int max_color;
    int i;

    if (image == NULL || band_color_max == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    max_color = 0;
    for (i = 0; i < band_count; ++i) {
        if (band_color_max[i] > max_color) {
            max_color = band_color_max[i];
        }
    }
    if (max_color < 0) {
        max_color = 0;
    }
    image->ncolors = max_color;
    return SIXEL_OK;
}

static void sixel_parallel_apply_final_palette(
    image_buffer_t *image,
    sixel_prescan_band_state_t const *state)
{
    uint32_t rgba;
    int entries;
    int r;
    int g;
    int b;
    int i;

    if (image == NULL || state == NULL) {
        return;
    }
    entries = SIXEL_PRESCAN_PALETTE_MAX;
    if (entries > SIXEL_PALETTE_MAX_DECODER) {
        entries = SIXEL_PALETTE_MAX_DECODER;
    }
    for (i = 0; i < entries; ++i) {
        rgba = state->palette[i];
        r = (int)((rgba >> 24) & 0xff);
        g = (int)((rgba >> 16) & 0xff);
        b = (int)((rgba >> 8) & 0xff);
        image->palette[i] = (r << 16) | (g << 8) | b;
    }
}

int
sixel_decoder_parallel_resolve_threads(void)
{
    sixel_decoder_threads_load_env();
    if (g_decoder_threads.override_active) {
        return g_decoder_threads.override_threads;
    }
    if (g_decoder_threads.env_valid) {
        return g_decoder_threads.env_threads;
    }
    return sixel_decoder_threads_normalize(0);
}

static SIXELSTATUS sixel_parallel_decode_entry(
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int *used_parallel,
    sixel_parallel_plan_builder_t builder)
{
    SIXELSTATUS status;
    sixel_parallel_plan_inputs_t inputs;
    sixel_parallel_metrics_t metrics;
    int threads;

    status = SIXEL_FALSE;
    sixel_parallel_plan_inputs_reset(&inputs);
    memset(&metrics, 0, sizeof(metrics));
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    if (image == NULL || allocator == NULL || builder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    threads = sixel_decoder_parallel_resolve_threads();
    sixel_parallel_metrics_init(&metrics, threads);
    if (threads < 2 || len < SIXEL_PARALLEL_MIN_BYTES) {
        status = SIXEL_OK;
        goto end;
    }
    status = builder(p, len, threads, allocator, &metrics, &inputs);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (!sixel_parallel_should_attempt_inputs(&inputs, len, threads)) {
        status = SIXEL_OK;
        goto end;
    }
    status = sixel_parallel_decode_execute(&inputs,
                                           p,
                                           len,
                                           image,
                                           allocator,
                                           depth,
                                           threads,
                                           &metrics,
                                           used_parallel);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = SIXEL_OK;
end:
    if (metrics.enabled) {
        if (metrics.wait_done_at == 0.0) {
            metrics.wait_done_at = sixel_assessment_timer_now();
        }
        sixel_parallel_metrics_log(&metrics);
    }
    sixel_parallel_plan_inputs_destroy(&inputs, allocator);
    sixel_parallel_metrics_destroy(&metrics);
    return status;
}

static SIXELSTATUS sixel_parallel_decode_with_prescan(
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int *used_parallel)
{
    return sixel_parallel_decode_entry(p,
                                      len,
                                      image,
                                      allocator,
                                      depth,
                                      used_parallel,
                                      sixel_parallel_plan_from_prescan);
}

static SIXELSTATUS sixel_parallel_decode_onepass(
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int *used_parallel)
{
    return sixel_parallel_decode_entry(p,
                                      len,
                                      image,
                                      allocator,
                                      depth,
                                      used_parallel,
                                      sixel_parallel_plan_onepass);
}

static SIXELSTATUS sixel_parallel_decode_internal(
    unsigned char *p,
    int len,
    image_buffer_t *image,
    sixel_allocator_t *allocator,
    int depth,
    int *used_parallel)
{
    if (sixel_parallel_env_force_prescan()) {
        return sixel_parallel_decode_with_prescan(p,
                                                  len,
                                                  image,
                                                  allocator,
                                                  depth,
                                                  used_parallel);
    }
    return sixel_parallel_decode_onepass(p,
                                         len,
                                         image,
                                         allocator,
                                         depth,
                                         used_parallel);
}

#else /* !SIXEL_ENABLE_THREADS */

int sixel_decoder_parallel_resolve_threads(void)
{
    return 1;
}

#endif /* SIXEL_ENABLE_THREADS */

SIXELSTATUS sixel_decode_raw_parallel(unsigned char *p,
                                      int len,
                                      image_buffer_t *image,
                                      sixel_allocator_t *allocator,
                                      int *used_parallel)
{
#if SIXEL_ENABLE_THREADS
    return sixel_parallel_decode_internal(p,
                                          len,
                                          image,
                                          allocator,
                                          1,
                                          used_parallel);
#else
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    (void)p;
    (void)len;
    (void)image;
    (void)allocator;
    return SIXEL_OK;
#endif
}

SIXELSTATUS sixel_decode_direct_parallel(unsigned char *p,
                                         int len,
                                         image_buffer_t *image,
                                         sixel_allocator_t *allocator,
                                         int *used_parallel)
{
#if SIXEL_ENABLE_THREADS
    return sixel_parallel_decode_internal(p,
                                          len,
                                          image,
                                          allocator,
                                          4,
                                          used_parallel);
#else
    if (used_parallel != NULL) {
        *used_parallel = 0;
    }
    (void)p;
    (void)len;
    (void)image;
    (void)allocator;
    return SIXEL_OK;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
