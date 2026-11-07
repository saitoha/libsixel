/* SPDX-License-Identifier: MIT */
/* Playback owns execution and bounded storage. The algorithms receive only
 * their existing typed inputs: no component knows a queue or another lane. */
#define SIXEL_PLAYBACK_RING_CAPACITY 2

typedef struct sixel_playback_record {
    sixel_encoder_pipeline_slot_t slot;
    sixel_encoder_frame_job_t job;
    sixel_output_resolved_t image;
    unsigned char *bytes;
    size_t size;
    unsigned int packet_slot;
    double ready_at, resolved_at, encoded_at;
    int origin_x, origin_y;
    int claimed;
    int error_saved;
    sixel_atomic_u32_t released;
} sixel_playback_record_t;

typedef struct sixel_playback_ring {
    sixel_playback_record_t *entries[SIXEL_PLAYBACK_RING_CAPACITY];
    sixel_atomic_u32_t head, tail;
} sixel_playback_ring_t;

typedef struct sixel_playback_packet_storage {
    unsigned char *bytes;
    size_t allocated;
    int busy;
} sixel_playback_packet_storage_t;

typedef struct sixel_playback_independent {
    sixel_encoder_frame_pipeline_t *pipeline;
    sixel_playback_record_t *records;
    sixel_playback_ring_t queues[4];
    sixel_thread_t threads[4];
    sixel_mutex_t gate;
    sixel_cond_t changed;
    sixel_output_t *resolve_output, *packet_output;
    sixel_playback_record_t *packet;
    sixel_playback_packet_storage_t packets[SIXEL_PLAYBACK_RING_CAPACITY];
    /* S solutions, R images, E packets, including active users. */
    unsigned int used[3], high[3];
    size_t max_packet;
    int started;
    int abort_start;
    int fallback;
    uint64_t generation, revision, cancelled_through, encoded_generation;
    int geometry[6];
    uint64_t failure_sequence;
    double encoded_at;
} sixel_playback_independent_t;

/* SPSC payload publication is atomic; the mutex is only a parking gate.
 * Resource credits are independent: pop never permits overwriting a payload. */
static int
sixel_playback_ring_push(sixel_playback_ring_t *q,
    sixel_playback_record_t *record)
{
    unsigned int head, tail;

    tail = sixel_playback_ready_load(&q->tail);
    head = sixel_playback_ready_load(&q->head);
    if ((unsigned int)((uint64_t)tail + UINT_MAX + 1u - head)
            == SIXEL_PLAYBACK_RING_CAPACITY) return 0;
    q->entries[tail % SIXEL_PLAYBACK_RING_CAPACITY] = record;
    (void)sixel_atomic_fetch_add_u32(&q->tail, 1);
    return 1;
}

static int
sixel_playback_ring_pop(sixel_playback_ring_t *q,
    sixel_playback_record_t **record)
{
    unsigned int head, tail;

    head = sixel_playback_ready_load(&q->head);
    tail = sixel_playback_ready_load(&q->tail);
    if (head == tail) return 0;
    *record = q->entries[head % SIXEL_PLAYBACK_RING_CAPACITY];
    (void)sixel_atomic_fetch_add_u32(&q->head, 1);
    return 1;
}

static void
sixel_playback_independent_put(sixel_playback_independent_t *c, int queue,
    sixel_playback_record_t *record)
{
    int done;

    done = sixel_playback_ring_push(&c->queues[queue], record);
    sixel_mutex_lock(&c->gate);
    while (!done) {
        done = sixel_playback_ring_push(&c->queues[queue], record);
        if (!done) sixel_cond_wait(&c->changed, &c->gate);
    }
    sixel_cond_broadcast(&c->changed);
    sixel_mutex_unlock(&c->gate);
}

static sixel_playback_record_t *
sixel_playback_independent_take(sixel_playback_independent_t *c, int queue)
{
    sixel_playback_record_t *record;
    int done;

    done = sixel_playback_ring_pop(&c->queues[queue], &record);
    sixel_mutex_lock(&c->gate);
    while (!done) {
        done = sixel_playback_ring_pop(&c->queues[queue], &record);
        if (!done) sixel_cond_wait(&c->changed, &c->gate);
    }
    sixel_cond_broadcast(&c->changed);
    sixel_mutex_unlock(&c->gate);
    return record;
}

static void
sixel_playback_independent_credit(sixel_playback_independent_t *c, int pool,
    int acquire)
{
    sixel_mutex_lock(&c->gate);
    if (acquire) {
        while (c->used[pool] == SIXEL_PLAYBACK_RING_CAPACITY)
            sixel_cond_wait(&c->changed, &c->gate);
        ++c->used[pool];
        if (c->used[pool] > c->high[pool]) c->high[pool] = c->used[pool];
    } else {
        --c->used[pool];
        sixel_cond_broadcast(&c->changed);
    }
    sixel_mutex_unlock(&c->gate);
}

static int
sixel_playback_independent_start(sixel_playback_independent_t *c)
{
    int ready;

    sixel_mutex_lock(&c->gate);
    while (!c->started && !c->abort_start)
        sixel_cond_wait(&c->changed, &c->gate);
    ready = !c->abort_start;
    sixel_mutex_unlock(&c->gate);
    return ready;
}

static SIXELSTATUS
sixel_playback_independent_failure(sixel_playback_independent_t *c,
    uint64_t sequence)
{
    SIXELSTATUS status;
    uint64_t failed;

    sixel_mutex_lock(&c->gate);
    failed = c->failure_sequence;
    sixel_mutex_unlock(&c->gate);
    if (sequence != 0 && failed != 0 && sequence < failed) return SIXEL_OK;
    sixel_mutex_lock(&c->pipeline->mutex);
    status = c->pipeline->worker_status;
    sixel_mutex_unlock(&c->pipeline->mutex);
    if (status != SIXEL_OK && sequence != 0 && sequence > failed)
        return SIXEL_INTERRUPTED;
    return status;
}

static void
sixel_playback_independent_error(sixel_playback_independent_t *c,
    sixel_playback_record_t *r)
{
    if (r->slot.status == SIXEL_OK || r->error_saved) return;
    sixel_mutex_lock(&c->gate);
    if (r->slot.status == SIXEL_INTERRUPTED &&
            (!r->claimed || c->failure_sequence != 0)) {
        sixel_mutex_unlock(&c->gate);
        return;
    }
    if (c->failure_sequence == 0 ||
            r->slot.metadata.sequence < c->failure_sequence)
        c->failure_sequence = r->slot.metadata.sequence;
    sixel_mutex_unlock(&c->gate);
    r->error_saved = 1;
    sixel_status_take_message(&r->slot.message);
    sixel_encoder_frame_pipeline_request_stop(c->pipeline, r->slot.status,
        SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_ERROR);
}

static int
sixel_playback_independent_cancelled(void *context)
{
    sixel_playback_record_t *r;

    r = context;
    return sixel_encoder_playback_frame_cancelled(r->job.context.pipeline,
        &r->slot.metadata);
}

/* A profile transition belongs to the serial owner. Put the untouched
 * admission back before draining our prefix: its public capacity slot is
 * still reserved, so returning it cannot overflow the admission queue.
 * No later admission has been taken and no callback is duplicated. */
static void
sixel_playback_independent_return(sixel_playback_independent_t *c,
    sixel_playback_record_t *r)
{
    sixel_encoder_frame_pipeline_t *pipeline;

    pipeline = c->pipeline;
    sixel_mutex_lock(&pipeline->mutex);
    pipeline->queue_head = (pipeline->queue_head +
        SIXEL_ENCODER_FRAME_PIPELINE_SLOTS - 1) %
        SIXEL_ENCODER_FRAME_PIPELINE_SLOTS;
    pipeline->queue[pipeline->queue_head] = r->slot.frame;
    pipeline->queue_meta[pipeline->queue_head] = r->slot.metadata;
    ++pipeline->queue_count;
    r->slot.frame = NULL;
    sixel_cond_broadcast(&pipeline->cond);
    sixel_mutex_unlock(&pipeline->mutex);
    c->fallback = 1;
}

/* P owns preparation/palette admission. A cancellation fence drains the old
 * predictions before new admissions inherit an origin or keep-key state. */
static int
sixel_playback_independent_prepare(void *context)
{
    sixel_playback_independent_t *c;
    sixel_playback_record_t *r;
    sixel_encoder_t *encoder;
    sixel_playback_t *playback;
    sixel_thread_budget_scope_t policy, build_budget;
    uint64_t cancelled, previous;
    struct sixel_encoder_prepare_test_hooks const *hooks;
    unsigned int index;
    int alpha, x, y;

    c = context;
    if (!sixel_playback_independent_start(c)) return 0;
    encoder = c->pipeline->encoder;
    hooks = encoder->prepare_test_hooks;
    playback = c->pipeline->playback;
    alpha = sixel_encoder_6delta_reserves_alpha_key(encoder);
    x = encoder->sixdelta_origin_x;
    y = encoder->sixdelta_origin_y;
    previous = cancelled = 0;
    index = 0;
    sixel_threading_policy_enter(&policy, &c->pipeline->policy);
    for (;;) {
        r = &c->records[index];
        sixel_mutex_lock(&c->gate);
        while (!sixel_playback_ready_load(&r->released))
            sixel_cond_wait(&c->changed, &c->gate);
        sixel_mutex_unlock(&c->gate);
        if (sixel_encoder_frame_pipeline_take(c->pipeline, 1,
                &r->slot.frame, &r->slot.metadata) != 1) break;
        if (sixel_frame_get_pixelformat(r->slot.frame) !=
                SIXEL_PIXELFORMAT_RGB888 ||
                sixel_encoder_frame_working_colorspace(encoder,
                    r->slot.frame) != SIXEL_COLORSPACE_GAMMA) {
            sixel_playback_independent_return(c, r);
            break;
        }
        (void)sixel_atomic_fetch_sub_u32(&r->released, 1);
        sixel_mutex_lock(&c->pipeline->mutex);
        if (playback->cancelled_through != cancelled &&
                r->slot.metadata.sequence > playback->cancelled_through) {
            while (playback->completed < previous &&
                    c->pipeline->worker_status == SIXEL_OK)
                sixel_cond_wait(&c->pipeline->cond, &c->pipeline->mutex);
            cancelled = playback->cancelled_through;
            alpha = sixel_encoder_6delta_reserves_alpha_key(encoder);
            x = encoder->sixdelta_origin_x;
            y = encoder->sixdelta_origin_y;
        }
        sixel_mutex_unlock(&c->pipeline->mutex);
        previous = r->slot.metadata.sequence;
        /* A cancelled reuse must not expose the preceding admission's
         * palette selection or disposed provider pointers. */
        memset(&r->job.context, 0, sizeof(r->job.context));
        sixel_encoder_pipeline_slot_bind(c->pipeline, &r->slot);
        r->slot.status = r->slot.bind_status;
        r->claimed = 0;
        r->error_saved = 0;
        r->size = 0;
        if (r->slot.metadata.option_flags & SIXEL_PLAYBACK_OPTION_ORIGIN) {
            x = r->slot.metadata.origin_x;
            y = r->slot.metadata.origin_y;
        }
        r->origin_x = x;
        r->origin_y = y;
        r->job.delta_alpha_key = alpha;
        if (sixel_encoder_playback_frame_cancelled(c->pipeline,
                &r->slot.metadata)) r->slot.status = SIXEL_INTERRUPTED;
        if (r->slot.status == SIXEL_OK) {
            if (hooks != NULL && hooks->begin != NULL)
                hooks->begin(hooks->context, r->slot.metadata.sequence);
            r->slot.status = sixel_encoder_pipeline_prepare_job(c->pipeline,
                &r->job, r->slot.work_frame, &r->slot.metadata, NULL, 0);
            if (r->slot.status == SIXEL_OK && r->job.context.latest &&
                    !r->job.context.own_started) {
                /* P remains a producer when no background pool lease is
                 * available. S must not become the only way to finish a
                 * future palette while it is selecting an earlier frame. */
                r->job.context.own_sync = 1;
                sixel_thread_budget_enter(&build_budget,
                    c->pipeline->palette_scheduler.prepare_limit);
                sixel_palette_store_job_run(r->job.context.own_palette);
                sixel_thread_budget_leave(&build_budget);
            }
            alpha = sixel_encoder_6delta_accumulation_requested(encoder);
            if (hooks != NULL && hooks->complete != NULL)
                hooks->complete(hooks->context, r->slot.metadata.sequence,
                    r->slot.status);
        }
        /* Only R uses this output. P merely transfers its reference; E has
         * a different writer and the dispatcher owns the external callback. */
        if (r->job.active) {
            sixel_output_unref(r->job.context.output);
            r->job.context.output = c->resolve_output;
            r->job.context.independent = 1;
            sixel_output_ref(c->resolve_output);
        }
        r->slot.metadata.pipeline_sample.palette_seconds =
            r->job.context.own_palette_seconds;
        r->slot.metadata.pipeline_sample.valid =
            SIXEL_PLAYBACK_PIPELINE_INDEPENDENT;
        if (r->job.context.own_palette_seconds > 0)
            r->slot.metadata.pipeline_sample.valid |=
                SIXEL_PLAYBACK_PIPELINE_PALETTE;
        sixel_playback_independent_error(c, r);
        sixel_playback_independent_put(c, 0, r);
        index = (index + 1) % playback->capacity;
    }
    /* Stop publishing before the end marker enters any ring: it is not a
     * waiting frame. The same mutex pins this stack controller for readers. */
    sixel_mutex_lock(&c->pipeline->mutex);
    c->pipeline->queue_controller = NULL;
    sixel_mutex_unlock(&c->pipeline->mutex);
    sixel_playback_independent_put(c, 0, NULL);
    sixel_thread_budget_leave(&policy);
    return 0;
}

/* S pins one palette. A late own build remains owned by its admission until
 * the dispatcher joins it; no chosen palette changes under a solved image. */
static SIXELSTATUS
sixel_playback_independent_select(sixel_encode_dag_context_t *context)
{
    SIXELSTATUS status;

    if (!context->latest) {
        context->palette_sequence = context->sequence;
        return SIXEL_OK;
    }
    status = sixel_encoder_latest_select(context);
    if (status != SIXEL_OK) return status;
    status = sixel_palette_snapshot_clone(context->selected_palette.snapshot,
        &context->dither);
    if (status != SIXEL_OK) return status;
    return sixel_encode_dag_node_palette_bind(context);
}

static int
sixel_playback_independent_solve(void *context)
{
    sixel_playback_independent_t *c;
    sixel_playback_record_t *r;
    sixel_playback_producer_t preparation;
    sixel_playback_pipeline_sample_t *sample;
    sixel_thread_budget_scope_t policy;
    double start, selected;
    struct sixel_encoder_solve_test_hooks const *hooks;

    c = context;
    if (!sixel_playback_independent_start(c)) return 0;
    memset(&preparation, 0, sizeof(preparation));
    preparation.pipeline = c->pipeline;
    hooks = c->pipeline->encoder->solve_test_hooks;
    sixel_threading_policy_enter(&policy, &c->pipeline->policy);
    while ((r = sixel_playback_independent_take(c, 0)) != NULL) {
        sixel_playback_independent_credit(c, 0, 1);
        sample = &r->slot.metadata.pipeline_sample;
        if (sixel_encoder_playback_frame_cancelled(c->pipeline,
                &r->slot.metadata)) r->slot.status = SIXEL_INTERRUPTED;
        r->slot.metadata.started_at = sixel_timer_now();
        start = sixel_timer_monotonic();
        if (r->slot.status == SIXEL_OK)
            r->slot.status = sixel_playback_independent_select(&r->job.context);
        selected = sixel_timer_monotonic();
        sample->palette_wait_seconds = selected - start;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_PALETTE_WAIT;
        preparation.origin_x = r->origin_x;
        preparation.origin_y = r->origin_y;
        if (r->slot.status == SIXEL_OK) {
            r->slot.status = sixel_playback_prepare_application(&preparation,
                &r->slot);
            /* Ordinary indexed providers have no deferred capability. R
             * still captures a complete image without external output. */
            if (r->slot.status == SIXEL_FEATURE_ERROR ||
                    r->slot.status == SIXEL_FALSE) r->slot.status = SIXEL_OK;
        }
        sample->gpu_prepare_seconds = sixel_timer_monotonic() - selected;
        if (r->slot.status == SIXEL_OK &&
                r->job.context.deferred.capability != NULL) {
            if (hooks != NULL && hooks->started != NULL)
                hooks->started(hooks->context, r->slot.metadata.sequence);
            r->slot.status = sixel_output_solve_deferred(
                &r->job.context.deferred, r->job.context.job_logger,
                sixel_playback_independent_cancelled, r);
            if (r->slot.status == SIXEL_OK && hooks != NULL &&
                    hooks->completed != NULL)
                hooks->completed(hooks->context, r->slot.metadata.sequence);
        }
        r->ready_at = sixel_timer_monotonic();
        sample->solve_seconds = r->ready_at - selected;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_SOLVE;
        r->slot.metadata.palette_sequence = r->job.context.palette_sequence;
        sixel_playback_independent_error(c, r);
        sixel_playback_independent_put(c, 1, r);
    }
    sixel_playback_independent_put(c, 1, NULL);
    sixel_thread_budget_leave(&policy);
    return 0;
}

/* R reserves a result slot before claiming the frame or touching history.
 * Its complete image owns wire colors/indexes/skip independently of S. */
static int
sixel_playback_independent_resolve(void *context)
{
    sixel_playback_independent_t *c;
    sixel_playback_record_t *r;
    sixel_encoder_t *encoder;
    sixel_playback_pipeline_sample_t *sample;
    sixel_thread_budget_scope_t policy;
    SIXELSTATUS failure;
    double start;
    int entered;
    uint64_t cancelled;
    struct sixel_encoder_solve_test_hooks const *hooks;

    c = context;
    if (!sixel_playback_independent_start(c)) return 0;
    encoder = c->pipeline->encoder;
    hooks = encoder->solve_test_hooks;
    sixel_threading_policy_enter(&policy, &c->pipeline->policy);
    while ((r = sixel_playback_independent_take(c, 1)) != NULL) {
        sixel_playback_independent_credit(c, 1, 1);
        sample = &r->slot.metadata.pipeline_sample;
        start = sixel_timer_monotonic();
        sample->ready_wait_seconds = start - r->ready_at;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_READY_WAIT;
        entered = 0;
        failure = sixel_playback_independent_failure(c,
            r->slot.metadata.sequence);
        if (failure != SIXEL_OK) r->slot.status = failure;
        if (sixel_encoder_playback_frame_cancelled(c->pipeline,
                &r->slot.metadata)) r->slot.status = SIXEL_INTERRUPTED;
        if (r->slot.status == SIXEL_OK) {
            r->claimed = 1;
            if (hooks != NULL && hooks->resolving != NULL)
                hooks->resolving(hooks->context, r->slot.metadata.sequence);
            r->slot.status = sixel_encoder_operation_enter(encoder,
                SIXEL_ENCODER_OPERATION_PLAYBACK);
            entered = r->slot.status == SIXEL_OK;
        }
        if (r->slot.status == SIXEL_OK)
            r->slot.status = sixel_encoder_playback_frame_begin(c->pipeline,
                &r->slot.metadata);
        if (r->slot.status == SIXEL_OK) {
            encoder->sixdelta_refinement_needed = 0;
            encoder->sixdelta_painted_pixels = 0;
            r->job.context.deferred.capture = &r->image;
            r->slot.status = sixel_encode_dag_node_dither_plan(&r->job.context);
            if (r->slot.status == SIXEL_OK)
                r->slot.status = sixel_encode_dag_node_output(&r->job.context);
            if (r->slot.status == SIXEL_OK && r->image.image == NULL)
                r->slot.status = SIXEL_FEATURE_ERROR;
            if (r->slot.status == SIXEL_OK) {
                sixel_encoder_frame_job_publish(encoder);
                r->slot.metadata.painted_pixels =
                    encoder->sixdelta_painted_pixels;
                r->slot.metadata.refinement_needed =
                    encoder->sixdelta_refinement_needed;
            }
        }
        r->slot.metadata.stages = r->job.context.deferred.stages;
        if (r->job.active) {
            r->slot.metadata.stage_revision =
                r->job.context.configuration_revision;
            r->slot.metadata.stage_geometry[0] =
                sixel_frame_get_width(r->job.context.frame);
            r->slot.metadata.stage_geometry[1] =
                sixel_frame_get_height(r->job.context.frame);
            r->slot.metadata.stage_geometry[2] = encoder->sixdelta_plane_width;
            r->slot.metadata.stage_geometry[3] = encoder->sixdelta_plane_height;
            r->slot.metadata.stage_geometry[4] = encoder->sixdelta_origin_x;
            r->slot.metadata.stage_geometry[5] = encoder->sixdelta_origin_y;
        }
        sample->history_seconds = r->job.context.deferred.history_seconds;
        /* No provider state survives R. An E stall cannot pin S scratch. */
        sixel_output_dispose_deferred(&r->job.context.deferred);
        if (r->slot.status == SIXEL_OK && hooks != NULL &&
                hooks->resolved != NULL)
            hooks->resolved(hooks->context, r->slot.metadata.sequence);
        if (entered) sixel_encoder_operation_leave(encoder);
        sixel_playback_independent_credit(c, 0, 0);
        r->resolved_at = sixel_timer_monotonic();
        sample->resolve_seconds = r->resolved_at - start;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_RESOLVE;
        /* Only R assigns generations. E receives their immutable values;
         * no timing interval crosses cancellation or changed geometry. */
        sixel_mutex_lock(&c->pipeline->mutex);
        cancelled = c->pipeline->playback->cancelled_through;
        sixel_mutex_unlock(&c->pipeline->mutex);
        if (c->revision != r->slot.metadata.stage_revision ||
                c->cancelled_through != cancelled ||
                memcmp(c->geometry, r->slot.metadata.stage_geometry,
                    sizeof(c->geometry)) != 0) {
            ++c->generation;
            c->revision = r->slot.metadata.stage_revision;
            c->cancelled_through = cancelled;
            memcpy(c->geometry, r->slot.metadata.stage_geometry,
                sizeof(c->geometry));
        }
        sample->generation = c->generation;
        sixel_playback_independent_error(c, r);
        sixel_playback_independent_put(c, 2, r);
    }
    sixel_playback_independent_put(c, 2, NULL);
    sixel_thread_budget_leave(&policy);
    return 0;
}

/* Packet growth is owned by E. A callback failure leaves a private prefix;
 * it is discarded and cannot corrupt an external stream or R's successor. */
static int
sixel_playback_independent_write(char *bytes, int size, void *context)
{
    sixel_playback_independent_t *c;
    sixel_playback_record_t *r;
    unsigned char *grown;
    size_t required, capacity;
    sixel_playback_packet_storage_t *packet;

    c = context;
    r = c->packet;
    if (r == NULL || size < 0 || (size_t)size > SIZE_MAX - r->size) return -1;
    packet = &c->packets[r->packet_slot];
    required = r->size + (size_t)size;
    if (required > packet->allocated) {
        capacity = packet->allocated != 0 ? packet->allocated : 4096;
        while (capacity < required && capacity <= SIZE_MAX / 2) capacity *= 2;
        if (capacity < required) capacity = required;
        grown = sixel_allocator_realloc(c->pipeline->encoder->allocator,
            packet->bytes, capacity);
        if (grown == NULL) return -1;
        packet->bytes = grown;
        packet->allocated = capacity;
    }
    r->bytes = packet->bytes;
    memcpy(r->bytes + r->size, bytes, (size_t)size);
    r->size = required;
    if (required > c->max_packet) c->max_packet = required;
    return size;
}

static int
sixel_playback_independent_encode(void *context)
{
    sixel_playback_independent_t *c;
    sixel_playback_record_t *r;
    sixel_playback_pipeline_sample_t *sample;
    sixel_thread_budget_scope_t policy;
    sixel_threading_snapshot_t work;
    SIXELSTATUS failure;
    double start;
    struct sixel_encoder_solve_test_hooks const *hooks;

    c = context;
    if (!sixel_playback_independent_start(c)) return 0;
    hooks = c->pipeline->encoder->solve_test_hooks;
    work = c->pipeline->policy;
    if (c->pipeline->palette_store != NULL) {
        work.work = sixel_palette_scheduler_foreground;
        work.work_context = &c->pipeline->palette_scheduler;
        work.work_worker = 0;
    }
    sixel_threading_policy_enter(&policy, &work);
    while ((r = sixel_playback_independent_take(c, 2)) != NULL) {
        sixel_playback_independent_credit(c, 2, 1);
        sample = &r->slot.metadata.pipeline_sample;
        start = sixel_timer_monotonic();
        sample->resolved_wait_seconds = start - r->resolved_at;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_RESOLVED_WAIT;
        failure = sixel_playback_independent_failure(c,
            r->slot.metadata.sequence);
        if (failure != SIXEL_OK && r->slot.status == SIXEL_OK)
            r->slot.status = failure;
        sixel_mutex_lock(&c->gate);
        /* The acquired packet credit guarantees a free storage slot. */
        for (r->packet_slot = 0;
                r->packet_slot < SIXEL_PLAYBACK_RING_CAPACITY;
                ++r->packet_slot) {
            if (!c->packets[r->packet_slot].busy) break;
        }
        c->packets[r->packet_slot].busy = 1;
        sixel_mutex_unlock(&c->gate);
        c->packet = r;
        if (r->slot.status == SIXEL_OK) {
            if (hooks != NULL && hooks->encoding != NULL)
                hooks->encoding(hooks->context, r->slot.metadata.sequence);
            sixel_thread_work_begin();
            r->slot.status = sixel_output_encode_resolved(c->packet_output,
                &r->image);
            sixel_thread_work_end();
            if (r->slot.status == SIXEL_OK && hooks != NULL &&
                    hooks->encoded != NULL)
                hooks->encoded(hooks->context, r->slot.metadata.sequence);
        }
        c->packet = NULL;
        sixel_indexed_frame_unref(r->image.image);
        memset(&r->image, 0, sizeof(r->image));
        sixel_playback_independent_credit(c, 1, 0);
        r->encoded_at = sixel_timer_monotonic();
        sample->encode_seconds = r->encoded_at - start;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_ENCODE;
        if (r->slot.status == SIXEL_OK && c->encoded_at > 0 &&
                c->encoded_generation == sample->generation) {
            sample->encode_interval_seconds = r->encoded_at - c->encoded_at;
            sample->valid |= SIXEL_PLAYBACK_PIPELINE_ENCODE_INTERVAL;
        }
        c->encoded_at = r->slot.status == SIXEL_OK ? r->encoded_at : 0;
        c->encoded_generation = sample->generation;
        sixel_playback_independent_error(c, r);
        sixel_playback_independent_put(c, 3, r);
    }
    sixel_playback_independent_put(c, 3, NULL);
    sixel_thread_budget_leave(&policy);
    return 0;
}

static int
sixel_encoder_frame_pipeline_run_deferred(
    sixel_encoder_frame_pipeline_t *pipeline)
{
    sixel_playback_independent_t c;
    sixel_playback_record_t *r;
    sixel_playback_pipeline_sample_t *sample;
    sixel_encoder_t *encoder;
    SIXELSTATUS status, failure;
    sixel_writer_controls_t controls;
    unsigned int i, created;
    double start, delivered;
    size_t offset;
    int amount, finished;

    encoder = pipeline->encoder;
    if (pipeline->playback == NULL || pipeline->playback->capacity < 2 ||
            pipeline->output == NULL ||
            encoder->gpu_policy == SIXEL_GPU_POLICY_OFF ||
            encoder->prefer_float32 || encoder->gamut_mapper != NULL ||
            encoder->working_colorspace != SIXEL_COLORSPACE_GAMMA ||
            encoder->output_color_definition != SIXEL_PALETTE_OUTPUT_LEGACY ||
            encoder->ormode || encoder->fuse_macro ||
            encoder->macro_number >= 0 || encoder->output_is_png ||
            pipeline->policy.options.palette == SIXEL_THREADING_PALETTE_AUTO)
        return sixel_encoder_frame_pipeline_run_deferred_legacy(pipeline);
    memset(&c, 0, sizeof(c));
    c.pipeline = pipeline;
    sixel_palette_store_set_capacity(pipeline->palette_store,
        (int)pipeline->playback->capacity);
    created = 0;
    c.records = sixel_allocator_calloc(encoder->allocator,
        pipeline->playback->capacity, sizeof(*c.records));
    if (c.records == NULL) return 0;
    for (i = 0; i < pipeline->playback->capacity; ++i) {
        c.records[i].released = 1;
        c.records[i].slot.job = &c.records[i].job;
    }
    if (sixel_mutex_init(&c.gate) != 0) goto no_mutex;
    if (sixel_cond_init(&c.changed) != 0) goto no_cond;
    status = sixel_output_new(&c.resolve_output,
        sixel_playback_prepare_no_write, NULL, encoder->allocator);
    if (status != SIXEL_OK) goto cleanup;
    status = sixel_output_new(&c.packet_output,
        sixel_playback_independent_write, &c, encoder->allocator);
    if (status != SIXEL_OK) goto cleanup;
    status = sixel_output_get_writer_controls(pipeline->output, &controls);
    if (status != SIXEL_OK) goto cleanup;
    status = sixel_output_set_writer_controls(c.packet_output, &controls);
    if (status != SIXEL_OK) goto cleanup;
    sixel_output_set_8bit_availability(c.packet_output, encoder->f8bit);
    sixel_output_set_penetrate_multiplexer(c.packet_output,
        encoder->penetrate_multiplexer);
    status = sixel_thread_create(&c.threads[0],
        sixel_playback_independent_encode, &c);
    if (status != SIXEL_OK) goto cleanup;
    ++created;
    status = sixel_thread_create(&c.threads[1],
        sixel_playback_independent_resolve, &c);
    if (status != SIXEL_OK) goto cleanup;
    ++created;
    status = sixel_thread_create(&c.threads[2],
        sixel_playback_independent_solve, &c);
    if (status != SIXEL_OK) goto cleanup;
    ++created;
    status = sixel_thread_create(&c.threads[3],
        sixel_playback_independent_prepare, &c);
    if (status != SIXEL_OK) goto cleanup;
    ++created;
    /* No admitted work is taken until every owner exists. Startup failure
     * can still use the synchronous controller without abandoning a frame. */
    /* A new owner must not mix receipt-checked and resolved history.
     * All workers still wait at the startup barrier, so reset is exclusive. */
    (void)sixel_encoder_invalidate_6delta_plane_owned(encoder);
    pipeline->independent = 1;
    sixel_mutex_lock(&pipeline->mutex);
    pipeline->queue_controller = &c;
    sixel_mutex_unlock(&pipeline->mutex);
    sixel_mutex_lock(&c.gate);
    c.started = 1;
    sixel_cond_broadcast(&c.changed);
    sixel_mutex_unlock(&c.gate);
    while ((r = sixel_playback_independent_take(&c, 3)) != NULL) {
        sample = &r->slot.metadata.pipeline_sample;
        start = sixel_timer_monotonic();
        sample->encoded_wait_seconds = start - r->encoded_at;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_ENCODED_WAIT;
        failure = sixel_playback_independent_failure(&c,
            r->slot.metadata.sequence);
        if (failure != SIXEL_OK && r->slot.status == SIXEL_OK)
            r->slot.status = failure;
        /* A failure already known before delivery cannot leak packet bytes.
         * Pending construction is joined after delivery without blocking E. */
        if (r->slot.status == SIXEL_OK && r->job.context.own_palette != NULL) {
            status = sixel_palette_store_job_status(
                r->job.context.own_palette, &finished);
            if (finished && status != SIXEL_OK)
                r->slot.status = sixel_encoder_latest_join(&r->job.context,
                    SIXEL_OK);
        }
        offset = 0;
        while (r->slot.status == SIXEL_OK && offset < r->size) {
            amount = r->size - offset > INT_MAX ? INT_MAX
                : (int)(r->size - offset);
            r->slot.status = sixel_output_write_packet(pipeline->output,
                (char *)r->bytes + offset, amount);
            offset += (size_t)amount;
        }
        delivered = sixel_timer_monotonic();
        sample->delivery_seconds = delivered - start;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_DELIVERY;
        if (r->job.context.own_palette != NULL) {
            r->slot.status = sixel_encoder_latest_join(&r->job.context,
                r->slot.status);
            if (r->job.context.own_palette != NULL) {
                sample->palette_seconds = sixel_palette_store_job_seconds(
                    r->job.context.own_palette);
                if (sample->palette_seconds > 0)
                    sample->valid |= SIXEL_PLAYBACK_PIPELINE_PALETTE;
            }
        }
        if (r->job.active)
            r->slot.status = sixel_encoder_frame_job_release(&r->job,
                r->slot.status, 2);
        sample->completion_wait_seconds = sixel_timer_monotonic() - delivered;
        sample->valid |= SIXEL_PLAYBACK_PIPELINE_COMPLETION_WAIT;
        sixel_playback_independent_error(&c, r);
        if (r->error_saved) sixel_status_restore_message(&r->slot.message);
        sixel_encoder_playback_frame_done(pipeline, &r->slot.metadata,
            r->slot.status, r->claimed);
        sixel_encoder_pipeline_slot_clear(&r->slot);
        sixel_mutex_lock(&c.gate);
        c.packets[r->packet_slot].busy = 0;
        sixel_mutex_unlock(&c.gate);
        sixel_playback_independent_credit(&c, 2, 0);
        sixel_mutex_lock(&c.gate);
        (void)sixel_atomic_fetch_add_u32(&r->released, 1);
        sixel_cond_broadcast(&c.changed);
        sixel_mutex_unlock(&c.gate);
    }
cleanup:
    /* Unpublish before joins and destruction, including startup failures. */
    sixel_mutex_lock(&pipeline->mutex);
    pipeline->queue_controller = NULL;
    sixel_mutex_unlock(&pipeline->mutex);
    sixel_mutex_lock(&c.gate);
    if (!c.started) c.abort_start = 1;
    sixel_cond_broadcast(&c.changed);
    sixel_mutex_unlock(&c.gate);
    for (i = 0; i < created; ++i) sixel_thread_join(&c.threads[i]);
    /* Detach crosses back to receipt-checked direct output even on success.
     * No source, solution or image user survives the worker joins above. */
    if (c.started) (void)sixel_encoder_invalidate_6delta_plane_owned(encoder);
    pipeline->independent = 0;
    if (c.started) {
        sixel_trace_topic_message("playback_pipeline",
            "LSXPIPE1|capacity=%u|solutions=%u|images=%u|packets=%u|"
            "packet0_capacity=%llu|packet1_capacity=%llu|max_packet=%llu",
            pipeline->playback->capacity, c.high[0], c.high[1], c.high[2],
            (unsigned long long)c.packets[0].allocated,
            (unsigned long long)c.packets[1].allocated,
            (unsigned long long)c.max_packet);
    }
    sixel_output_unref(c.packet_output);
    sixel_output_unref(c.resolve_output);
    for (i = 0; i < SIXEL_PLAYBACK_RING_CAPACITY; ++i)
        sixel_allocator_free(encoder->allocator, c.packets[i].bytes);
    sixel_cond_destroy(&c.changed);
no_cond:
    sixel_mutex_destroy(&c.gate);
no_mutex:
    sixel_allocator_free(encoder->allocator, c.records);
    if (c.fallback && pipeline->palette_store != NULL)
        sixel_palette_store_reset(pipeline->palette_store, 0);
    return c.started && !c.fallback;
}
