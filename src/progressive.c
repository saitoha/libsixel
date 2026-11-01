/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "progressive.h"
#include "progressive-measure.h"
#include "encoder.h"
#include "palette.h"
#include "palette-factory.h"
#include "output.h"
#include "output-factory.h"
#include "compat_stub.h"
#if HAVE_ERRNO_H
#include <errno.h>
#endif

enum {
    REPAIR_PREPARE_SELECT,
    REPAIR_PREPARE_PALETTE,
    REPAIR_PREPARE_APPLICATION_BEGIN,
    REPAIR_PREPARE_APPLY,
    REPAIR_PREPARE_RESCUE,
    REPAIR_PREPARE_READY,
    REPAIR_PREPARE_STALLED
};

enum {
    REPAIR_PACKET_RESTORE,
    REPAIR_PACKET_BUILD,
    REPAIR_PACKET_SERIALIZE,
    REPAIR_PACKET_COLLECT,
    REPAIR_PACKET_SORT,
    REPAIR_PACKET_CLEAR,
    REPAIR_PACKET_MARK,
    REPAIR_PACKET_RECORDS,
    REPAIR_PACKET_RECORD_SCAN,
    REPAIR_PACKET_READY,
    REPAIR_PACKET_TOO_SMALL
};

static void
sixel_repair_packet_reset(sixel_repair_context_t *context)
{
    sixel_indexed_packet_free(context->packet);
    context->packet = NULL;
    sixel_allocator_free(context->allocator, context->commit);
    context->commit = NULL;
    context->commit_count = 0;
    context->packet_limit = 0;
    context->packet_stage = REPAIR_PACKET_RESTORE;
    context->packet_cursor = context->packet_kept = context->packet_count = 0;
    context->packet_order = context->packet_scratch = NULL;
    context->video_guard_stage = 0;
    context->video_before_loss = context->video_after_loss = 0;
    context->video_cursor = 0;
}

static void
sixel_repair_ladder_sync(sixel_repair_context_t *context)
{
    if (context->ladder.allowance) {
        context->full_selection = context->ladder.count &&
            context->ladder.rung + 1 == context->ladder.count;
        context->zero_noise = context->ladder.noise_off;
    }
}

static void
sixel_repair_work_reset(sixel_repair_context_t *context)
{
    sixel_repair_packet_reset(context);
    sixel_repair_selection_free(context->selection);
    context->selection = NULL;
    memset(&context->solver, 0, sizeof(context->solver));
    context->prepare_stage = REPAIR_PREPARE_SELECT;
    context->video_stage = 0;
    context->video_maximum = 0;
    context->zero_noise = context->rescue_used = 0;
    context->rescue_count = 0;
    context->rescue_position = 0;
    memset(&context->application, 0, sizeof(context->application));
    sixel_repair_ladder_sync(context);
}

void
sixel_repair_video_disable_owned(sixel_repair_context_t *context)
{
    if (!context->video_active) return;
    context->video_active = 0;
    sixel_repair_work_reset(context);
    context->audit_started = 0;
}

SIXELSTATUS
sixel_repair_ladder_enable(sixel_progressive_t *context,
    unsigned int allowance)
{
    SIXELSTATUS status;

    if (context == NULL || (allowance != 1 && allowance != 2 &&
            allowance != 4)) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_encoder_operation_enter(context->encoder,
        SIXEL_ENCODER_OPERATION_NORMAL);
    if (status != SIXEL_OK) {
        return status;
    }
    if (context->generation != 0) {
        status = SIXEL_BAD_ARGUMENT;
    } else {
        context->ladder.allowance = allowance;
    }
    sixel_encoder_operation_leave(context->encoder);
    return status;
}

static int
sixel_repair_config_valid(sixel_repair_config_t const *config)
{
    return config != NULL && config->struct_size >= sizeof(*config) &&
        (config->stop_distance_auto == 0 ||
         config->stop_distance_auto == 1) &&
        isfinite(config->stop_distance) && config->stop_distance >= 0 &&
        isfinite(config->stop_distance * config->stop_distance) &&
        isfinite(config->selection_fraction) &&
        config->selection_fraction > 0 && config->selection_fraction <= 1 &&
        config->max_colors >= 1 && config->max_colors <= 255 &&
        config->sample_limit >= 1 && config->sample_limit <= 65536 &&
        config->candidate_limit >= config->max_colors &&
        config->candidate_limit <= 4096 &&
        isfinite(config->min_gain_ratio) && config->min_gain_ratio >= 0 &&
        config->min_gain_ratio < 1 && config->swap_limit <= 16 &&
        isfinite(config->noise_strength) && config->noise_strength >= 0 &&
        config->noise_strength <= sqrt(DBL_MAX) / 4 &&
        config->noise_phase_x < 64 && config->noise_phase_y < 64 &&
        (config->noise_channel_rgb == 0 || config->noise_channel_rgb == 1) &&
        config->gpu_policy >= -1 &&
        config->gpu_policy <= SIXEL_GPU_POLICY_FORCE;
}

SIXELSTATUS
sixel_repair_config_init(sixel_repair_config_t *config, size_t size)
{
    if (config == NULL || size < sizeof(*config)) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* Do not read a size tag from uninitialized caller storage, and leave
     * any future tail untouched rather than assuming ownership of SIZE. */
    memset(config, 0, sizeof(*config));
    config->struct_size = sizeof(*config);
    config->stop_distance_auto = 1;
    config->selection_fraction = 0.25;
    config->max_colors = 32;
    config->sample_limit = 4096;
    config->candidate_limit = 128;
    config->min_gain_ratio = 0.001;
    config->swap_limit = 2;
    config->noise_strength = 0.055;
    config->gpu_policy = -1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_repair_context_new_owned(sixel_repair_context_t **context,
                         sixel_encoder_t *encoder,
                         sixel_repair_config_t const *config,
                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_repair_context_t *created;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *context = NULL;
    if (encoder == NULL || !sixel_repair_config_valid(config)) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = SIXEL_BAD_ARGUMENT;
    if (!encoder->sixdelta_enabled || encoder->revision_exhausted ||
            encoder->residual_enabled || encoder->progressive_context != NULL) {
        goto end;
    }
    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (status != SIXEL_OK) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }
    created = sixel_allocator_calloc(allocator, 1, sizeof(*created));
    if (created == NULL) {
        sixel_allocator_unref(allocator);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    created->encoder = encoder;
    created->allocator = allocator;
    created->config = *config;
    sixel_encoder_ref(encoder);
    encoder->progressive_context = created;
    *context = created;
    status = SIXEL_OK;
end:
    return status;
}

SIXELSTATUS
sixel_repair_context_new(sixel_repair_context_t **context,
                         sixel_encoder_t *encoder,
                         sixel_repair_config_t const *config,
                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *context = NULL;
    status = sixel_encoder_operation_enter(encoder,
                                           SIXEL_ENCODER_OPERATION_NORMAL);
    if (status == SIXEL_OK) {
        status = sixel_repair_context_new_owned(context, encoder, config,
                                                allocator);
        sixel_encoder_operation_leave(encoder);
    }
    return status;
}

static void
sixel_repair_binding_release(sixel_repair_context_t *context)
{
    sixel_repair_work_reset(context);
    sixel_repair_gpu_free(context->gpu);
    context->gpu = NULL;
    sixel_allocator_free(context->allocator, context->indices);
    sixel_allocator_free(context->allocator, context->paint);
    sixel_allocator_free(context->allocator, context->gains);
    sixel_allocator_free(context->allocator, context->gpu_retained);
    context->gpu_retained = NULL;
    sixel_allocator_free(context->allocator, context->video_excluded);
    sixel_allocator_free(context->allocator, context->video_age);
    sixel_allocator_free(context->allocator, context->video_error);
    sixel_allocator_free(context->allocator, context->video_luma);
    sixel_allocator_free(context->allocator, context->video_after);
    sixel_allocator_free(context->allocator, context->video_priority);
    context->video_excluded = context->video_age = NULL;
    context->video_error = context->video_after = NULL;
    context->video_luma = NULL;
    context->video_priority = NULL;
    context->indices = context->paint = NULL;
    context->gains = NULL;
    sixel_frame_unref(context->source);
    sixel_allocator_free(context->encoder->allocator, context->domain);
    sixel_allocator_free(context->allocator, context->errors);
    if (context->transform != NULL) {
        context->transform->vtbl->unref(context->transform);
    }
    context->source = NULL;
    context->domain = NULL;
    context->errors = NULL;
    context->transform = NULL;
}

/* Transfer the context's retained encoder reference to its owner. Public
 * teardown releases it after leaving the gate; playback already retains its
 * encoder independently and may release that extra reference while owned. */
sixel_encoder_t *
sixel_repair_context_detach_owned(sixel_repair_context_t *context)
{
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;

    if (context == NULL) {
        return NULL;
    }
    encoder = context->encoder;
    allocator = context->allocator;
    encoder->progressive_context = NULL;
    sixel_repair_binding_release(context);
    sixel_allocator_free(allocator, context->measurement);
    sixel_allocator_free(allocator, context);
    sixel_allocator_unref(allocator);
    return encoder;
}

SIXELSTATUS
sixel_repair_context_destroy(sixel_repair_context_t *context)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    if (context == NULL) {
        return SIXEL_OK;
    }
    encoder = context->encoder;
    status = sixel_encoder_operation_enter(encoder,
                                           SIXEL_ENCODER_OPERATION_NORMAL);
    if (status != SIXEL_OK) {
        return status;
    }
    encoder = sixel_repair_context_detach_owned(context);
    sixel_encoder_operation_leave(encoder);
    sixel_encoder_unref(encoder);
    return SIXEL_OK;
}

/* Prepare the existing typed output transform once per binding. The black
 * palette entry is only a vehicle for its immutable color definitions; it
 * is not a candidate and never replaces encoder history. */
static SIXELSTATUS
sixel_repair_transform_new(sixel_repair_context_t *context)
{
    sixel_palette_entries_request_t bytes;
    sixel_palette_float32_entries_request_t floats;
    unsigned char black[3] = { 0, 0, 0 };
    float black_float[3] = { 0, 0, 0 };
    SIXELSTATUS status;

    status = sixel_palette_create_from_factory(&context->transform,
                                               context->allocator);
    if (status != SIXEL_OK) {
        return status;
    }
    bytes.entries = black;
    bytes.colors = 1;
    bytes.depth = 3;
    floats.entries = black_float;
    floats.colors = 1;
    floats.depth = 3 * sizeof(float);
    status = context->transform->vtbl->init_entries(context->transform,
                                                    &bytes);
    if (status == SIXEL_OK) {
        status = context->transform->vtbl->init_entries_float32(
            context->transform, &floats);
    }
    if (status == SIXEL_OK) {
        status = sixel_palette_prepare_output(context->transform,
            context->workspace, context->encoder->output_color_definition,
            context->encoder->output_colorspace);
    }
    return status;
}

SIXELSTATUS
sixel_repair_context_bind_owned(sixel_repair_context_t *context,
                          sixel_frame_t *source, int x, int y,
                          uint64_t generation)
{
    sixel_repair_context_t replacement;
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    double stop;
    size_t position;
    float const *old_pixels;
    float const *new_pixels;

    if (context == NULL || source == NULL || x < 0 || y < 0 ||
            generation == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder = context->encoder;
    memset(&replacement, 0, sizeof(replacement));
    replacement.encoder = encoder;
    replacement.allocator = context->allocator;
    replacement.config = context->config;
    replacement.ladder.allowance = context->ladder.allowance;
    replacement.ladder.noise_off = context->ladder.allowance &&
        context->config.noise_strength == 0;
    replacement.yield_requested = context->yield_requested;
    replacement.yield_priv = context->yield_priv;
    status = SIXEL_BAD_ARGUMENT;
    if (generation <= context->generation || encoder->revision_exhausted ||
            !encoder->sixdelta_enabled) {
        goto end;
    }
    status = sixel_encoder_prepare_repair(encoder, source,
        &replacement.source, &replacement.domain);
    if (status != SIXEL_OK) {
        goto end;
    }
    replacement.width = sixel_frame_get_width(replacement.source);
    replacement.height = sixel_frame_get_height(replacement.source);
    replacement.origin_x = x;
    replacement.origin_y = y;
    replacement.plane_width = encoder->sixdelta_plane_width;
    replacement.plane_height = encoder->sixdelta_plane_height;
    if (replacement.plane_width == 0) {
        replacement.plane_width = encoder->accumulation_valid
            ? encoder->accumulation_width : replacement.width;
        replacement.plane_height = encoder->accumulation_valid
            ? encoder->accumulation_height : replacement.height;
    }
    status = SIXEL_BAD_ARGUMENT;
    if (replacement.width > replacement.plane_width ||
            replacement.height > replacement.plane_height ||
            x > replacement.plane_width - replacement.width ||
            y > replacement.plane_height - replacement.height ||
            (size_t)replacement.width >
                SIZE_MAX / (size_t)replacement.height / sizeof(double)) {
        goto end;
    }
    replacement.count = (size_t)replacement.width * replacement.height;
    replacement.errors = sixel_allocator_malloc(context->allocator,
        replacement.count * sizeof(*replacement.errors));
    if (replacement.errors == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    replacement.workspace = encoder->working_colorspace;
    stop = context->config.stop_distance;
    if (context->config.stop_distance_auto) {
        stop = replacement.workspace == SIXEL_COLORSPACE_GAMMA ? 2.0 / 255
            : replacement.workspace == SIXEL_COLORSPACE_LINEAR ? 0.005
            : 0.01;
    }
    replacement.stop_squared = stop * stop;
    status = sixel_repair_transform_new(&replacement);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (sixel_encoder_is_cancel_requested(encoder)) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }
    replacement.generation = generation;
    replacement.ordinary_revision = encoder->ordinary_revision;
    replacement.configuration_revision = encoder->configuration_revision;
    replacement.history_revision = encoder->history_revision;
    /* Carry age only where the target and interpretation really survived.
     * An omitted ordinary pixel is not evidence of an unchanged source. */
    if (context->video_age != NULL &&
            context->width == replacement.width &&
            context->height == replacement.height &&
            context->origin_x == x && context->origin_y == y &&
            context->configuration_revision ==
                replacement.configuration_revision) {
        replacement.video_age = sixel_allocator_calloc(context->allocator,
            replacement.count, 1);
        if (replacement.video_age == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        old_pixels = (float const *)sixel_frame_get_pixels(context->source);
        new_pixels = (float const *)sixel_frame_get_pixels(replacement.source);
        for (position = 0; position < replacement.count; ++position) {
            if (context->domain[position] && replacement.domain[position] &&
                    memcmp(old_pixels + position * 3,
                        new_pixels + position * 3, 3 * sizeof(float)) == 0) {
                replacement.video_age[position] =
                    context->video_age[position] < 255
                    ? context->video_age[position] + 1 : 255;
            }
        }
    }
    /* Publish once. Rejected refreshes cannot revive stale generations or
     * discard a valid in-progress audit. Acc and ordinary origin stay put. */
    sixel_repair_binding_release(context);
    replacement.measurement = context->measurement;
    *context = replacement;
    sixel_repair_measure_rebind(context);
    memset(&replacement, 0, sizeof(replacement));
    replacement.encoder = encoder;
    replacement.allocator = context->allocator;
    status = SIXEL_OK;
end:
    sixel_repair_binding_release(&replacement);
    return status;
}

SIXELSTATUS
sixel_repair_context_bind(sixel_repair_context_t *context,
                          sixel_frame_t *source, int x, int y,
                          uint64_t generation)
{
    sixel_thread_budget_scope_t policy_scope;
    SIXELSTATUS status;

    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_encoder_operation_enter(context->encoder,
                                           SIXEL_ENCODER_OPERATION_NORMAL);
    if (status == SIXEL_OK) {
        sixel_threading_object_enter(&policy_scope,
            &context->encoder->threading, 0);
        status = sixel_repair_context_bind_owned(context, source, x, y,
                                                 generation);
        sixel_thread_budget_leave(&policy_scope);
        sixel_encoder_operation_leave(context->encoder);
    }
    return status;
}

static int
sixel_repair_history_valid(sixel_repair_context_t const *context,
                           size_t position)
{
    sixel_encoder_t const *encoder;

    encoder = context->encoder;
    return encoder->accumulation_valid &&
        encoder->accumulation_pixels != NULL &&
        encoder->accumulation_width == context->plane_width &&
        encoder->accumulation_height == context->plane_height &&
        encoder->accumulation_pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
        encoder->accumulation_color_definition ==
            encoder->output_color_definition &&
        encoder->accumulation_colorspace == encoder->output_colorspace &&
        position < encoder->accumulation_pixels_size / 3 &&
        encoder->accumulation_valid_mask != NULL &&
        position < encoder->accumulation_valid_mask_size &&
        encoder->accumulation_valid_mask[position] != 0;
}

/* Return the next pixel in 32-by-24 tiles. Exact tile coordinates survive
 * budget yields, including partial right/bottom edges. Sub-tile checks keep
 * even very small distance budgets bounded without rescanning a prefix. */
static size_t
sixel_repair_audit_position(sixel_repair_context_t *context)
{
    int width;
    int height;
    size_t position;

    width = context->width - context->tile_x;
    height = context->height - context->tile_y;
    if (width > 32) {
        width = 32;
    }
    if (height > 24) {
        height = 24;
    }
    position = (size_t)(context->tile_y + context->tile_offset / width) *
        context->width + context->tile_x + context->tile_offset % width;
    ++context->tile_offset;
    if (context->tile_offset == width * height) {
        context->tile_offset = 0;
        context->tile_x += width;
        if (context->tile_x == context->width) {
            context->tile_x = 0;
            context->tile_y += height;
        }
    }
    return position;
}

static SIXELSTATUS
sixel_repair_retained_prepare(sixel_repair_context_t *context)
{
    int policy;

    if (context->gpu_retained != NULL || context->gpu_retained_disabled) {
        return SIXEL_OK;
    }
    policy = context->config.gpu_policy < 0 ? context->encoder->gpu_policy
        : context->config.gpu_policy;
    if (policy == SIXEL_GPU_POLICY_OFF ||
            (policy == SIXEL_GPU_POLICY_AUTO && context->count <
                context->encoder->gpu_palette_threshold)) return SIXEL_OK;
    if (context->count > SIZE_MAX / (3 * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    context->gpu_retained = sixel_allocator_calloc(context->allocator,
        context->count, 3 * sizeof(float));
    if (context->gpu_retained != NULL) return SIXEL_OK;
    if (policy == SIXEL_GPU_POLICY_FORCE) return SIXEL_BAD_ALLOCATION;
    context->gpu_retained_disabled = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_repair_context_audit_owned(sixel_repair_context_t *context,
                           sixel_repair_work_t *work,
                           sixel_repair_audit_t *audit)
{
    sixel_encoder_t *encoder;
    float const *source;
    float retained[3];
    double error;
    double target_luma;
    double retained_luma;
    size_t position;
    size_t plane_position;
    int gamma_history;
    int channel;
    SIXELSTATUS status;

    encoder = context->encoder;
    status = SIXEL_OK;
    if (context->source == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    audit->state_snapshot_valid = 1;
    audit->source_generation = context->generation;
    audit->history_revision = encoder->history_revision;
    audit->configuration_revision = encoder->configuration_revision;
    if (encoder->revision_exhausted ||
            context->ordinary_revision != encoder->ordinary_revision ||
            context->configuration_revision !=
                encoder->configuration_revision) {
        sixel_repair_work_reset(context);
        context->full_selection = 0;
        audit->outcome = SIXEL_REPAIR_NEEDS_SOURCE;
        goto end;
    }
    if (context->history_revision != encoder->history_revision) {
        sixel_repair_work_reset(context);
        context->full_selection = 0;
    }
    if (!context->audit_started ||
            context->history_revision != encoder->history_revision) {
        status = sixel_repair_retained_prepare(context);
        if (status != SIXEL_OK) goto end;
        context->audit = *audit;
        sixel_repair_measure_audit_begin(context);
        context->audit_started = 1;
        context->history_revision = encoder->history_revision;
        context->tile_x = context->tile_y = context->tile_offset = 0;
    }
    source = (float const *)sixel_frame_get_pixels(context->source);
    gamma_history = context->workspace == SIXEL_COLORSPACE_GAMMA &&
        sixel_palette_output_is_gamma(context->transform);
    while (context->tile_y < context->height) {
        if ((work->pixel_visits & 127U) == 0) {
            if (sixel_encoder_is_cancel_requested(encoder)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }
            if (sixel_encoder_operation_pending(encoder)) {
                *audit = context->audit;
                audit->outcome = SIXEL_REPAIR_YIELDED;
                audit->yield_reason = SIXEL_PROGRESSIVE_YIELD_UPDATE_PENDING;
                goto end;
            }
            if (context->yield_requested != NULL &&
                    context->yield_requested(context->yield_priv)) {
                *audit = context->audit;
                audit->outcome = SIXEL_REPAIR_YIELDED;
                audit->yield_reason = SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET;
                goto end;
            }
        }
        if (work->pixel_visits >= work->pixel_limit ||
                work->distance_evaluations >= work->distance_limit) {
            *audit = context->audit;
            audit->outcome = SIXEL_REPAIR_YIELDED;
            audit->yield_reason = SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET;
            goto end;
        }
        position = sixel_repair_audit_position(context);
        ++work->pixel_visits;
        context->errors[position] = 0;
        /* Unknown history is counted before any intersection with validity.
         * Transparent source positions never create a baseline obligation. */
        if (!context->domain[position]) {
            continue;
        }
        ++context->audit.domain_count;
        plane_position =
            (position / context->width + context->origin_y) *
                (size_t)context->plane_width +
            position % context->width + context->origin_x;
        if (!sixel_repair_history_valid(context, plane_position)) {
            ++context->audit.invalid_count;
            continue;
        }
        /* The identity output/work transform needs only the same byte
         * normalization as decode_output. Avoid per-pixel CMS dispatch in
         * the common video path, including the ordinary-painted halo. */
        if (gamma_history) {
            for (channel = 0; channel < 3; ++channel) {
                retained[channel] = (float)(encoder->accumulation_pixels[
                    plane_position * 3 + channel] / 255.0);
            }
        } else if (!sixel_palette_decode_output(context->transform,
                encoder->accumulation_pixels + plane_position * 3,
                context->workspace, retained)) {
            status = SIXEL_BAD_INPUT;
            context->audit_started = 0;
            goto end;
        }
        error = sixel_repair_distance2(source + position * 3, retained);
        if (context->gpu_retained != NULL) {
            memcpy(context->gpu_retained + position * 3, retained,
                sizeof(retained));
        }
        if (context->video_active) {
            if (!sixel_repair_video_luma(source + position * 3,
                    context->workspace, &target_luma) ||
                    !sixel_repair_video_luma(retained, context->workspace,
                        &retained_luma)) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            context->video_error[position] = target_luma - retained_luma;
            context->video_luma[position] = target_luma;
            if (context->video_excluded[position]) {
                context->errors[position] = 0;
                continue;
            }
        }
        ++work->distance_evaluations;
        ++context->audit.eligible_count;
        context->errors[position] = error;
        if (context->measurement != NULL) {
            sixel_repair_measure_pixel(context, position,
                source + position * 3, error);
        }
        context->audit.sum_error += error;
        if (error > context->audit.maximum_error) {
            context->audit.maximum_error = error;
        }
        if (error > context->stop_squared) {
            ++context->audit.unresolved_count;
        }
    }
    context->audit.complete = 1;
    context->audit.outcome = context->audit.invalid_count != 0
        ? SIXEL_REPAIR_NEEDS_BASELINE : context->audit.unresolved_count != 0
        ? SIXEL_REPAIR_READY : SIXEL_REPAIR_CONVERGED;
    *audit = context->audit;
end:
    return status;
}

SIXELSTATUS
sixel_repair_context_audit(sixel_repair_context_t *context,
                           sixel_repair_work_t *work,
                           sixel_repair_audit_t *audit)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    if (audit == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(audit, 0, sizeof(*audit));
    if (context == NULL || work == NULL || work->pixel_limit == 0 ||
            work->distance_limit == 0 || work->pixel_visits != 0 ||
            work->distance_evaluations != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder = context->encoder;
    status = sixel_encoder_operation_enter(encoder,
                                           SIXEL_ENCODER_OPERATION_REPAIR);
    if (status == SIXEL_FALSE) {
        audit->outcome = SIXEL_REPAIR_YIELDED;
        return SIXEL_OK;
    }
    if (status != SIXEL_OK) {
        return status;
    }
    status = sixel_repair_context_audit_owned(context, work, audit);
    sixel_encoder_operation_leave(encoder);
    return status;
}

static SIXELSTATUS
sixel_repair_selection_gpu_setup(sixel_repair_context_t *context)
{
    sixel_repair_selection_t *selection;
    sixel_repair_gpu_view_t *view;
    int policy;
    SIXELSTATUS status;

    selection = context->selection;
    if (context->gpu_retained == NULL) return SIXEL_OK;
    policy = context->config.gpu_policy < 0 ? context->encoder->gpu_policy
        : context->config.gpu_policy;
    /* Admission is per unresolved population, before bounded scan slices.
     * Palette application independently uses the final selected count. */
    if (policy == SIXEL_GPU_POLICY_OFF ||
            (policy == SIXEL_GPU_POLICY_AUTO &&
             context->audit.unresolved_count <
                context->encoder->gpu_palette_threshold)) return SIXEL_OK;
    if (context->gpu == NULL) {
        status = sixel_repair_gpu_new(&context->gpu, context->allocator);
        if (status != SIXEL_OK) {
            return policy == SIXEL_GPU_POLICY_FORCE ? status : SIXEL_OK;
        }
    }
    selection->gpu = context->gpu;
    selection->gpu_policy = policy;
    view = &selection->gpu_view;
    view->source = selection->source;
    view->retained = context->gpu_retained;
    /* Preparation is reached only after a complete CPU audit established
     * validity throughout this domain. Video exclusions are batch filters. */
    view->domain = view->valid = context->domain;
    view->count = context->count;
    view->width = context->width;
    view->height = context->height;
    view->origin_x = context->origin_x;
    view->origin_y = context->origin_y;
    view->plane_width = context->plane_width;
    view->plane_height = context->plane_height;
    view->workspace = context->workspace;
    view->source_generation = context->generation;
    view->history_revision = context->history_revision;
    view->configuration_revision = context->configuration_revision;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_repair_application_setup(sixel_repair_context_t *context)
{
    sixel_repair_application_t *application;
    int policy;
    SIXELSTATUS status;

    if (context->indices == NULL) {
        context->indices = sixel_allocator_malloc(context->allocator,
                                                  context->count);
    }
    if (context->paint == NULL) {
        context->paint = sixel_allocator_malloc(context->allocator,
                                                context->count);
    }
    if (context->gains == NULL) {
        context->gains = sixel_allocator_malloc(context->allocator,
            context->count * sizeof(*context->gains));
    }
    if (context->indices == NULL || context->paint == NULL ||
            context->gains == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    application = &context->application;
    memset(application, 0, sizeof(*application));
    application->source = (float const *)
        sixel_frame_get_pixels(context->source);
    application->errors = context->errors;
    application->selected = context->selection->selected;
    application->count = context->count;
    application->width = context->width;
    application->origin_x = context->origin_x;
    application->origin_y = context->origin_y;
    application->protected_position = context->selection->protected_position;
    application->strength = context->zero_noise
        || context->video_active ? 0 : context->config.noise_strength;
    application->phase_x = context->config.noise_phase_x;
    application->phase_y = context->config.noise_phase_y;
    application->channel_rgb = context->config.noise_channel_rgb;
    application->colors = context->rescue_used ? context->rescue_colors
        : context->selection->colors;
    application->palette = context->rescue_used ? context->rescue_palette
        : context->solver.palette;
    application->color_count = context->rescue_used ? context->rescue_count
        : context->solver.colors;
    application->indices = context->indices;
    application->paint = context->paint;
    application->gains = context->gains;
    policy = context->config.gpu_policy < 0 ? context->encoder->gpu_policy
        : context->config.gpu_policy;
    /* Admission uses the complete selected population, not a dispatch slice
     * whose size changes with the caller's work budget. No tuning is implied
     * by reusing the encoder's existing palette threshold here. */
    if (policy == SIXEL_GPU_POLICY_AUTO &&
            context->selection->selected_count <
                context->encoder->gpu_palette_threshold) {
        policy = SIXEL_GPU_POLICY_OFF;
    }
    if (policy == SIXEL_GPU_POLICY_OFF) {
        sixel_repair_gpu_evict(context->gpu);
    } else if (context->gpu == NULL) {
        status = sixel_repair_gpu_new(&context->gpu, context->allocator);
        if (status != SIXEL_OK) {
            if (policy == SIXEL_GPU_POLICY_FORCE) return status;
            policy = SIXEL_GPU_POLICY_OFF;
        }
    }
    application->gpu = context->gpu;
    application->gpu_policy = policy;
    return sixel_repair_application_begin(application);
}

static SIXELSTATUS
sixel_repair_rescue_advance(sixel_repair_context_t *context,
                            sixel_repair_work_t *work, int *done)
{
    sixel_palette_emitted_color_t color;
    sixel_palette_emitted_color_t const *entry;
    float const *source;
    size_t position;
    unsigned int i;
    unsigned int chunk;
    double distance;

    *done = 0;
    source = (float const *)sixel_frame_get_pixels(context->source);
    for (chunk = 0; chunk < 128; ++chunk) {
        if (context->rescue_position == context->count ||
                context->rescue_count == context->config.max_colors) {
            *done = 1;
            return SIXEL_OK;
        }
        if (work->pixel_visits >= work->pixel_limit ||
                work->distance_evaluations >= work->distance_limit) {
            return SIXEL_OK;
        }
        position = context->rescue_position++;
        ++work->pixel_visits;
        if (!context->selection->selected[position]) {
            continue;
        }
        if (!sixel_palette_project_output(context->transform,
                source + position * 3, context->workspace,
                context->encoder->palette_type, &color)) {
            return SIXEL_BAD_INPUT;
        }
        distance = sixel_repair_distance2(source + position * 3,
                                           color.working);
        ++work->distance_evaluations;
        if (!sixel_repair_improves(context->errors[position], distance)) {
            continue;
        }
        for (i = 0; i < context->rescue_count; ++i) {
            entry = context->rescue_colors + i;
            if (entry->type == color.type &&
                    memcmp(entry->fields, color.fields,
                           sizeof(color.fields)) == 0) {
                break;
            }
        }
        if (i == context->rescue_count) {
            context->rescue_colors[i] = color;
            context->rescue_palette[i] = i;
            ++context->rescue_count;
        }
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_repair_context_prepare_owned(sixel_repair_context_t *context,
    sixel_repair_work_t *work, sixel_repair_audit_t *audit)
{
    sixel_encoder_t *encoder;
    sixel_repair_selection_t *selection;
    SIXELSTATUS status;
    int done;
    int pending;

    encoder = context->encoder;
    status = sixel_repair_context_audit_owned(context, work, audit);
    if (status != SIXEL_OK || audit->outcome != SIXEL_REPAIR_READY) {
        goto end;
    }
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT) {
        audit->outcome = SIXEL_REPAIR_UNSUPPORTED;
        goto end;
    }
    while (1) {
        if (sixel_encoder_is_cancel_requested(encoder)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }
        pending = sixel_encoder_operation_pending(encoder);
        if (pending || (context->yield_requested != NULL &&
                context->yield_requested(context->yield_priv)) ||
                work->pixel_visits >= work->pixel_limit ||
                work->distance_evaluations >= work->distance_limit) {
            audit->outcome = SIXEL_REPAIR_YIELDED;
            audit->yield_reason = pending
                ? SIXEL_PROGRESSIVE_YIELD_UPDATE_PENDING
                : SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET;
            goto end;
        }
        selection = context->selection;
        switch (context->prepare_stage) {
        case REPAIR_PREPARE_SELECT:
            sixel_repair_measure_phase(context,
                SIXEL_REPAIR_MEASURE_SELECT, work);
            if (selection == NULL) {
                sixel_repair_ladder_sync(context);
                if (context->video_active) {
                    status = sixel_repair_video_select(context, work, &done);
                    if (status != SIXEL_OK) goto end;
                    if (!done) break;
                }
                status = sixel_repair_selection_new(&context->selection,
                    (float const *)sixel_frame_get_pixels(context->source),
                    context->errors, context->domain, context->count,
                    context->transform, context->workspace,
                    encoder->palette_type, context->stop_squared,
                    context->config.selection_fraction,
                    context->video_active || context->full_selection,
                    context->video_active ? 128 : context->config.sample_limit,
                    context->video_active ? context->config.max_colors
                        : context->config.candidate_limit,
                    context->allocator);
                if (status != SIXEL_OK) {
                    goto end;
                }
                selection = context->selection;
                if (context->video_active) {
                    selection->priority = context->video_priority;
                    selection->priority_maximum = context->video_maximum;
                } else if (context->ladder.allowance) {
                    selection->ladder = &context->ladder;
                }
                status = sixel_repair_selection_gpu_setup(context);
                if (status != SIXEL_OK) {
                    sixel_repair_selection_free(context->selection);
                    context->selection = NULL;
                    goto end;
                }
            }
            status = sixel_repair_selection_advance(selection, work, &done);
            sixel_repair_ladder_sync(context);
            if (status != SIXEL_OK) {
                /* Share the public FORCE-unavailable outcome with palette
                 * application; do not expose a raw backend status here. */
                break;
            }
            if (done) {
                /* A fixed rung can become empty after a successful packet.
                 * It is an exhausted mask, not a full-image stall. */
                if (selection->sample_count == 0 &&
                        context->ladder.allowance &&
                        sixel_repair_ladder_descend(&context->ladder,
                            SIXEL_REPAIR_LADDER_EMPTY_MASK)) {
                    sixel_repair_work_reset(context);
                    break;
                }
                status = sixel_repair_palette_begin(&context->solver,
                    selection->samples, selection->sample_count,
                    (float const (*)[3])selection->working,
                    selection->color_count, context->config.max_colors,
                    selection->protected_color,
                    context->config.min_gain_ratio,
                    context->video_active ? 0 : context->config.swap_limit);
                if (status != SIXEL_OK) {
                    goto end;
                }
                context->prepare_stage = REPAIR_PREPARE_PALETTE;
            }
            break;
        case REPAIR_PREPARE_PALETTE:
            sixel_repair_measure_phase(context,
                SIXEL_REPAIR_MEASURE_PALETTE, work);
            status = sixel_repair_palette_advance(&context->solver, work,
                                                   &done);
            if (status != SIXEL_OK) {
                goto end;
            }
            if (done) {
                context->prepare_stage = REPAIR_PREPARE_APPLICATION_BEGIN;
            }
            break;
        case REPAIR_PREPARE_APPLICATION_BEGIN:
            sixel_repair_measure_phase(context,
                SIXEL_REPAIR_MEASURE_APPLY, work);
            status = sixel_repair_application_setup(context);
            if (status == SIXEL_OK) {
                context->prepare_stage = REPAIR_PREPARE_APPLY;
            }
            break;
        case REPAIR_PREPARE_APPLY:
            sixel_repair_measure_phase(context,
                SIXEL_REPAIR_MEASURE_APPLY, work);
            status = sixel_repair_application_advance(&context->application,
                work, &done);
            if (status != SIXEL_OK || !done) {
                break;
            }
            if (context->application.painted) {
                context->prepare_stage = REPAIR_PREPARE_READY;
            } else if (context->video_active) {
                context->prepare_stage = REPAIR_PREPARE_STALLED;
            } else if (context->ladder.allowance &&
                    sixel_repair_ladder_descend(&context->ladder,
                        SIXEL_REPAIR_LADDER_NO_PAINT)) {
                /* No-paint evaluations descend without spending a packet
                 * allowance. Only the final full-mask recovery may stall. */
                sixel_repair_work_reset(context);
            } else if (!context->full_selection) {
                sixel_repair_work_reset(context);
                context->full_selection = 1;
            } else if (!context->zero_noise) {
                context->zero_noise = 1;
                context->prepare_stage = REPAIR_PREPARE_APPLICATION_BEGIN;
            } else if (!context->rescue_used) {
                context->rescue_used = 1;
                context->prepare_stage = REPAIR_PREPARE_RESCUE;
            } else {
                context->prepare_stage = REPAIR_PREPARE_STALLED;
            }
            break;
        case REPAIR_PREPARE_RESCUE:
            sixel_repair_measure_phase(context,
                SIXEL_REPAIR_MEASURE_RESCUE, work);
            status = sixel_repair_rescue_advance(context, work, &done);
            if (status != SIXEL_OK || !done) {
                break;
            }
            if (context->rescue_count == 0) {
                context->prepare_stage = REPAIR_PREPARE_STALLED;
            } else {
                context->prepare_stage = REPAIR_PREPARE_APPLICATION_BEGIN;
            }
            break;
        case REPAIR_PREPARE_READY:
            goto end;
        case REPAIR_PREPARE_STALLED:
            audit->outcome = SIXEL_REPAIR_STALLED;
            goto end;
        default:
            status = SIXEL_LOGIC_ERROR;
            break;
        }
        if (status != SIXEL_OK) {
            if (status == SIXEL_FEATURE_ERROR) {
                /* Forced GPU repair has no CPU fallback on this build or
                 * device. This is a supported API outcome, not completion. */
                sixel_repair_work_reset(context);
                audit->outcome = SIXEL_REPAIR_UNSUPPORTED;
                status = SIXEL_OK;
            }
            goto end;
        }
    }
end:
    if (status != SIXEL_OK) {
        audit->outcome = SIXEL_REPAIR_NONE;
        if (status != SIXEL_BAD_ALLOCATION && status != SIXEL_INTERRUPTED) {
            /* A failed numerical stage may have advanced a private cursor.
             * Rebuild it on retry; never skip a failed projection and later
             * certify the unchecked suffix as a complete artifact. */
            sixel_repair_work_reset(context);
            context->full_selection = 0;
        }
    }
    return status;
}

SIXELSTATUS
sixel_repair_context_prepare(sixel_repair_context_t *context,
    sixel_repair_work_t *work, sixel_repair_audit_t *audit)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;

    if (audit == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(audit, 0, sizeof(*audit));
    if (context == NULL || work == NULL || work->pixel_limit == 0 ||
            work->distance_limit == 0 || work->pixel_visits != 0 ||
            work->distance_evaluations != 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder = context->encoder;
    status = sixel_encoder_operation_enter(encoder,
                                           SIXEL_ENCODER_OPERATION_REPAIR);
    if (status == SIXEL_FALSE) {
        audit->outcome = SIXEL_REPAIR_YIELDED;
        return SIXEL_OK;
    }
    if (status != SIXEL_OK) {
        return status;
    }
    status = sixel_repair_context_prepare_owned(context, work, audit);
    sixel_encoder_operation_leave(encoder);
    return status;
}

/* Sorting only accepted pixels avoids making a byte-budget retry a new
 * quality decision. Ties use bound row-major order, which is also absolute
 * plane order because the binding's origin and stride are immutable. */
static int
sixel_repair_packet_sort(sixel_repair_context_t *context)
{
    size_t count;
    size_t a;
    size_t b;
    size_t *swap;
    int take_left;

    count = context->packet_count;
    if (context->sort_width >= count) {
        return 1;
    }
    if (context->sort_destination == context->sort_finish) {
        context->sort_block = context->sort_finish;
        if (context->sort_block == count) {
            swap = context->packet_order;
            context->packet_order = context->packet_scratch;
            context->packet_scratch = swap;
            context->sort_width = context->sort_width > count / 2
                ? count : context->sort_width * 2;
            context->sort_block = 0;
            if (context->sort_width >= count) {
                return 1;
            }
        }
        context->sort_left = context->sort_block;
        context->sort_middle =
            count - context->sort_block < context->sort_width
            ? count : context->sort_block + context->sort_width;
        context->sort_right = context->sort_middle;
        context->sort_finish =
            count - context->sort_middle < context->sort_width
            ? count : context->sort_middle + context->sort_width;
        context->sort_destination = context->sort_block;
    }
    take_left = context->sort_right == context->sort_finish;
    if (context->sort_left == context->sort_middle) {
        take_left = 0;
    } else if (!take_left) {
        a = context->packet_order[context->sort_left];
        b = context->packet_order[context->sort_right];
        take_left = context->gains[a] > context->gains[b] ||
            (context->gains[a] == context->gains[b] && a < b);
    }
    context->packet_scratch[context->sort_destination++] =
        context->packet_order[take_left ? context->sort_left++
                                       : context->sort_right++];
    return 0;
}

static SIXELSTATUS
sixel_repair_packet_advance(sixel_repair_context_t *context,
    sixel_repair_work_t *work, int *ready)
{
    sixel_indexed_packet_request_t request;
    sixel_repair_commit_t *record;
    sixel_repair_application_t const *application;
    size_t position;
    unsigned int chunk;
    SIXELSTATUS status;
    int done;

    *ready = 0;
    application = &context->application;
    for (chunk = 0; chunk < 256; ++chunk) {
        if (context->packet_stage == REPAIR_PACKET_READY ||
                context->packet_stage == REPAIR_PACKET_TOO_SMALL) {
            *ready = 1;
            return SIXEL_OK;
        }
        if (work->pixel_visits >= work->pixel_limit) {
            return SIXEL_OK;
        }
        if (context->packet_stage == REPAIR_PACKET_SERIALIZE) {
            status = sixel_indexed_packet_advance(context->packet,
                work->pixel_limit, &work->pixel_visits, &done);
            if (status != SIXEL_OK || !done) {
                return status;
            }
            sixel_repair_measure_serialized(context);
            if (context->packet->bytes <= context->packet_limit) {
                context->packet_stage = REPAIR_PACKET_RECORDS;
            } else if (context->packet_kept == 1) {
                context->packet_stage = REPAIR_PACKET_TOO_SMALL;
            } else {
                sixel_indexed_packet_free(context->packet);
                context->packet = NULL;
                context->packet_kept = (context->packet_kept + 1) / 2;
                context->packet_cursor = 0;
                context->packet_stage = context->packet_count
                    ? REPAIR_PACKET_CLEAR : REPAIR_PACKET_COLLECT;
                if (!context->packet_count) {
                    context->packet_order = context->selection->indices;
                    context->packet_scratch = context->selection->scratch;
                }
            }
            /* The core has its own 256-unit checkpoint; return to the owner
             * before doing another batch or considering output admission. */
            return SIXEL_OK;
        }
        ++work->pixel_visits;
        switch (context->packet_stage) {
        case REPAIR_PACKET_RESTORE:
            position = context->packet_cursor++;
            context->paint[position] = context->gains[position] > 0;
            if (context->measurement != NULL) {
                sixel_repair_measure_proposal(context, position);
            }
            context->packet_kept += context->paint[position];
            if (context->packet_cursor == context->count) {
                context->packet_cursor = 0;
                context->packet_stage = REPAIR_PACKET_BUILD;
            }
            break;
        case REPAIR_PACKET_BUILD:
            memset(&request, 0, sizeof(request));
            request.width = context->width;
            request.height = context->height;
            request.indices = context->indices;
            request.paint = context->paint;
            request.colors = application->colors;
            request.palette = application->palette;
            request.color_count = application->color_count;
            request.byte_limit = context->packet_limit;
            request.has_8bit_control = context->packet_8bit;
            request.has_gri_arg_limit = context->packet_gri_limit;
            status = sixel_indexed_packet_new(&context->packet, &request,
                                               context->allocator);
            if (status != SIXEL_OK) {
                return status;
            }
            context->packet_stage = REPAIR_PACKET_SERIALIZE;
            break;
        case REPAIR_PACKET_COLLECT:
            position = context->packet_cursor++;
            if (context->gains[position] > 0) {
                context->packet_order[context->packet_count++] = position;
            }
            if (context->packet_cursor == context->count) {
                context->sort_width = 1;
                context->sort_block = context->sort_destination = 0;
                context->sort_finish = 0;
                context->packet_stage = REPAIR_PACKET_SORT;
            }
            break;
        case REPAIR_PACKET_SORT:
            if (sixel_repair_packet_sort(context)) {
                context->packet_cursor = 0;
                context->packet_stage = REPAIR_PACKET_CLEAR;
            }
            break;
        case REPAIR_PACKET_CLEAR:
            context->paint[context->packet_cursor++] = 0;
            if (context->packet_cursor == context->count) {
                context->packet_cursor = 0;
                context->packet_stage = REPAIR_PACKET_MARK;
            }
            break;
        case REPAIR_PACKET_MARK:
            context->paint[context->packet_order[context->packet_cursor++]] = 1;
            if (context->packet_cursor == context->packet_kept) {
                context->packet_cursor = 0;
                context->packet_stage = REPAIR_PACKET_BUILD;
            }
            break;
        case REPAIR_PACKET_RECORDS:
            if (context->packet_kept > SIZE_MAX / sizeof(*context->commit)) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            context->commit = sixel_allocator_malloc(context->allocator,
                context->packet_kept * sizeof(*context->commit));
            if (context->commit == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            context->packet_cursor = context->commit_count = 0;
            context->packet_stage = REPAIR_PACKET_RECORD_SCAN;
            break;
        case REPAIR_PACKET_RECORD_SCAN:
            position = context->packet_cursor++;
            if (context->paint[position]) {
                record = context->commit + context->commit_count++;
                if (context->measurement != NULL) {
                    sixel_repair_measure_commit_pixel(context, position);
                }
                record->position =
                    (position / context->width + context->origin_y) *
                        (size_t)context->plane_width +
                    position % context->width + context->origin_x;
                memcpy(record->rgb, application->colors[
                    application->palette[context->indices[position]]].rgb, 3);
            }
            if (context->packet_cursor == context->count) {
                context->packet_stage = REPAIR_PACKET_READY;
            }
            break;
        default:
            return SIXEL_LOGIC_ERROR;
        }
    }
    return SIXEL_OK;
}

/* The default descriptor has the same destination as ordinary encoding. A
 * short host write is not complete acceptance: finish it or report failure. */
static int
sixel_repair_write_all(char *data, int size, void *priv)
{
    sixel_encoder_t *encoder;
    ssize_t written;
    size_t offset;

    encoder = priv;
    offset = 0;
    while (offset < (size_t)size) {
        written = sixel_compat_write(encoder->outfd, data + offset,
                                     (size_t)size - offset);
#if HAVE_ERRNO_H
        if (written < 0 && errno == EINTR) {
            continue;
        }
#endif
        if (written <= 0 || (size_t)written > (size_t)size - offset) {
            return -1;
        }
        offset += (size_t)written;
    }
    return 0;
}

/* Capture every state-derived field while owning the encoder. In particular
 * a public wrapper must not peek at context after releasing that ownership. */
static void
sixel_repair_stats_snapshot(sixel_repair_context_t const *context,
                            sixel_repair_result_t *result)
{
    sixel_progressive_stats_t *stats;
    sixel_repair_audit_t const *audit;

    stats = &result->stats;
    audit = &result->audit;
    stats->state_snapshot_valid = audit->state_snapshot_valid;
    if (!audit->state_snapshot_valid) {
        return;
    }
    stats->audit_complete = audit->complete;
    stats->source_generation = audit->source_generation;
    stats->history_revision = audit->history_revision;
    stats->configuration_revision = audit->configuration_revision;
    stats->working_colorspace = context->encoder->working_colorspace;
    stats->output_colorspace = context->encoder->output_colorspace;
    stats->output_color_definition = context->encoder->output_color_definition;
    stats->backend = (context->application.gpu_used ||
        (context->selection != NULL && context->selection->gpu_used))
        ? SIXEL_PROGRESSIVE_BACKEND_METAL : SIXEL_PROGRESSIVE_BACKEND_CPU;
    stats->selection_round = context->selection_round;
    stats->full_selection = context->full_selection;
    stats->domain_count = audit->domain_count;
    stats->eligible_count = audit->eligible_count;
    stats->invalid_count = audit->invalid_count;
    stats->unresolved_count = audit->unresolved_count;
    stats->maximum_distance = sqrt(audit->maximum_error);
    stats->sum_squared_error = audit->sum_error;
    stats->total_pixel_visits = context->total_pixel_visits;
    stats->total_distance_evaluations = context->total_distance_evaluations;
    stats->zero_noise_used = context->zero_noise;
    stats->rescue_used = context->rescue_used;
    if (context->selection != NULL) {
        stats->selected_count = context->selection->selected_count;
        stats->sampled_cost_before = context->solver.initial_cost;
        stats->sampled_cost_after = context->solver.cost;
        stats->constructed_colors = context->application.color_count;
    }
    stats->stage = SIXEL_PROGRESSIVE_STAGE_AUDIT;
    if (audit->outcome == SIXEL_REPAIR_FRAME_EMITTED) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_COMMIT;
    } else if (audit->outcome == SIXEL_REPAIR_CONVERGED ||
               audit->outcome == SIXEL_REPAIR_STALLED) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_COMPLETE;
    } else if (context->prepare_stage == REPAIR_PREPARE_READY) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_PACKET;
    } else if (context->rescue_used) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_RESCUE;
    } else if (context->prepare_stage >= REPAIR_PREPARE_APPLICATION_BEGIN) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_APPLY;
    } else if (context->prepare_stage == REPAIR_PREPARE_PALETTE) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_PALETTE;
    } else if (audit->complete) {
        stats->stage = SIXEL_PROGRESSIVE_STAGE_SELECT;
    }
}

SIXELSTATUS
sixel_repair_context_step(sixel_repair_context_t *context,
    sixel_repair_work_t *work, size_t byte_limit, sixel_output_t *output,
    sixel_repair_result_t *result)
{
    sixel_thread_budget_scope_t policy_scope;
    sixel_encoder_t *encoder;
    sixel_writer_controls_t controls;
    sixel_output_options_t options;
    SIXELSTATUS status;
    int ready;
    int next_full;
    int pending;
    int accepted;
    size_t position;
    sixel_output_t *default_output;

    if (result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (context == NULL || work == NULL || work->pixel_limit == 0 ||
            work->distance_limit == 0 || work->pixel_visits != 0 ||
            work->distance_evaluations != 0 || byte_limit == 0 ||
            byte_limit > INT_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    encoder = context->encoder;
    default_output = NULL;
    status = sixel_encoder_operation_enter_reason(encoder,
        SIXEL_ENCODER_OPERATION_REPAIR, &result->audit.yield_reason);
    if (status == SIXEL_FALSE) {
        result->audit.outcome = SIXEL_REPAIR_YIELDED;
        return SIXEL_OK;
    }
    if (status != SIXEL_OK) {
        return status;
    }
    sixel_threading_object_enter(&policy_scope, &encoder->threading, 0);
    sixel_repair_measure_begin(context, work);
    /* Reserve counters for the entire requested quantum before any output.
     * This conservative check makes every subsequent increment nonfallible. */
    if (work->pixel_limit > UINT64_MAX - context->total_pixel_visits ||
            work->distance_limit >
                UINT64_MAX - context->total_distance_evaluations ||
            context->selection_round == UINT64_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    memset(&controls, 0, sizeof(controls));
    memset(&options, 0, sizeof(options));
    if (output != NULL) {
        status = sixel_output_get_writer_controls(output, &controls);
        if (status == SIXEL_OK) {
            status = sixel_output_get_encoder_options(output, &options);
        }
    } else {
        controls.has_8bit_control = encoder->f8bit;
        controls.penetrate_multiplexer = encoder->penetrate_multiplexer;
        options.has_gri_arg_limit = encoder->has_gri_arg_limit;
        options.ormode = encoder->ormode;
        options.transparent_offset_left = encoder->transparent_offset_left;
        options.transparent_offset_top = encoder->transparent_offset_top;
    }
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_repair_context_prepare_owned(context, work, &result->audit);
    if (status != SIXEL_OK || result->audit.outcome != SIXEL_REPAIR_READY) {
        goto end;
    }
    if (encoder->fuse_macro || encoder->macro_number >= 0 ||
            encoder->fdrcs || encoder->output_is_png ||
            controls.skip_dcs_envelope || controls.skip_header ||
            controls.penetrate_multiplexer || options.ormode ||
            options.transparent_offset_left || options.transparent_offset_top) {
        result->audit.outcome = SIXEL_REPAIR_UNSUPPORTED;
        goto end;
    }
    result->constructed_colors = context->application.color_count;
    sixel_repair_measure_phase(context, SIXEL_REPAIR_MEASURE_PACKET, work);
    if (context->packet_limit != byte_limit ||
            context->packet_8bit != controls.has_8bit_control ||
            context->packet_gri_limit != options.has_gri_arg_limit) {
        sixel_repair_packet_reset(context);
        sixel_repair_measure_packet_begin(context);
        context->packet_limit = byte_limit;
        context->packet_8bit = controls.has_8bit_control;
        context->packet_gri_limit = options.has_gri_arg_limit;
    }
    while (1) {
        if (sixel_encoder_is_cancel_requested(encoder)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }
        pending = sixel_encoder_operation_pending(encoder);
        if (pending || (context->yield_requested != NULL &&
                context->yield_requested(context->yield_priv)) ||
                work->pixel_visits >= work->pixel_limit) {
            result->audit.outcome = SIXEL_REPAIR_YIELDED;
            result->audit.yield_reason = pending
                ? SIXEL_PROGRESSIVE_YIELD_UPDATE_PENDING
                : SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET;
            goto end;
        }
        status = sixel_repair_packet_advance(context, work, &ready);
        if (status != SIXEL_OK) {
            goto end;
        }
        if (ready) {
            break;
        }
    }
    if (context->packet_stage == REPAIR_PACKET_TOO_SMALL) {
        result->required_bytes = context->packet->bytes;
        result->audit.outcome = SIXEL_REPAIR_BUDGET_TOO_SMALL;
        goto end;
    }
    /* Fitting may change which neighbors are repainted. Test the actual
     * packet, including the unchanged halo, before admitting any bytes. */
    if (context->video_active) {
        ready = 0;
        while (!ready) {
            if (sixel_encoder_is_cancel_requested(encoder)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }
            if (work->pixel_visits >= work->pixel_limit ||
                    (context->yield_requested != NULL &&
                     context->yield_requested(context->yield_priv))) {
                result->audit.outcome = SIXEL_REPAIR_YIELDED;
                result->audit.yield_reason =
                    SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET;
                goto end;
            }
            status = sixel_repair_video_guard(context, work, &ready,
                                               &accepted);
            if (status != SIXEL_OK) goto end;
        }
        if (!accepted) {
            result->audit.outcome = SIXEL_REPAIR_STALLED;
            goto end;
        }
    }
    if (output == NULL) {
        status = sixel_encoder_core_create_output_from_factory(
            &default_output, sixel_repair_write_all, encoder,
            context->allocator);
        if (status != SIXEL_OK) {
            goto end;
        }
        output = default_output;
    }
    if (sixel_encoder_is_cancel_requested(encoder)) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }
    /* Computation can consume the final slack after the last checkpoint.
     * Keep the completed packet resumable when playback's deadline wins. */
    if (context->yield_requested != NULL &&
            context->yield_requested(context->yield_priv)) {
        result->audit.outcome = SIXEL_REPAIR_YIELDED;
        result->audit.yield_reason = SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET;
        goto end;
    }
    if (!sixel_encoder_operation_admit_output(encoder)) {
        result->audit.outcome = SIXEL_REPAIR_YIELDED;
        result->audit.yield_reason = SIXEL_PROGRESSIVE_YIELD_UPDATE_PENDING;
        goto end;
    }
    sixel_repair_measure_phase(context, SIXEL_REPAIR_MEASURE_WRITE, work);
    status = sixel_output_write_bytes(output,
        (char const *)context->packet->data, (int)context->packet->bytes);
    if (status != SIXEL_OK) {
        /* The ordered sink may have accepted a prefix. Its displayed state
         * is now unknown even when it reported failure before accepting any. */
        (void)sixel_encoder_invalidate_6delta_plane_owned(encoder);
        sixel_repair_work_reset(context);
        result->audit.complete = 0;
        result->audit.history_revision = encoder->history_revision;
        goto end;
    }
    /* Records already contain the exact emitted RGB and absolute positions.
     * Complete this nonfallible commit even if cancellation arrives now. */
    sixel_repair_measure_phase(context, SIXEL_REPAIR_MEASURE_COMMIT, work);
    sixel_encoder_commit_repair(encoder, context->commit,
                                 context->commit_count);
    if (context->video_active) {
        for (position = 0; position < context->count; ++position) {
            if (context->paint[position]) context->video_age[position] = 0;
        }
    }
    result->painted = context->commit_count;
    result->packet_bytes = context->packet->bytes;
    result->transmitted_colors = context->packet->colors;
    result->audit.history_revision = encoder->history_revision;
    result->audit.complete = 0;
    result->audit.outcome = SIXEL_REPAIR_FRAME_EMITTED;
end:
    if (status != SIXEL_OK) {
        result->audit.outcome = SIXEL_REPAIR_NONE;
        result->audit.yield_reason = SIXEL_PROGRESSIVE_YIELD_NONE;
    }
    context->total_pixel_visits += work->pixel_visits;
    context->total_distance_evaluations += work->distance_evaluations;
    sixel_repair_stats_snapshot(context, result);
    sixel_repair_measure_finish(context, work, result, status);
    if (result->audit.outcome == SIXEL_REPAIR_FRAME_EMITTED) {
        next_full = !context->full_selection;
        context->selection_round += context->full_selection != 0;
        sixel_repair_ladder_commit(&context->ladder);
        sixel_repair_work_reset(context);
        if (!context->ladder.allowance) {
            context->full_selection = next_full;
        }
        context->audit_started = 0;
        context->history_revision = encoder->history_revision;
    } else if (status != SIXEL_OK && status != SIXEL_BAD_ALLOCATION &&
               status != SIXEL_INTERRUPTED) {
        sixel_repair_work_reset(context);
        context->audit_started = 0;
        context->full_selection = 0;
    }
    sixel_output_unref(default_output);
    sixel_thread_budget_leave(&policy_scope);
    sixel_encoder_operation_leave(encoder);
    return status;
}

SIXELSTATUS
sixel_progressive_config_init(sixel_progressive_config_t *config, size_t size)
{
    return sixel_repair_config_init(config, size);
}

SIXELSTATUS
sixel_progressive_new(sixel_progressive_t **context, sixel_encoder_t *encoder,
    sixel_progressive_config_t const *config, sixel_allocator_t *allocator)
{
    return sixel_repair_context_new(context, encoder, config, allocator);
}

SIXELSTATUS
sixel_progressive_bind_frame(sixel_progressive_t *context,
    sixel_frame_t *frame, int origin_x, int origin_y, uint64_t generation)
{
    return sixel_repair_context_bind(context, frame, origin_x, origin_y,
                                      generation);
}

SIXELSTATUS
sixel_progressive_step(sixel_progressive_t *context,
    sixel_progressive_budget_t const *budget, sixel_output_t *output,
    sixel_progressive_stats_t *stats)
{
    sixel_repair_work_t work;
    sixel_repair_result_t result;
    SIXELSTATUS status;

    /* Inspect only the size tag before touching caller-owned layout fields.
     * Future tails are neither read nor cleared by this implementation. */
    if (stats == NULL || stats->struct_size < sizeof(*stats)) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(stats, 0, sizeof(*stats));
    stats->struct_size = sizeof(*stats);
    if (budget == NULL || budget->struct_size < sizeof(*budget)) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(&work, 0, sizeof(work));
    work.pixel_limit = budget->max_pixel_visits;
    work.distance_limit = budget->max_distance_evaluations;
    status = sixel_repair_context_step(context, &work,
        budget->max_packet_bytes, output, &result);
    *stats = result.stats;
    stats->struct_size = sizeof(*stats);
    stats->yield_reason = result.audit.yield_reason;
    stats->painted_pixels = result.painted;
    stats->packet_bytes = result.packet_bytes;
    stats->required_packet_bytes = result.required_bytes;
    stats->transmitted_colors = result.transmitted_colors;
    stats->pixel_visits = work.pixel_visits;
    stats->distance_evaluations = work.distance_evaluations;
    switch (result.audit.outcome) {
    case SIXEL_REPAIR_FRAME_EMITTED:
        stats->outcome = SIXEL_PROGRESSIVE_FRAME_EMITTED;
        break;
    case SIXEL_REPAIR_YIELDED:
        stats->outcome = SIXEL_PROGRESSIVE_YIELDED;
        break;
    case SIXEL_REPAIR_CONVERGED:
        stats->outcome = SIXEL_PROGRESSIVE_CONVERGED;
        break;
    case SIXEL_REPAIR_STALLED:
        stats->outcome = SIXEL_PROGRESSIVE_STALLED;
        break;
    case SIXEL_REPAIR_NEEDS_BASELINE:
        stats->outcome = SIXEL_PROGRESSIVE_NEEDS_BASELINE;
        break;
    case SIXEL_REPAIR_NEEDS_SOURCE:
        stats->outcome = SIXEL_PROGRESSIVE_NEEDS_SOURCE;
        break;
    case SIXEL_REPAIR_BUDGET_TOO_SMALL:
        stats->outcome = SIXEL_PROGRESSIVE_BUDGET_TOO_SMALL;
        break;
    case SIXEL_REPAIR_UNSUPPORTED:
        stats->outcome = SIXEL_PROGRESSIVE_UNSUPPORTED;
        break;
    default:
        stats->outcome = SIXEL_PROGRESSIVE_NONE;
        break;
    }
    return status;
}

void
sixel_progressive_unref(sixel_progressive_t *context)
{
    (void)sixel_repair_context_destroy(context);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
