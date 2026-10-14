/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_PALETTE_H
#define LIBSIXEL_PROGRESSIVE_PALETTE_H

#include <sixel.h>
#include <stddef.h>

/* Samples keep individual displayed-error fallbacks, even when source
 * colors coincide. Population is the sole weight in the repair objective. */
typedef struct sixel_repair_sample {
    float source[3];
    double error;
    size_t population;
    size_t position;
    double best;
    double second;
    int winner;
} sixel_repair_sample_t;

typedef struct sixel_repair_work {
    size_t pixel_limit;
    size_t distance_limit;
    size_t pixel_visits;
    size_t distance_evaluations;
} sixel_repair_work_t;

/* All pointers borrow context-owned storage. Cursors survive a work yield;
 * this solver neither emits bytes nor observes/mutates encoder history. */
typedef struct sixel_repair_palette {
    sixel_repair_sample_t *samples;
    float const (*candidates)[3];
    size_t sample_count;
    unsigned int candidate_count;
    unsigned int max_colors;
    unsigned int swap_limit;
    int protected_candidate;
    double min_gain_ratio;
    unsigned int palette[255];
    unsigned char used[4096];
    unsigned int colors;
    unsigned int swaps;
    int stage;
    size_t sample;
    unsigned int candidate;
    unsigned int slot;
    unsigned int rebuild_slot;
    unsigned int best_candidate;
    unsigned int best_slot;
    double initial_cost;
    double cost;
    double gain;
    double numerical_margin;
    double best_gain;
} sixel_repair_palette_t;

SIXEL_INTERNAL_API double
sixel_repair_distance2(float const a[3], float const b[3]);
SIXEL_INTERNAL_API int
sixel_repair_improves(double before, double after);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_palette_begin(sixel_repair_palette_t *solver,
                           sixel_repair_sample_t *samples, size_t count,
                           float const (*candidates)[3], unsigned int colors,
                           unsigned int max_colors, int protected_candidate,
                           double min_gain_ratio, unsigned int swap_limit);
/* DONE is false on a work yield. The caller checks cancellation and normal
 * priority between chunks of at most 256 distance evaluations. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_palette_advance(sixel_repair_palette_t *solver,
                             sixel_repair_work_t *work, int *done);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
