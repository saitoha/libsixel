/* SPDX-License-Identifier: MIT */
/* Private playback controller. Components see only solve/resolve contracts;
 * this owner alone transports jobs, grants credits and orders completion. */
#include "playback-ready.h"

typedef struct sixel_playback_producer {
    sixel_encoder_frame_pipeline_t *pipeline;
    sixel_encoder_pipeline_slot_t slots[2];
    sixel_playback_ready_t ready;
    sixel_thread_t thread;
    int alpha_key;
    int origin_x, origin_y;
    int disabled;
    uint64_t emitting_sequence; /* Consumer-only credit notification context. */
} sixel_playback_producer_t;

static void
sixel_playback_resolved_credit(void *context)
{
    sixel_playback_producer_t *producer;
    struct sixel_encoder_solve_test_hooks const *hooks;

    producer = context;
    hooks = producer->pipeline->encoder->solve_test_hooks;
    if (hooks != NULL && hooks->resolved != NULL)
        hooks->resolved(hooks->context, producer->emitting_sequence);
    sixel_playback_ready_credit(&producer->ready);
}

typedef struct sixel_playback_solve_cancel {
    sixel_playback_producer_t *producer;
    uint64_t sequence;
} sixel_playback_solve_cancel_t;

static int
sixel_playback_solve_cancelled(void *context)
{
    sixel_playback_solve_cancel_t *cancel;
    sixel_encoder_frame_pipeline_t *pipeline;
    int stopped;

    cancel = context;
    pipeline = cancel->producer->pipeline;
    if (sixel_playback_ready_load(&cancel->producer->ready.stop)) return 1;
    sixel_mutex_lock(&pipeline->mutex);
    stopped = pipeline->worker_status != SIXEL_OK ||
        cancel->sequence <= pipeline->playback->cancelled_through;
    sixel_mutex_unlock(&pipeline->mutex);
    return stopped;
}

static int
sixel_playback_prepare_no_write(char *bytes, int size, void *context)
{
    (void)bytes;
    (void)size;
    (void)context;
    return -1; /* Preparation must never emit a transport byte. */
}

/* A private output adapter freezes wire options. Reading the live emitter's
 * output object while it is writing would race, even for read-only options. */
static SIXELSTATUS
sixel_playback_prepare_application(sixel_playback_producer_t *producer,
    sixel_encoder_pipeline_slot_t *slot)
{
    sixel_encode_dag_context_t *context;
    sixel_encoder_t *encoder;
    sixel_legacy_encode_request_t request;
    sixel_encode_session_t geometry;
    sixel_output_t *output;
    SIXELSTATUS status;

    context = &slot->job->context;
    encoder = producer->pipeline->encoder;
    output = NULL;
    if (context->latest || context->dither == NULL ||
            context->current_pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            context->current_colorspace != SIXEL_COLORSPACE_GAMMA ||
            encoder->output_color_definition != SIXEL_PALETTE_OUTPUT_LEGACY ||
            encoder->ormode || encoder->fuse_macro ||
            encoder->macro_number >= 0 || encoder->output_is_png)
        return SIXEL_FEATURE_ERROR;
    /* Absence of a GPU dither is a capability decision, not a method-name
     * switch. Legacy byte lookup/dither providers keep synchronous apply. */
    status = sixel_dither_prepare_indexed_components(context->dither, 0, 0);
    if (status != SIXEL_OK) return status;
    if (context->dither->gpu_dither == NULL) return SIXEL_FEATURE_ERROR;
    status = sixel_encoder_core_create_output_from_factory(&output,
        sixel_playback_prepare_no_write, NULL, encoder->allocator);
    if (status != SIXEL_OK) return status;
    sixel_output_set_palette_type(output, encoder->palette_type);
    status = sixel_encoder_set_output_transparent_policy(output,
        sixel_encoder_resolve_transparent_policy(encoder));
    if (status != SIXEL_OK) goto end;
    status = sixel_encoder_set_output_transparent_offset(output,
        encoder->transparent_offset_left, encoder->transparent_offset_top);
    if (status != SIXEL_OK) goto end;
    status = sixel_encoder_set_encoder_core_format(output,
        SIXEL_PIXELFORMAT_RGB888, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_GAMMA);
    if (status != SIXEL_OK) goto end;
    memset(&request, 0, sizeof(request));
    request.pixels = sixel_frame_get_pixels(context->frame);
    request.width = sixel_frame_get_width(context->frame);
    request.height = sixel_frame_get_height(context->frame);
    request.depth = 3;
    request.dither = context->dither;
    request.output = output;
    memset(&geometry, 0, sizeof(geometry));
    if (sixel_encoder_6delta_accumulation_requested(encoder)) {
        geometry.plane_width = encoder->sixdelta_plane_width;
        geometry.plane_height = encoder->sixdelta_plane_height;
        geometry.origin_x = producer->origin_x;
        geometry.origin_y = producer->origin_y;
        if (geometry.plane_width <= 0 || geometry.plane_height <= 0) {
            geometry.plane_width = request.width;
            geometry.plane_height = request.height;
            geometry.origin_x = geometry.origin_y = 0;
        }
        if (geometry.origin_x >= 0 && geometry.origin_y >= 0 &&
                geometry.origin_x <= geometry.plane_width - request.width &&
                geometry.origin_y <= geometry.plane_height - request.height)
            sixel_output_bind_session(output, &geometry);
    }
    status = sixel_output_prepare_deferred(&request, &context->deferred);
end:
    sixel_output_bind_session(output, NULL);
    sixel_output_unref(output);
    return status;
}

static void
sixel_playback_producer_publish(sixel_playback_producer_t *producer,
    sixel_encoder_pipeline_slot_t *slot)
{
    sixel_playback_ready_t *q;

    q = &producer->ready;
    if (!sixel_playback_ready_load(&q->stop) &&
            sixel_playback_ready_push(q, slot)) {
        sixel_playback_ready_wake(q);
        return;
    }
    sixel_mutex_lock(&q->gate);
    while (!sixel_playback_ready_load(&q->stop) &&
            !sixel_playback_ready_push(q, slot))
        sixel_cond_wait(&q->changed, &q->gate);
    sixel_cond_broadcast(&q->changed);
    sixel_mutex_unlock(&q->gate);
}

static int
sixel_playback_producer_main(void *context)
{
    sixel_playback_producer_t *producer;
    sixel_playback_ready_t *q;
    sixel_encoder_pipeline_slot_t *slot;
    sixel_thread_budget_scope_t policy;
    sixel_playback_solve_cancel_t cancel;
    double start;
    unsigned int index;
    int terminal;
    SIXELSTATUS status;

    producer = context;
    q = &producer->ready;
    index = 0;
    terminal = 0;
    sixel_threading_policy_enter(&policy, &producer->pipeline->policy);
    for (;;) {
        /* Recheck under the parking gate. Wakeups never substitute for the
         * predicate, and no GPU work or finalizer runs with this gate held. */
        if (!sixel_playback_ready_load(&q->credit) ||
                !sixel_playback_ready_load(&q->released[index])) {
            sixel_mutex_lock(&q->gate);
            while (!sixel_playback_ready_load(&q->stop) &&
                    (!sixel_playback_ready_load(&q->credit) ||
                     !sixel_playback_ready_load(&q->released[index])))
                sixel_cond_wait(&q->changed, &q->gate);
            sixel_mutex_unlock(&q->gate);
        }
        if (sixel_playback_ready_load(&q->stop)) break;
        (void)sixel_atomic_fetch_sub_u32(&q->credit, 1);
        (void)sixel_atomic_fetch_sub_u32(&q->released[index], 1);
        slot = &producer->slots[index];
        if (sixel_encoder_frame_pipeline_take(producer->pipeline, 1,
                &slot->frame, &slot->metadata) != 1) {
            terminal = 1;
            break;
        }
        start = sixel_timer_monotonic();
        sixel_encoder_pipeline_slot_bind(producer->pipeline, slot);
        status = slot->bind_status;
        cancel.producer = producer;
        cancel.sequence = slot->metadata.sequence;
        if (status == SIXEL_OK && sixel_playback_solve_cancelled(&cancel))
            status = SIXEL_INTERRUPTED;
        if (status == SIXEL_OK) {
            status = sixel_encoder_frame_clone(slot->work_frame,
                producer->pipeline->encoder->allocator, &slot->source);
        }
        if (status == SIXEL_OK) {
            if (producer->pipeline->encoder->prepare_test_hooks != NULL &&
                    producer->pipeline->encoder->prepare_test_hooks->begin) {
                producer->pipeline->encoder->prepare_test_hooks->begin(
                    producer->pipeline->encoder->prepare_test_hooks->context,
                    slot->metadata.sequence);
            }
            slot->job->delta_alpha_key = producer->alpha_key;
            status = sixel_encoder_pipeline_prepare_job(producer->pipeline,
                slot->job, slot->work_frame, &slot->metadata, NULL, 0);
        }
        if (producer->pipeline->encoder->prepare_test_hooks != NULL &&
                producer->pipeline->encoder->prepare_test_hooks->complete) {
            producer->pipeline->encoder->prepare_test_hooks->complete(
                producer->pipeline->encoder->prepare_test_hooks->context,
                slot->metadata.sequence, status);
        }
        if (slot->metadata.option_flags & SIXEL_PLAYBACK_OPTION_ORIGIN) {
            producer->origin_x = slot->metadata.origin_x;
            producer->origin_y = slot->metadata.origin_y;
        }
        if (status == SIXEL_OK && !producer->disabled) {
            status = sixel_playback_prepare_application(producer, slot);
            if (status == SIXEL_FEATURE_ERROR || status == SIXEL_FALSE ||
                    status == SIXEL_BAD_ALLOCATION) {
                /* Resource failure latches synchronous admission. A missing
                 * capability can change with the next frame's geometry. */
                producer->disabled = status == SIXEL_BAD_ALLOCATION;
                status = SIXEL_OK;
            }
        }
        slot->metadata.stages.prepare_seconds =
            sixel_timer_monotonic() - start;
        slot->metadata.stages.valid = SIXEL_PLAYBACK_STAGE_PREPARE;
        if (status == SIXEL_OK && slot->job->context.deferred.capability) {
            if (producer->pipeline->encoder->solve_test_hooks != NULL &&
                    producer->pipeline->encoder->solve_test_hooks->started)
                producer->pipeline->encoder->solve_test_hooks->started(
                    producer->pipeline->encoder->solve_test_hooks->context,
                    slot->metadata.sequence);
            (void)sixel_timeline_logger_prepare_env(
                producer->pipeline->encoder->allocator,
                &slot->job->context.job_logger);
            sixel_timeline_logger_set_frame_context(
                slot->job->context.job_logger, slot->metadata.frame_no,
                slot->metadata.loop_no, slot->metadata.multiframe);
            status = sixel_output_solve_deferred(&slot->job->context.deferred,
                slot->job->context.job_logger, sixel_playback_solve_cancelled,
                &cancel);
            if (status == SIXEL_OK &&
                    producer->pipeline->encoder->solve_test_hooks != NULL &&
                    producer->pipeline->encoder->solve_test_hooks->completed) {
                producer->pipeline->encoder->solve_test_hooks->completed(
                    producer->pipeline->encoder->solve_test_hooks->context,
                    slot->metadata.sequence);
            }
            slot->metadata.started_at =
                slot->job->context.deferred.solve_started;
            slot->job->context.deferred.stages.prepare_seconds =
                slot->metadata.stages.prepare_seconds;
            slot->job->context.deferred.stages.valid |=
                SIXEL_PLAYBACK_STAGE_PREPARE;
        }
        slot->status = status;
        slot->prepared = slot->job->active != 0;
        if (SIXEL_FAILED(status)) sixel_status_take_message(&slot->message);
        sixel_playback_producer_publish(producer, slot);
        if (status != SIXEL_OK && status != SIXEL_INTERRUPTED) break;
        index ^= 1;
    }
    if (terminal) sixel_playback_producer_publish(producer, NULL);
    sixel_thread_budget_leave(&policy);
    return 0;
}

/* Admission and cancellation remain in the existing pipeline. This bounded
 * handoff changes only where history-free GPU work runs. */
static int
sixel_encoder_frame_pipeline_run_deferred(
    sixel_encoder_frame_pipeline_t *pipeline)
{
    sixel_playback_producer_t producer;
    sixel_encoder_pipeline_slot_t *slot;
    sixel_playback_ready_t *q;
    sixel_encoder_t *encoder;
    sixel_indexed_frame_description_t const *description;
    SIXELSTATUS status;
    unsigned int i;
    int entered, resolved, publish;

    encoder = pipeline->encoder;
    if (pipeline->playback == NULL || pipeline->playback->capacity < 2 ||
            encoder->gpu_policy == SIXEL_GPU_POLICY_OFF ||
            pipeline->policy.options.palette != SIXEL_THREADING_PALETTE_FRAME)
        return 0;
    memset(&producer, 0, sizeof(producer));
    producer.pipeline = pipeline;
    producer.alpha_key = sixel_encoder_6delta_reserves_alpha_key(encoder);
    producer.origin_x = encoder->sixdelta_origin_x;
    producer.origin_y = encoder->sixdelta_origin_y;
    q = &producer.ready;
    for (i = 0; i < 2; ++i) {
        producer.slots[i].job = sixel_allocator_calloc(encoder->allocator,
            1, sizeof(*producer.slots[i].job));
        if (producer.slots[i].job == NULL) goto allocation_failed;
        q->released[i] = 1;
    }
    q->credit = 1;
    if (sixel_mutex_init(&q->gate) != 0) goto allocation_failed;
    if (sixel_cond_init(&q->changed) != 0) {
        sixel_mutex_destroy(&q->gate);
        goto allocation_failed;
    }
    status = sixel_thread_create(&producer.thread,
        sixel_playback_producer_main, &producer);
    if (status != SIXEL_OK) {
        sixel_cond_destroy(&q->changed);
        sixel_mutex_destroy(&q->gate);
        goto allocation_failed;
    }
    for (;;) {
        if (!sixel_playback_ready_pop(q, &slot)) {
            sixel_mutex_lock(&q->gate);
            while (!sixel_playback_ready_pop(q, &slot))
                sixel_cond_wait(&q->changed, &q->gate);
            sixel_mutex_unlock(&q->gate);
        }
        sixel_playback_ready_wake(q);
        if (slot == NULL) break;
        entered = 0;
        resolved = 0;
        status = slot->status;
        if (sixel_encoder_playback_frame_cancelled(pipeline, &slot->metadata))
            status = SIXEL_INTERRUPTED;
        if (status == SIXEL_OK) {
            status = sixel_encoder_operation_enter(encoder,
                SIXEL_ENCODER_OPERATION_PLAYBACK);
            entered = status == SIXEL_OK;
        }
        if (status == SIXEL_OK)
            status = sixel_encoder_playback_frame_begin(pipeline,
                &slot->metadata);
        if (status == SIXEL_OK &&
                (slot->job->delta_alpha_key !=
                    sixel_encoder_6delta_reserves_alpha_key(encoder) ||
                 slot->job->context.configuration_revision !=
                    encoder->configuration_revision)) {
            /* Repair from pristine input after discarding the opaque solve;
             * never remap solved indexes to a different palette. */
            (void)sixel_encoder_frame_job_release(slot->job, SIXEL_OK, 0);
            if (slot->work_is_clone) sixel_frame_unref(slot->work_frame);
            slot->work_frame = slot->source;
            slot->work_is_clone = 1;
            slot->source = NULL;
            slot->job->delta_alpha_key = -1;
            status = sixel_encoder_pipeline_prepare_job(pipeline, slot->job,
                slot->work_frame, &slot->metadata, NULL, 0);
        }
        if (status == SIXEL_OK) {
            description = &slot->job->context.deferred.description;
            slot->metadata.stage_revision =
                slot->job->context.configuration_revision;
            slot->metadata.stage_geometry[0] = description->width;
            slot->metadata.stage_geometry[1] = description->height;
            slot->metadata.stage_geometry[2] = description->plane_width;
            slot->metadata.stage_geometry[3] = description->plane_height;
            slot->metadata.stage_geometry[4] = description->origin_x;
            slot->metadata.stage_geometry[5] = description->origin_y;
            slot->job->context.deferred.resolved =
                sixel_playback_resolved_credit;
            slot->job->context.deferred.resolved_context = &producer;
            producer.emitting_sequence = slot->metadata.sequence;
            producer.alpha_key =
                sixel_encoder_6delta_accumulation_requested(encoder);
            if (slot->job->context.job_logger != NULL) {
                sixel_timeline_logger_set_frame_context(
                    slot->job->context.job_logger, slot->metadata.frame_no,
                    slot->metadata.loop_no, slot->metadata.multiframe);
                slot->job->context.dither->pipeline_logger =
                    slot->job->context.job_logger;
            }
            if (slot->job->context.deferred.solution == NULL) {
                sixel_playback_ready_credit(q);
                resolved = 1; /* Ordinary apply retains CPU prepare-ahead. */
            }
            status = sixel_encoder_frame_job_emit(slot->job);
            slot->metadata.stages = slot->job->context.deferred.stages;
            resolved |= (slot->metadata.stages.valid &
                SIXEL_PLAYBACK_STAGE_RESOLVE) != 0;
        }
        if (status != SIXEL_OK &&
                (status != SIXEL_INTERRUPTED || entered)) {
            if (slot->status != SIXEL_OK)
                sixel_status_restore_message(&slot->message);
            sixel_encoder_frame_pipeline_request_stop(pipeline, status,
                SIXEL_ENCODER_HANDOFF_TRACE_REASON_WORKER_ERROR);
        }
        if (slot->job->active) {
            publish = status == SIXEL_OK ||
                slot->job->context.deferred.output_started;
            status = sixel_encoder_frame_job_release(slot->job, status,
                entered && publish);
        }
        slot->metadata.palette_sequence = status == SIXEL_OK
            ? slot->metadata.sequence : 0;
        sixel_encoder_playback_frame_done(pipeline, &slot->metadata, status,
            entered);
        if (entered) sixel_encoder_operation_leave(encoder);
        i = slot == &producer.slots[0] ? 0 : 1;
        sixel_encoder_pipeline_slot_clear(slot);
        (void)sixel_atomic_fetch_add_u32(&q->released[i], 1);
        if (!resolved) {
            /* An unclaimed/cancelled frame never applied its origin. Reset
             * the prediction before the next admission inherits it. The
             * producer still lacks credit, so these writes cannot race. */
            producer.origin_x = encoder->sixdelta_origin_x;
            producer.origin_y = encoder->sixdelta_origin_y;
            producer.alpha_key =
                sixel_encoder_6delta_reserves_alpha_key(encoder);
            sixel_playback_ready_credit(q);
        } else {
            sixel_playback_ready_wake(q);
        }
        if (status != SIXEL_OK &&
                (status != SIXEL_INTERRUPTED || entered)) break;
    }
    sixel_playback_ready_stop(q);
    sixel_thread_join(&producer.thread);
    for (i = 0; i < 2; ++i) {
        sixel_encoder_pipeline_slot_discard(pipeline, &producer.slots[i],
            SIXEL_INTERRUPTED);
        sixel_allocator_free(encoder->allocator, producer.slots[i].job);
    }
    sixel_cond_destroy(&q->changed);
    sixel_mutex_destroy(&q->gate);
    return 1;
allocation_failed:
    for (i = 0; i < 2; ++i)
        sixel_allocator_free(encoder->allocator, producer.slots[i].job);
    return 0;
}
