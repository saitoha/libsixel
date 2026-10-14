/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_SELECT_H
#define LIBSIXEL_PROGRESSIVE_SELECT_H

#include "progressive-palette.h"
#include "palette.h"
#include "progressive-ladder.h"
#include "progressive-gpu.h"

typedef struct sixel_repair_candidate {
    sixel_palette_emitted_color_t color;
    size_t position;
    double error;
    double nearest;
    int used;
} sixel_repair_candidate_t;

/* One speculative selection owns its workspace, but borrows the immutable
 * source and audited errors. Every scan and candidate reduction resumes at
 * its exact cursor. No image-sized pairwise distance matrix is allocated. */
typedef struct sixel_repair_selection {
    sixel_allocator_t *allocator;
    float const *source;
    double const *errors;
    unsigned char const *domain;
    sixel_palette_t const *transform;
    size_t count;
    int workspace;
    int palette_type;
    double stop_squared;
    double fraction;
    int full;
    size_t sample_limit;
    unsigned int candidate_limit;
    size_t *indices;
    size_t *scratch;
    unsigned char *selected;
    sixel_repair_sample_t *samples;
    sixel_repair_candidate_t *raw;
    size_t *hash;
    size_t hash_size;
    sixel_palette_emitted_color_t *colors;
    float (*working)[3];
    size_t unresolved;
    size_t selected_count;
    size_t sample_count;
    size_t raw_count;
    unsigned int color_count;
    size_t protected_position;
    size_t protected_raw;
    int protected_color;
    sixel_repair_candidate_t protected;
    double threshold;
    /* Optional owner state. Only its first unresolved sort builds knots;
     * later selections use the frozen rung and skip that sort entirely. */
    sixel_repair_ladder_t *ladder;
    int stage;
    size_t cursor;
    size_t ordinal;
    size_t sample;
    size_t width;
    size_t block;
    size_t left;
    size_t middle;
    size_t right;
    size_t finish;
    size_t destination;
    size_t raw_total;
    size_t hash_slot;
    int hashing;
    sixel_repair_candidate_t pending;
    size_t chosen;
    size_t best;
    /* Borrowed cache and immutable audited planes. Pending samples survive
     * yields until the matching CPU projection cursor consumes the batch. */
    sixel_repair_gpu_t *gpu;
    sixel_repair_gpu_view_t gpu_view;
    sixel_repair_gpu_result_t gpu_result;
    int gpu_policy;
    int gpu_used;
    size_t gpu_end;
    unsigned int gpu_sample;
} sixel_repair_selection_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_selection_new(sixel_repair_selection_t **selection,
    float const *source, double const *errors, unsigned char const *domain,
    size_t count, sixel_palette_t const *transform, int workspace,
    int palette_type, double stop_squared, double fraction, int full,
    size_t sample_limit, unsigned int candidate_limit,
    sixel_allocator_t *allocator);
SIXEL_INTERNAL_API void
sixel_repair_selection_free(sixel_repair_selection_t *selection);
/* A call processes at most 256 visits/evaluations before giving the owner
 * an opportunity to check cancellation and ordinary-operation priority. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_selection_advance(sixel_repair_selection_t *selection,
    sixel_repair_work_t *work, int *done);

#endif
