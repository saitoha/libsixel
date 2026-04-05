/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Public entry points for the k-medoids quantizer and its runtime knobs.
 * palette.c selects this solver when -Q medoids is active.
 */

#ifndef LIBSIXEL_PALETTE_KMEDOIDS_H
#define LIBSIXEL_PALETTE_KMEDOIDS_H

#include <stdint.h>

#include "palette.h"
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sixel_kmedoids_algo {
    SIXEL_PALETTE_KMEDOIDS_ALGO_PAM = 0,
    SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA,
    SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS,
    SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM,
    SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO,
} sixel_kmedoids_algo_t;

SIXELSTATUS
sixel_palette_build_kmedoids(sixel_palette_t *palette,
                             unsigned char const *data,
                             unsigned int length,
                             int pixelformat,
                             sixel_allocator_t *allocator,
                             sixel_logger_t *logger,
                             int *job_seq,
                             char const *engine_name,
                             sixel_palette_telemetry_t *telemetry);

SIXELSTATUS
sixel_palette_build_kmedoids_float32(sixel_palette_t *palette,
                                     float const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     sixel_logger_t *logger,
                                     int *job_seq,
                                     char const *engine_name,
                                     sixel_palette_telemetry_t *telemetry);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_algo_override(int enabled,
                                 sixel_kmedoids_algo_t algo);

SIXEL_INTERNAL_API sixel_kmedoids_algo_t
sixel_get_kmedoids_algo(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_seed_override(int enabled,
                                 uint32_t seed);

SIXEL_INTERNAL_API uint32_t
sixel_get_kmedoids_seed(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_iter_override(int enabled,
                                 unsigned int iter_count);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_iter(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_sample_override(int enabled,
                                   unsigned int sample_count);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_sample(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_clara_trials_override(int enabled,
                                         unsigned int trials);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clara_trials(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_clara_sample_override(int enabled,
                                         unsigned int sample_count);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clara_sample(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_clarans_local_override(int enabled,
                                          unsigned int local_searches);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clarans_local(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_clarans_neighbors_override(int enabled,
                                              unsigned int neighbors);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clarans_neighbors(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_bandit_iter_override(int enabled,
                                        unsigned int iter_count);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_iter(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_bandit_candidates_override(int enabled,
                                              unsigned int candidates);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_candidates(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_bandit_batch_override(int enabled,
                                         unsigned int batch_size);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_batch(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_histbits_override(int enabled,
                                     unsigned int histbits);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_histbits(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_point_budget_override(int enabled,
                                         unsigned int point_budget);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_point_budget(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_rare_keep_override(int enabled,
                                      unsigned int rare_keep);

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_rare_keep(void);

SIXEL_INTERNAL_API void
sixel_set_kmedoids_prune_mass_override(int enabled,
                                       double prune_mass);

SIXEL_INTERNAL_API double
sixel_get_kmedoids_prune_mass(void);

SIXEL_INTERNAL_API double
sixel_kmedoids_test_swap_cost_cutoff(double const *points,
                                     double const *weights,
                                     unsigned int point_count,
                                     unsigned int const *nearest_slot,
                                     double const *nearest_dist,
                                     double const *second_dist,
                                     unsigned int replace_slot,
                                     unsigned int candidate_point,
                                     unsigned int const *order,
                                     double cutoff,
                                     int *early_stop_out);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_clarans_guided_sets(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    unsigned char const *flags,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    unsigned int *hot_points,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots,
    unsigned int *hot_slot_count_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_pam_polish_cost(double const *points,
                                    double const *weights,
                                    unsigned int point_count,
                                    unsigned int const *initial_medoids,
                                    unsigned int k,
                                    double *before_cost_out,
                                    double *after_cost_out,
                                    unsigned int *iterations_out,
                                    sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KMEDOIDS_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
