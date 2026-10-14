/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <string.h>
#include "progressive-palette.h"

enum sixel_repair_palette_stage {
    REPAIR_PALETTE_INIT,
    REPAIR_PALETTE_SCORE,
    REPAIR_PALETTE_ADD,
    REPAIR_PALETTE_SWAP,
    REPAIR_PALETTE_REBUILD,
    REPAIR_PALETTE_DONE
};

double
sixel_repair_distance2(float const a[3], float const b[3])
{
    double x;
    double y;
    double z;

    /* Widen before subtraction. Float subtraction here would define a
     * different acceptance metric from the final canonical audit. */
    x = (double)a[0] - (double)b[0];
    y = (double)a[1] - (double)b[1];
    z = (double)a[2] - (double)b[2];
    return x * x + y * y + z * z;
}

static double
sixel_repair_margin(double before, double after)
{
    return 1e-12 + 1e-7 * (before > after ? before : after);
}

int
sixel_repair_improves(double before, double after)
{
    return before - after > sixel_repair_margin(before, after);
}

SIXELSTATUS
sixel_repair_palette_begin(sixel_repair_palette_t *solver,
                           sixel_repair_sample_t *samples, size_t count,
                           float const (*candidates)[3], unsigned int colors,
                           unsigned int max_colors, int protected_candidate,
                           double min_gain_ratio, unsigned int swap_limit)
{
    if (solver == NULL || samples == NULL || candidates == NULL ||
            count == 0 || count > 65536 || colors == 0 || colors > 4096 ||
            max_colors == 0 || max_colors > 255 ||
            protected_candidate < -1 || protected_candidate >= (int)colors ||
            !isfinite(min_gain_ratio) || min_gain_ratio < 0.0 ||
            min_gain_ratio >= 1.0 || swap_limit > 16) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(solver, 0, sizeof(*solver));
    solver->samples = samples;
    solver->sample_count = count;
    solver->candidates = candidates;
    solver->candidate_count = colors;
    solver->max_colors = max_colors;
    solver->protected_candidate = protected_candidate;
    solver->min_gain_ratio = min_gain_ratio;
    solver->swap_limit = swap_limit;
    solver->stage = REPAIR_PALETTE_INIT;
    return SIXEL_OK;
}

/* Keep is always a competitor, but never an entry that another pixel can
 * paint. The second error includes keep and permits exact O(samples)
 * replacement scoring without an image-sized pairwise distance matrix. */
static void
sixel_repair_palette_cache(sixel_repair_sample_t *sample,
                           double error, unsigned int slot)
{
    if (error < sample->best) {
        sample->second = sample->best;
        sample->best = error;
        sample->winner = (int)slot;
    } else if (error < sample->second) {
        sample->second = error;
    }
}

static void
sixel_repair_palette_score_reset(sixel_repair_palette_t *solver)
{
    solver->sample = 0;
    solver->candidate = 0;
    solver->slot = 0;
    solver->gain = 0;
    solver->numerical_margin = 0;
    solver->best_gain = 0;
    solver->best_candidate = 0;
    solver->best_slot = 0;
}

SIXELSTATUS
sixel_repair_palette_advance(sixel_repair_palette_t *solver,
                             sixel_repair_work_t *work, int *done)
{
    sixel_repair_sample_t *sample;
    double distance;
    double after;
    double retained;
    unsigned int index;
    unsigned int chunk;
    int swapping;

    if (solver == NULL || work == NULL || done == NULL ||
            work->pixel_limit == 0 || work->distance_limit == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    *done = 0;
    chunk = 0;
    while (solver->stage != REPAIR_PALETTE_DONE) {
        if (chunk++ == 256 || work->pixel_visits >= work->pixel_limit ||
                work->distance_evaluations >= work->distance_limit) {
            return SIXEL_OK;
        }
        if (solver->stage == REPAIR_PALETTE_INIT) {
            sample = &solver->samples[solver->sample++];
            ++work->pixel_visits;
            if (!isfinite(sample->source[0]) ||
                    !isfinite(sample->source[1]) ||
                    !isfinite(sample->source[2]) ||
                    !isfinite(sample->error) || sample->error < 0 ||
                    sample->population == 0) {
                return SIXEL_BAD_INPUT;
            }
            sample->best = sample->second = sample->error;
            sample->winner = -1;
            solver->initial_cost += sample->population * sample->error;
            if (!isfinite(solver->initial_cost)) {
                return SIXEL_BAD_INPUT;
            }
            if (solver->sample < solver->sample_count) {
                continue;
            }
            solver->cost = solver->initial_cost;
            sixel_repair_palette_score_reset(solver);
            solver->stage = REPAIR_PALETTE_SCORE;
            if (solver->protected_candidate >= 0) {
                index = (unsigned int)solver->protected_candidate;
                solver->palette[solver->colors++] = index;
                solver->used[index] = 1;
                solver->cost = 0;
                solver->stage = REPAIR_PALETTE_ADD;
            }
            continue;
        }
        if (solver->stage == REPAIR_PALETTE_ADD ||
                solver->stage == REPAIR_PALETTE_REBUILD) {
            sample = &solver->samples[solver->sample];
            if (solver->stage == REPAIR_PALETTE_REBUILD &&
                    solver->rebuild_slot == 0) {
                sample->best = sample->second = sample->error;
                sample->winner = -1;
            }
            index = solver->stage == REPAIR_PALETTE_ADD
                ? solver->colors - 1 : solver->rebuild_slot;
            distance = sixel_repair_distance2(sample->source,
                solver->candidates[solver->palette[index]]);
            ++work->distance_evaluations;
            if (!isfinite(distance)) {
                return SIXEL_BAD_INPUT;
            }
            sixel_repair_palette_cache(sample, distance, index);
            if (solver->stage == REPAIR_PALETTE_REBUILD &&
                    ++solver->rebuild_slot < solver->colors) {
                continue;
            }
            solver->rebuild_slot = 0;
            solver->cost += sample->population * sample->best;
            ++work->pixel_visits;
            if (++solver->sample < solver->sample_count) {
                continue;
            }
            swapping = solver->stage == REPAIR_PALETTE_REBUILD;
            sixel_repair_palette_score_reset(solver);
            solver->stage = swapping || solver->colors == solver->max_colors
                ? REPAIR_PALETTE_SWAP : REPAIR_PALETTE_SCORE;
            continue;
        }
        swapping = solver->stage == REPAIR_PALETTE_SWAP;
        if (swapping && solver->swaps == solver->swap_limit) {
            solver->stage = REPAIR_PALETTE_DONE;
            continue;
        }
        if (swapping && solver->slot == 0 &&
                solver->protected_candidate >= 0) {
            /* Protected provenance survives every optional medoid swap. */
            ++solver->slot;
        }
        if (swapping && solver->slot == solver->colors) {
            if (solver->best_gain <= 0) {
                solver->stage = REPAIR_PALETTE_DONE;
            } else {
                index = solver->palette[solver->best_slot];
                solver->used[index] = 0;
                index = solver->best_candidate;
                solver->palette[solver->best_slot] = index;
                solver->used[index] = 1;
                solver->cost = 0;
                ++solver->swaps;
                solver->sample = 0;
                solver->rebuild_slot = 0;
                solver->stage = REPAIR_PALETTE_REBUILD;
            }
            continue;
        }
        if (solver->candidate == solver->candidate_count) {
            if (swapping) {
                ++solver->slot;
                solver->candidate = 0;
                continue;
            }
            if (solver->best_gain <= 0) {
                solver->stage = solver->colors == 0
                    ? REPAIR_PALETTE_DONE : REPAIR_PALETTE_SWAP;
                sixel_repair_palette_score_reset(solver);
                continue;
            }
            index = solver->best_candidate;
            solver->palette[solver->colors++] = index;
            solver->used[index] = 1;
            solver->cost = 0;
            solver->sample = 0;
            solver->stage = REPAIR_PALETTE_ADD;
            continue;
        }
        if (solver->used[solver->candidate]) {
            ++solver->candidate;
            continue;
        }
        sample = &solver->samples[solver->sample];
        distance = sixel_repair_distance2(sample->source,
            solver->candidates[solver->candidate]);
        ++work->distance_evaluations;
        ++work->pixel_visits;
        if (!isfinite(distance)) {
            return SIXEL_BAD_INPUT;
        }
        retained = swapping && sample->winner == (int)solver->slot
            ? sample->second : sample->best;
        after = distance < retained ? distance : retained;
        solver->gain += sample->population * (sample->best - after);
        solver->numerical_margin += sample->population *
            sixel_repair_margin(sample->best, after);
        if (++solver->sample < solver->sample_count) {
            continue;
        }
        /* Strict comparisons preserve candidate order and then slot order
         * at ties. The optional-gain floor does not suppress first paint. */
        if (solver->gain > solver->best_gain &&
                solver->gain > solver->numerical_margin &&
                (swapping || solver->colors == 0 ||
                 solver->gain > solver->min_gain_ratio *
                                solver->initial_cost)) {
            solver->best_gain = solver->gain;
            solver->best_candidate = solver->candidate;
            solver->best_slot = solver->slot;
        }
        solver->sample = 0;
        solver->gain = 0;
        solver->numerical_margin = 0;
        ++solver->candidate;
    }
    *done = 1;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
