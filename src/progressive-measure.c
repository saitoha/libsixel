/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <string.h>
#include "progressive.h"
#include "progressive-measure.h"
#include "encoder.h"
#include "timer.h"

struct sixel_repair_measure_state {
    sixel_repair_measure_record_t current;
    sixel_repair_measure_record_t latest;
    sixel_repair_measure_cost_t call[SIXEL_REPAIR_MEASURE_PHASES];
    size_t transitions[SIXEL_REPAIR_MEASURE_BANDS]
                      [SIXEL_REPAIR_MEASURE_BANDS];
    double gain;
    double excess_gain;
    size_t resolved;
    uint64_t sequence;
    uint64_t visits;
    uint64_t distances;
    double clock;
    int phase;
    int running;
    int timing_valid;
};

SIXELSTATUS
sixel_repair_measure_enable(sixel_progressive_t *context)
{
    SIXELSTATUS status;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_encoder_operation_enter(context->encoder,
        SIXEL_ENCODER_OPERATION_NORMAL);
    if (status != SIXEL_OK) {
        return status;
    }
    if (context->generation != 0) {
        status = SIXEL_BAD_ARGUMENT;
    } else if (context->measurement == NULL) {
        context->measurement = sixel_allocator_calloc(context->allocator,
            1, sizeof(*context->measurement));
        if (context->measurement == NULL) {
            status = SIXEL_BAD_ALLOCATION;
        }
    }
    sixel_encoder_operation_leave(context->encoder);
    return status;
}

SIXELSTATUS
sixel_repair_measure_read(sixel_progressive_t *context,
    sixel_repair_measure_record_t *record)
{
    SIXELSTATUS status;

    if (context == NULL || record == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_encoder_operation_enter(context->encoder,
        SIXEL_ENCODER_OPERATION_NORMAL);
    if (status != SIXEL_OK) {
        return status;
    }
    memset(record, 0, sizeof(*record));
    if (context->measurement == NULL ||
            context->measurement->latest.version == 0) {
        status = SIXEL_FALSE;
    } else {
        *record = context->measurement->latest;
    }
    sixel_encoder_operation_leave(context->encoder);
    return status;
}

void
sixel_repair_measure_rebind(sixel_progressive_t *context)
{
    uint64_t sequence;

    if (context->measurement != NULL) {
        sequence = context->measurement->sequence;
        memset(context->measurement, 0, sizeof(*context->measurement));
        context->measurement->sequence = sequence;
    }
}

void
sixel_repair_measure_begin(sixel_progressive_t *context,
    sixel_repair_work_t const *work)
{
    struct sixel_repair_measure_state *state;

    state = context->measurement;
    if (state == NULL) {
        return;
    }
    memset(state->call, 0, sizeof(state->call));
    state->phase = SIXEL_REPAIR_MEASURE_AUDIT;
    state->visits = work->pixel_visits;
    state->distances = work->distance_evaluations;
    state->clock = sixel_timer_now();
    state->timing_valid = isfinite(state->clock);
    state->running = 1;
}

static void
sixel_repair_measure_checkpoint(struct sixel_repair_measure_state *state,
    sixel_repair_work_t const *work)
{
    sixel_repair_measure_cost_t *cost;
    double now;

    cost = state->call + state->phase;
    now = sixel_timer_now();
    if (!isfinite(now) || !isfinite(state->clock) || now < state->clock) {
        state->timing_valid = 0;
    } else {
        cost->seconds += now - state->clock;
    }
    cost->visits += work->pixel_visits - state->visits;
    cost->distances += work->distance_evaluations - state->distances;
    state->visits = work->pixel_visits;
    state->distances = work->distance_evaluations;
    state->clock = now;
}

void
sixel_repair_measure_phase(sixel_progressive_t *context, int phase,
    sixel_repair_work_t const *work)
{
    struct sixel_repair_measure_state *state;

    state = context->measurement;
    if (state != NULL && state->running && state->phase != phase) {
        sixel_repair_measure_checkpoint(state, work);
        state->phase = phase;
    }
}

void
sixel_repair_measure_audit_begin(sixel_progressive_t *context)
{
    sixel_repair_measure_record_t *record;

    if (context->measurement == NULL) {
        return;
    }
    record = &context->measurement->current;
    memset(record, 0, sizeof(*record));
    record->stop_squared = context->stop_squared;
    record->sample_stride = (context->count - 1) /
        SIXEL_REPAIR_MEASURE_SAMPLES + 1;
    sixel_repair_predict_reset(&record->prediction_model,
        context->stop_squared);
    memcpy(record->band_upper, record->prediction_model.upper,
        sizeof(record->band_upper));
}

static int
sixel_repair_measure_band(sixel_repair_measure_record_t const *record,
    double error)
{
    int band;

    for (band = 0; band < SIXEL_REPAIR_MEASURE_BANDS - 1; ++band) {
        if (error <= record->band_upper[band]) {
            break;
        }
    }
    return band;
}

void
sixel_repair_measure_pixel(sixel_progressive_t *context, size_t position,
    float const *target, double error)
{
    sixel_repair_measure_record_t *record;
    sixel_repair_measure_sample_t *sample;
    int band;

    if (context->measurement == NULL) {
        return;
    }
    record = &context->measurement->current;
    sixel_repair_predict_add(&record->prediction_model, position, target,
        error);
    band = sixel_repair_measure_band(record, error);
    ++record->bands[band];
    record->band_sums[band] += error;
    record->before_excess += fmax(0, error - context->stop_squared);
    if (position % record->sample_stride == 0 &&
            record->sample_count < SIXEL_REPAIR_MEASURE_SAMPLES) {
        sample = record->samples + record->sample_count++;
        sample->position = position;
        memcpy(sample->target, target, sizeof(sample->target));
        sample->error = error;
    }
}

void
sixel_repair_measure_packet_begin(sixel_progressive_t *context)
{
    struct sixel_repair_measure_state *state;

    state = context->measurement;
    if (state == NULL) {
        return;
    }
    state->current.proposal_complete = 0;
    state->current.proposal_pixels = 0;
    state->current.proposal_gain = 0;
    state->current.proposal_bytes = 0;
    state->current.fitting_retries = 0;
    state->current.observer_distances = 0;
    memset(state->transitions, 0, sizeof(state->transitions));
    state->gain = state->excess_gain = 0;
    state->resolved = 0;
}

void
sixel_repair_measure_proposal(sixel_progressive_t *context, size_t position)
{
    sixel_repair_measure_record_t *record;

    if (context->measurement == NULL) {
        return;
    }
    record = &context->measurement->current;
    record->proposal_pixels += context->gains[position] > 0;
    record->proposal_gain += context->gains[position];
    record->proposal_complete = position + 1 == context->count;
}

void
sixel_repair_measure_serialized(sixel_progressive_t *context)
{
    sixel_repair_measure_record_t *record;

    if (context->measurement == NULL) {
        return;
    }
    record = &context->measurement->current;
    if (record->proposal_bytes == 0) {
        record->proposal_bytes = context->packet->bytes;
    }
    record->fitting_retries += context->packet->bytes > context->packet_limit
        && context->packet_kept > 1;
}

void
sixel_repair_measure_commit_pixel(sixel_progressive_t *context,
    size_t position)
{
    struct sixel_repair_measure_state *state;
    sixel_repair_application_t const *application;
    float const *color;
    double before;
    double after;
    int from;
    int to;

    state = context->measurement;
    if (state == NULL) {
        return;
    }
    application = &context->application;
    color = application->colors[
        application->palette[context->indices[position]]].working;
    before = context->errors[position];
    /* Recompute rather than subtracting a rounded gain near a band edge.
     * Observer work is separate from algorithmic budget counters, bounded
     * by one evaluation per existing record-scan visit, and opt-in only. */
    after = sixel_repair_distance2(application->source + position * 3, color);
    ++state->current.observer_distances;
    state->gain += before - after;
    state->excess_gain += fmax(0, before - context->stop_squared)
        - fmax(0, after - context->stop_squared);
    state->resolved += before > context->stop_squared &&
        after <= context->stop_squared;
    from = sixel_repair_measure_band(&state->current, before);
    to = sixel_repair_measure_band(&state->current, after);
    ++state->transitions[from][to];
}

void
sixel_repair_measure_finish(sixel_progressive_t *context,
    sixel_repair_work_t const *work, struct sixel_repair_result const *result,
    SIXELSTATUS status)
{
    struct sixel_repair_measure_state *state;
    sixel_repair_measure_record_t *record;
    unsigned int color;

    state = context->measurement;
    if (state == NULL) {
        return;
    }
    sixel_repair_measure_checkpoint(state, work);
    state->running = 0;
    record = &state->latest;
    *record = state->current;
    record->version = 3;
    record->sequence = ++state->sequence;
    record->generation = context->generation;
    record->configuration_revision = context->configuration_revision;
    record->before_revision = context->audit.history_revision;
    record->after_revision = context->encoder->history_revision;
    record->status = status;
    record->outcome = result->audit.outcome;
    record->audit_complete = context->audit.complete &&
        context->audit.history_revision == context->history_revision &&
        result->audit.outcome != SIXEL_REPAIR_NEEDS_SOURCE &&
        status == SIXEL_OK;
    record->timing_valid = state->timing_valid;
    record->workspace = context->workspace;
    record->full_selection = context->full_selection;
    record->zero_noise = context->zero_noise;
    record->rescue = context->rescue_used;
    record->ladder = context->ladder;
    record->before_sum = context->audit.sum_error;
    record->before_max = context->audit.maximum_error;
    record->eligible = context->audit.eligible_count;
    record->invalid = context->audit.invalid_count;
    record->unresolved = context->audit.unresolved_count;
    record->selection_complete = context->solver.samples != NULL;
    record->packet_limit = context->packet_limit;
    record->pixel_limit = work->pixel_limit;
    record->distance_limit = work->distance_limit;
    record->color_limit = context->config.max_colors;
    if (context->selection != NULL) {
        record->mask_squared = context->selection->threshold;
        record->selected = context->selection->selected_count;
    }
    record->constructed_colors = result->constructed_colors;
    record->transmitted_colors = result->transmitted_colors;
    record->noise_strength = context->application.strength;
    record->palette_count = context->application.color_count;
    for (color = 0; color < record->palette_count; ++color) {
        memcpy(record->palette[color], context->application.colors[
            context->application.palette[color]].working,
            sizeof(record->palette[color]));
    }
    record->committed = result->audit.outcome == SIXEL_REPAIR_FRAME_EMITTED;
    if (record->committed) {
        record->committed_pixels = result->painted;
        record->committed_bytes = result->packet_bytes;
        record->committed_gain = state->gain;
        record->committed_excess_gain = state->excess_gain;
        record->resolved_pixels = state->resolved;
        memcpy(record->transitions, state->transitions,
            sizeof(record->transitions));
    }
    memcpy(record->call, state->call, sizeof(record->call));
}
