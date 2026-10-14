/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_H
#define LIBSIXEL_PROGRESSIVE_H

#include <sixel.h>
#include <6cells.h>
#include <stdint.h>
#include "progressive-palette.h"
#include "progressive-select.h"
#include "progressive-apply.h"
#include "encoder-core-packet.h"

/* Private state keeps speculative work separate from committed history. */
typedef sixel_progressive_config_t sixel_repair_config_t;

enum sixel_repair_outcome {
    SIXEL_REPAIR_NONE,
    SIXEL_REPAIR_READY,
    SIXEL_REPAIR_YIELDED,
    SIXEL_REPAIR_CONVERGED,
    SIXEL_REPAIR_NEEDS_BASELINE,
    SIXEL_REPAIR_NEEDS_SOURCE,
    SIXEL_REPAIR_STALLED,
    SIXEL_REPAIR_UNSUPPORTED,
    SIXEL_REPAIR_FRAME_EMITTED,
    SIXEL_REPAIR_BUDGET_TOO_SMALL
};

typedef struct sixel_repair_audit {
    int outcome;
    int yield_reason;
    int state_snapshot_valid;
    int complete;
    uint64_t source_generation;
    uint64_t history_revision;
    uint64_t configuration_revision;
    size_t domain_count;
    size_t eligible_count;
    size_t invalid_count;
    size_t unresolved_count;
    double maximum_error;
    double sum_error;
} sixel_repair_audit_t;

typedef struct sixel_repair_result {
    sixel_repair_audit_t audit;
    sixel_progressive_stats_t stats;
    size_t painted;
    size_t packet_bytes;
    size_t required_bytes;
    unsigned int constructed_colors;
    unsigned int transmitted_colors;
} sixel_repair_result_t;

/* Only the encoder owns authoritative retained pixels. A binding owns its
 * immutable target, source-domain mask and speculative derived errors. */
typedef struct sixel_progressive {
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_repair_config_t config;
    struct sixel_repair_measure_state *measurement;
    sixel_repair_ladder_t ladder;
    /* Private presentation deadlines; API clients use their work budgets. */
    int (*yield_requested)(void *priv);
    void *yield_priv;
    sixel_frame_t *source;
    unsigned char *domain;
    double *errors;
    /* Derived canonical history for GPU proposals, refreshed by each CPU
     * audit. It never becomes a second owner of committed display history. */
    float *gpu_retained;
    int gpu_retained_disabled;
    sixel_palette_t *transform;
    size_t count;
    int width;
    int height;
    int origin_x;
    int origin_y;
    int plane_width;
    int plane_height;
    int workspace;
    double stop_squared;
    uint64_t total_pixel_visits;
    uint64_t total_distance_evaluations;
    uint64_t selection_round;
    uint64_t generation;
    uint64_t ordinary_revision;
    uint64_t configuration_revision;
    uint64_t history_revision;
    int audit_started;
    int tile_x;
    int tile_y;
    int tile_offset;
    sixel_repair_audit_t audit;
    sixel_repair_selection_t *selection;
    sixel_repair_palette_t solver;
    sixel_repair_application_t application;
    sixel_repair_gpu_t *gpu;
    unsigned char *indices;
    unsigned char *paint;
    double *gains;
    int prepare_stage;
    int full_selection;
    int zero_noise;
    int rescue_used;
    size_t rescue_position;
    unsigned int rescue_count;
    sixel_palette_emitted_color_t rescue_colors[255];
    unsigned int rescue_palette[255];
    sixel_indexed_packet_t *packet;
    struct sixel_repair_commit *commit;
    size_t commit_count;
    size_t packet_limit;
    int packet_8bit;
    int packet_gri_limit;
    int packet_stage;
    size_t packet_cursor;
    size_t packet_kept;
    size_t packet_count;
    size_t *packet_order;
    size_t *packet_scratch;
    size_t sort_width;
    size_t sort_block;
    size_t sort_left;
    size_t sort_middle;
    size_t sort_right;
    size_t sort_finish;
    size_t sort_destination;
} sixel_repair_context_t;

/* Playback holds the encoder gate and owns an independent encoder reference.
 * Detach transfers the context's encoder reference; the caller releases it. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_new_owned(sixel_repair_context_t **context,
    sixel_encoder_t *encoder, sixel_repair_config_t const *config,
    sixel_allocator_t *allocator);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_bind_owned(sixel_repair_context_t *context,
    sixel_frame_t *source, int x, int y, uint64_t generation);
SIXEL_INTERNAL_API sixel_encoder_t *
sixel_repair_context_detach_owned(sixel_repair_context_t *context);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_config_init(sixel_repair_config_t *config, size_t size);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_new(sixel_repair_context_t **context,
                         sixel_encoder_t *encoder,
                         sixel_repair_config_t const *config,
                         sixel_allocator_t *allocator);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_destroy(sixel_repair_context_t *context);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_bind(sixel_repair_context_t *context,
                          sixel_frame_t *source, int x, int y,
                          uint64_t generation);
/* A resumable canonical audit. READY means residuals remain; it is not an
 * emitted frame or a terminal quality result. WORK starts with zero counts. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_audit(sixel_repair_context_t *context,
                           sixel_repair_work_t *work,
                           sixel_repair_audit_t *audit);
/* Private speculative preparation only. READY denotes a nonempty paint
 * artifact; this call never invokes the writer or changes retained history. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_prepare(sixel_repair_context_t *context,
    sixel_repair_work_t *work, sixel_repair_audit_t *audit);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_context_step(sixel_repair_context_t *context,
    sixel_repair_work_t *work, size_t byte_limit, sixel_output_t *output,
    sixel_repair_result_t *result);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
