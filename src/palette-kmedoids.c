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
 * k-medoids palette builder.
 *
 * The implementation keeps medoids anchored to observed samples so every
 * palette center remains an actual source color when final merge is disabled.
 * Four solver frontends are provided and selected through -Q
 * medoids:algo=...:
 *   - pam: full swap search.
 *   - sample (CLARA): repeated PAM runs on subsamples, scored globally.
 *   - random (CLARANS): randomized neighborhood search.
 *   - bandit (BanditPAM): swap pruning with progressive mini-batch scoring.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmedoids.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"


#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define SIXEL_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
# define SIXEL_TLS __thread
#else
# define SIXEL_TLS
#endif

static SIXEL_TLS int sixel_kmedoids_algo_override_enabled = 0;
static SIXEL_TLS sixel_kmedoids_algo_t sixel_kmedoids_algo_override_value
    = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
static SIXEL_TLS int sixel_kmedoids_seed_override_enabled = 0;
static SIXEL_TLS uint32_t sixel_kmedoids_seed_override_value = 1u;
static SIXEL_TLS int sixel_kmedoids_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_iter_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_sample_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_sample_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clara_trials_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_clara_trials_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clara_sample_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_clara_sample_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clarans_local_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_clarans_local_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clarans_neighbors_override_enabled = 0;
static SIXEL_TLS unsigned int
    sixel_kmedoids_clarans_neighbors_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_bandit_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_bandit_iter_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_bandit_candidates_override_enabled = 0;
static SIXEL_TLS unsigned int
    sixel_kmedoids_bandit_candidates_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_bandit_batch_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_bandit_batch_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_histbits_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_histbits_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_point_budget_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_point_budget_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_rare_keep_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_rare_keep_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_prune_mass_override_enabled = 0;
static SIXEL_TLS double sixel_kmedoids_prune_mass_override_value = 0.0;
static SIXEL_TLS double const *sixel_kmedoids_distance_cache = NULL;
static SIXEL_TLS unsigned int sixel_kmedoids_distance_cache_size = 0u;
static SIXEL_TLS unsigned int
    sixel_kmedoids_clarans_guided_full_build_count = 0u;

#undef SIXEL_TLS

typedef struct sixel_kmedoids_unique_slot {
    uint64_t key0;
    uint64_t key1;
    uint64_t key2;
    unsigned int index;
    int used;
} sixel_kmedoids_unique_slot_t;

typedef struct sixel_kmedoids_bin_rank {
    unsigned int index;
    unsigned int count;
} sixel_kmedoids_bin_rank_t;

typedef struct sixel_kmedoids_bin_variance_rank {
    unsigned int index;
    double score;
} sixel_kmedoids_bin_variance_rank_t;

typedef struct sixel_kmedoids_candidate_rank {
    unsigned int active_index;
    double cost;
} sixel_kmedoids_candidate_rank_t;

typedef struct sixel_kmedoids_point_weight_rank {
    unsigned int index;
    double weight;
} sixel_kmedoids_point_weight_rank_t;

static uint32_t
sixel_kmedoids_rng_next(uint32_t *state);

static SIXELSTATUS
sixel_kmedoids_run_pam(double const *points,
                       double const *weights,
                       unsigned int point_count,
                       unsigned int k,
                       unsigned int max_iterations,
                       unsigned int const *initial_medoids,
                       unsigned int *medoids_out,
                       double *cost_out,
                       unsigned int *iterations_out,
                       sixel_allocator_t *allocator);

static double *
sixel_kmedoids_clarans_get_candidate_distance_row(
    unsigned int point_count,
    unsigned int candidate,
    unsigned int cache_size,
    unsigned int *cache_keys,
    double *cache_rows,
    uint32_t *cache_generation,
    uint32_t *cache_row_epoch,
    uint32_t *next_epoch_io,
    unsigned int *next_slot_io,
    uint32_t **row_generation_out,
    uint32_t *row_epoch_out);

static double
sixel_kmedoids_swap_cost_with_cutoff_row_generation(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out);

static double
sixel_kmedoids_swap_cost_prefix_with_cutoff_row_generation(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int prefix_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out);

static double
sixel_kmedoids_swap_cost_with_cutoff_row(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out);

SIXEL_INTERNAL_API double
sixel_kmedoids_test_swap_cost_cutoff_with_row_generation(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out);

static int
sixel_kmedoids_clarans_evaluate_candidate_slots(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int candidate,
    unsigned int const *probe_slots,
    unsigned int probe_slot_count,
    unsigned int k,
    unsigned int const *residual_order,
    double const *damage_scores,
    unsigned int *slot_orders,
    unsigned char *slot_dirty,
    uint32_t *slot_generation,
    uint32_t slot_generation_id,
    sixel_kmedoids_point_weight_rank_t *slot_rank_work,
    uint64_t *seen_pairs,
    uint32_t *seen_generation,
    uint32_t seen_generation_id,
    unsigned int pair_capacity,
    unsigned int pair_mask,
    unsigned int cache_size,
    unsigned int *cache_keys,
    double *cache_rows,
    uint32_t *cache_generation,
    uint32_t *cache_row_epoch,
    uint32_t *cache_epoch_next,
    unsigned int *next_slot_io,
    double cutoff,
    unsigned int cheap_prefix_count,
    int enable_cheap_bound,
    unsigned int *best_slot_out,
    double *best_cost_out,
    unsigned int *evaluated_pairs_out);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_pick_sample_indices(unsigned int point_count,
                                        unsigned int sample_size,
                                        uint32_t seed,
                                        unsigned int **indices_out,
                                        sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_kmedoids_test_assign_points(double const *points,
                                  double const *weights,
                                  unsigned int point_count,
                                  unsigned int const *medoids,
                                  unsigned int k,
                                  unsigned int *nearest_slot,
                                  double *nearest_dist,
                                  double *second_dist,
                                  unsigned int *second_slot,
                                  double *cost_out);

SIXEL_INTERNAL_API void
sixel_kmedoids_test_update_assignments_after_swap(double const *points,
                                                  double const *weights,
                                                  unsigned int point_count,
                                                  unsigned int const *medoids,
                                                  unsigned int k,
                                                  unsigned int swapped_slot,
                                                  unsigned int new_medoid,
                                                  unsigned int *nearest_slot,
                                                  double *nearest_dist,
                                                  double *second_dist,
                                                  unsigned int *second_slot,
                                                  double *cost_out);

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

SIXEL_INTERNAL_API double
sixel_kmedoids_test_swap_cost_cutoff_with_row(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_eval_order_residual(
    double const *weights,
    double const *nearest_dist,
    unsigned int point_count,
    unsigned int *order_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_apply_eval_order_delta(
    double const *weights,
    double const *nearest_dist_before,
    double const *nearest_dist_after,
    unsigned int point_count,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int delta_threshold,
    unsigned int *order_io,
    int *full_refresh_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_clarans_slot_order(
    double const *weights,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int point_count,
    unsigned int k,
    unsigned int slot,
    unsigned int *order_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_clarans_slot_order_lazy(
    double const *weights,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int point_count,
    unsigned int k,
    unsigned int slot,
    unsigned int const *probe_slots,
    unsigned int probe_count,
    unsigned int *order_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_clarans_slot_order_dirty_rebuild(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot_before,
    double const *nearest_dist_before,
    double const *second_dist_before,
    unsigned int const *nearest_slot_after,
    double const *nearest_dist_after,
    double const *second_dist_after,
    unsigned int swapped_slot,
    unsigned int const *changed_old_slots,
    unsigned int const *changed_new_slots,
    unsigned int changed_count,
    unsigned int *slot_orders_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_clarans_candidate_batch_best(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int k,
    unsigned int candidate,
    unsigned int const *slots,
    unsigned int slot_count,
    unsigned int const *slot_orders,
    double cutoff,
    unsigned int *best_slot_out,
    double *best_cost_out,
    unsigned int *evaluated_pairs_out,
    sixel_allocator_t *allocator);

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
sixel_kmedoids_test_collect_samples(
    unsigned char const *data,
    unsigned int length,
    unsigned int channels,
    unsigned int pixel_stride,
    int input_is_float32,
    unsigned int histbits,
    unsigned int point_budget,
    unsigned int rare_keep,
    double prune_mass,
    uint32_t seed,
    double **samples_out,
    double **sample_weights_out,
    unsigned int *sample_count_out,
    unsigned int *visible_count_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_apply_clarans_guided_delta(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot_before,
    double const *nearest_dist_before,
    unsigned char const *flags_before,
    unsigned int const *nearest_slot_after,
    double const *nearest_dist_after,
    unsigned char const *flags_after,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int old_medoid,
    unsigned int new_medoid,
    unsigned int *hot_points_out,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots_out,
    unsigned int *hot_slot_count_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_kmedoids_test_reset_clarans_guided_full_build_count(void);

SIXEL_INTERNAL_API unsigned int
sixel_kmedoids_test_get_clarans_guided_full_build_count(void);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_two_step_pam_polish_cost(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *initial_medoids,
    unsigned int k,
    double *before_cost_out,
    double *after_first_cost_out,
    double *after_second_cost_out,
    unsigned int *first_iterations_out,
    unsigned int *second_iterations_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_bandit_select_topk(
    unsigned int const *active_in,
    double const *costs_in,
    unsigned int count,
    unsigned int keep,
    unsigned int *selected_out,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API unsigned int
sixel_kmedoids_test_clarans_cache_size(unsigned int point_count,
                                       unsigned int k);

SIXEL_INTERNAL_API unsigned int
sixel_kmedoids_test_clarans_cheap_prefix_count(unsigned int point_count);

static int
sixel_palette_kmedoids_log_start(sixel_logger_t *logger,
                                 int *job_seq,
                                 char const *engine_name,
                                 char const *role,
                                 char const *phase)
{
    int job_id;

    job_id = -1;
    if (logger == NULL) {
        return job_id;
    }
    if (job_seq != NULL) {
        job_id = *job_seq;
        *job_seq += 1;
    }
    sixel_logger_logf(logger,
                      (role != NULL && role[0] != '\0') ? role : "palette",
                      "palette/build",
                      "start",
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "engine=%s phase=%s",
                      engine_name != NULL ? engine_name : "(unknown)",
                      phase);
    return job_id;
}

static void
sixel_palette_kmedoids_log_finish(sixel_logger_t *logger,
                                  int job_id,
                                  char const *engine_name,
                                  char const *role,
                                  char const *phase,
                                  char const *detail)
{
    char const *suffix;

    suffix = "";
    if (logger == NULL || job_id < 0) {
        return;
    }
    if (detail != NULL && detail[0] != '\0') {
        suffix = detail;
    }
    sixel_logger_logf(logger,
                      (role != NULL && role[0] != '\0') ? role : "palette",
                      "palette/build",
                      "finish",
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "engine=%s phase=%s%s%s",
                      engine_name != NULL ? engine_name : "(unknown)",
                      phase,
                      suffix[0] != '\0' ? " " : "",
                      suffix);
}

static unsigned int
sixel_kmedoids_clamp_uint(unsigned int value,
                          unsigned int minimum,
                          unsigned int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static unsigned int
sixel_kmedoids_parse_env_uint(char const *env_name,
                              unsigned int fallback,
                              unsigned int minimum,
                              unsigned int maximum,
                              int allow_zero)
{
    char const *env_value;
    char *endptr;
    unsigned long parsed;

    env_value = NULL;
    endptr = NULL;
    parsed = 0ul;
    if (env_name == NULL || env_name[0] == '\0') {
        return fallback;
    }

    env_value = sixel_compat_getenv(env_name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtoul(env_value, &endptr, 10);
    if (endptr == env_value || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed > (unsigned long)UINT_MAX) {
        return fallback;
    }
    if (allow_zero && parsed == 0ul) {
        return 0u;
    }
    if (parsed == 0ul) {
        return fallback;
    }

    return sixel_kmedoids_clamp_uint((unsigned int)parsed,
                                     minimum,
                                     maximum);
}

static double
sixel_kmedoids_parse_env_double(char const *env_name,
                                double fallback,
                                double minimum,
                                double maximum)
{
    char const *env_value;
    char *endptr;
    double parsed;

    env_value = NULL;
    endptr = NULL;
    parsed = 0.0;
    if (env_name == NULL || env_name[0] == '\0') {
        return fallback;
    }

    env_value = sixel_compat_getenv(env_name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtod(env_value, &endptr);
    if (endptr == env_value || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed != parsed
            || parsed < minimum || parsed > maximum) {
        return fallback;
    }

    return parsed;
}

static int
sixel_kmedoids_compare_rank_desc(void const *lhs,
                                 void const *rhs)
{
    sixel_kmedoids_bin_rank_t const *left;
    sixel_kmedoids_bin_rank_t const *right;

    left = (sixel_kmedoids_bin_rank_t const *)lhs;
    right = (sixel_kmedoids_bin_rank_t const *)rhs;
    if (left->count < right->count) {
        return 1;
    }
    if (left->count > right->count) {
        return -1;
    }
    if (left->index > right->index) {
        return 1;
    }
    if (left->index < right->index) {
        return -1;
    }
    return 0;
}

static int
sixel_kmedoids_compare_rank_asc(void const *lhs,
                                void const *rhs)
{
    sixel_kmedoids_bin_rank_t const *left;
    sixel_kmedoids_bin_rank_t const *right;

    left = (sixel_kmedoids_bin_rank_t const *)lhs;
    right = (sixel_kmedoids_bin_rank_t const *)rhs;
    if (left->count > right->count) {
        return 1;
    }
    if (left->count < right->count) {
        return -1;
    }
    if (left->index > right->index) {
        return 1;
    }
    if (left->index < right->index) {
        return -1;
    }
    return 0;
}

static int
sixel_kmedoids_compare_bin_variance_rank_desc(void const *lhs,
                                              void const *rhs)
{
    sixel_kmedoids_bin_variance_rank_t const *left;
    sixel_kmedoids_bin_variance_rank_t const *right;

    left = (sixel_kmedoids_bin_variance_rank_t const *)lhs;
    right = (sixel_kmedoids_bin_variance_rank_t const *)rhs;
    if (left->score < right->score) {
        return 1;
    }
    if (left->score > right->score) {
        return -1;
    }
    if (left->index > right->index) {
        return 1;
    }
    if (left->index < right->index) {
        return -1;
    }
    return 0;
}

static int
sixel_kmedoids_compare_candidate_rank(void const *lhs,
                                      void const *rhs)
{
    sixel_kmedoids_candidate_rank_t const *left;
    sixel_kmedoids_candidate_rank_t const *right;

    left = (sixel_kmedoids_candidate_rank_t const *)lhs;
    right = (sixel_kmedoids_candidate_rank_t const *)rhs;
    if (left->cost < right->cost) {
        return -1;
    }
    if (left->cost > right->cost) {
        return 1;
    }
    if (left->active_index < right->active_index) {
        return -1;
    }
    if (left->active_index > right->active_index) {
        return 1;
    }
    return 0;
}

static int
sixel_kmedoids_candidate_rank_is_better(
    sixel_kmedoids_candidate_rank_t const *lhs,
    sixel_kmedoids_candidate_rank_t const *rhs)
{
    if (lhs->cost < rhs->cost) {
        return 1;
    }
    if (lhs->cost > rhs->cost) {
        return 0;
    }
    return lhs->active_index < rhs->active_index ? 1 : 0;
}

static int
sixel_kmedoids_candidate_rank_is_worse(
    sixel_kmedoids_candidate_rank_t const *lhs,
    sixel_kmedoids_candidate_rank_t const *rhs)
{
    if (lhs->cost > rhs->cost) {
        return 1;
    }
    if (lhs->cost < rhs->cost) {
        return 0;
    }
    return lhs->active_index > rhs->active_index ? 1 : 0;
}

static void
sixel_kmedoids_candidate_rank_swap(sixel_kmedoids_candidate_rank_t *ranks,
                                   unsigned int lhs,
                                   unsigned int rhs)
{
    sixel_kmedoids_candidate_rank_t temp;

    temp = ranks[lhs];
    ranks[lhs] = ranks[rhs];
    ranks[rhs] = temp;
}

static void
sixel_kmedoids_candidate_rank_heap_sift_up(
    sixel_kmedoids_candidate_rank_t *heap,
    unsigned int index)
{
    unsigned int parent;

    parent = 0u;
    if (heap == NULL) {
        return;
    }
    while (index > 0u) {
        parent = (index - 1u) / 2u;
        if (!sixel_kmedoids_candidate_rank_is_worse(&heap[index],
                                                    &heap[parent])) {
            break;
        }
        sixel_kmedoids_candidate_rank_swap(heap, index, parent);
        index = parent;
    }
}

static void
sixel_kmedoids_candidate_rank_heap_sift_down(
    sixel_kmedoids_candidate_rank_t *heap,
    unsigned int heap_size,
    unsigned int index)
{
    unsigned int left;
    unsigned int right;
    unsigned int worst;

    left = 0u;
    right = 0u;
    worst = 0u;
    if (heap == NULL) {
        return;
    }
    for (;;) {
        left = index * 2u + 1u;
        right = left + 1u;
        worst = index;
        if (left < heap_size
                && sixel_kmedoids_candidate_rank_is_worse(&heap[left],
                                                          &heap[worst])) {
            worst = left;
        }
        if (right < heap_size
                && sixel_kmedoids_candidate_rank_is_worse(&heap[right],
                                                          &heap[worst])) {
            worst = right;
        }
        if (worst == index) {
            break;
        }
        sixel_kmedoids_candidate_rank_swap(heap, index, worst);
        index = worst;
    }
}

static void
sixel_kmedoids_candidate_rank_select_topk(
    sixel_kmedoids_candidate_rank_t *ranks,
    unsigned int rank_count,
    unsigned int keep_count,
    sixel_kmedoids_candidate_rank_t *heap)
{
    sixel_kmedoids_candidate_rank_t current;
    unsigned int index;
    unsigned int heap_size;

    current.active_index = 0u;
    current.cost = 0.0;
    index = 0u;
    heap_size = 0u;
    if (ranks == NULL || heap == NULL || keep_count == 0u || rank_count == 0u) {
        return;
    }
    if (keep_count >= rank_count) {
        qsort(ranks,
              (size_t)rank_count,
              sizeof(sixel_kmedoids_candidate_rank_t),
              sixel_kmedoids_compare_candidate_rank);
        return;
    }

    for (index = 0u; index < rank_count; ++index) {
        current = ranks[index];
        if (heap_size < keep_count) {
            heap[heap_size] = current;
            sixel_kmedoids_candidate_rank_heap_sift_up(heap, heap_size);
            ++heap_size;
            continue;
        }
        if (sixel_kmedoids_candidate_rank_is_better(&current, &heap[0u])) {
            heap[0u] = current;
            sixel_kmedoids_candidate_rank_heap_sift_down(heap,
                                                         heap_size,
                                                         0u);
        }
    }

    for (index = 0u; index < keep_count; ++index) {
        ranks[index] = heap[index];
    }
    qsort(ranks,
          (size_t)keep_count,
          sizeof(sixel_kmedoids_candidate_rank_t),
          sixel_kmedoids_compare_candidate_rank);
}

static int
sixel_kmedoids_compare_point_weight_rank(void const *lhs,
                                         void const *rhs)
{
    sixel_kmedoids_point_weight_rank_t const *left;
    sixel_kmedoids_point_weight_rank_t const *right;

    left = (sixel_kmedoids_point_weight_rank_t const *)lhs;
    right = (sixel_kmedoids_point_weight_rank_t const *)rhs;
    if (left->weight < right->weight) {
        return 1;
    }
    if (left->weight > right->weight) {
        return -1;
    }
    if (left->index > right->index) {
        return 1;
    }
    if (left->index < right->index) {
        return -1;
    }
    return 0;
}

static int
sixel_kmedoids_residual_is_better(double lhs_score,
                                  unsigned int lhs_index,
                                  double rhs_score,
                                  unsigned int rhs_index)
{
    if (lhs_score > rhs_score) {
        return 1;
    }
    if (lhs_score < rhs_score) {
        return 0;
    }
    return lhs_index < rhs_index ? 1 : 0;
}

static int
sixel_kmedoids_residual_is_worse(double lhs_score,
                                 unsigned int lhs_index,
                                 double rhs_score,
                                 unsigned int rhs_index)
{
    if (lhs_score < rhs_score) {
        return 1;
    }
    if (lhs_score > rhs_score) {
        return 0;
    }
    return lhs_index > rhs_index ? 1 : 0;
}

static void
sixel_kmedoids_guided_heap_swap(sixel_kmedoids_point_weight_rank_t *rank,
                                unsigned int lhs,
                                unsigned int rhs)
{
    sixel_kmedoids_point_weight_rank_t temp;

    temp = rank[lhs];
    rank[lhs] = rank[rhs];
    rank[rhs] = temp;
}

static void
sixel_kmedoids_guided_heap_sift_up(sixel_kmedoids_point_weight_rank_t *rank,
                                   unsigned int index)
{
    unsigned int parent;

    parent = 0u;
    if (rank == NULL) {
        return;
    }
    while (index > 0u) {
        parent = (index - 1u) / 2u;
        if (!sixel_kmedoids_residual_is_worse(rank[index].weight,
                                              rank[index].index,
                                              rank[parent].weight,
                                              rank[parent].index)) {
            break;
        }
        sixel_kmedoids_guided_heap_swap(rank, index, parent);
        index = parent;
    }
}

static void
sixel_kmedoids_guided_heap_sift_down(sixel_kmedoids_point_weight_rank_t *rank,
                                     unsigned int count,
                                     unsigned int index)
{
    unsigned int left;
    unsigned int right;
    unsigned int child;

    left = 0u;
    right = 0u;
    child = 0u;
    if (rank == NULL || count == 0u) {
        return;
    }
    for (;;) {
        left = index * 2u + 1u;
        if (left >= count) {
            break;
        }
        right = left + 1u;
        child = left;
        if (right < count
                && sixel_kmedoids_residual_is_worse(rank[right].weight,
                                                    rank[right].index,
                                                    rank[left].weight,
                                                    rank[left].index)) {
            child = right;
        }
        if (!sixel_kmedoids_residual_is_worse(rank[child].weight,
                                              rank[child].index,
                                              rank[index].weight,
                                              rank[index].index)) {
            break;
        }
        sixel_kmedoids_guided_heap_swap(rank, index, child);
        index = child;
    }
}

static double
sixel_kmedoids_eval_score(double const *weights,
                          double const *nearest_dist,
                          unsigned int index)
{
    double weight;
    double distance;
    double score;

    weight = 1.0;
    distance = 0.0;
    score = 0.0;
    if (nearest_dist != NULL && isfinite(nearest_dist[index])) {
        distance = nearest_dist[index];
    }
    if (distance < 0.0) {
        distance = 0.0;
    }
    if (weights != NULL && isfinite(weights[index])) {
        weight = weights[index];
    }
    if (weight < 0.0) {
        weight = 0.0;
    }
    score = weight * distance;
    if (!isfinite(score) || score < 0.0) {
        score = 0.0;
    }
    return score;
}

static void
sixel_kmedoids_eval_order_swap(unsigned int *order,
                               unsigned int *positions,
                               unsigned int lhs_pos,
                               unsigned int rhs_pos)
{
    unsigned int lhs_index;
    unsigned int rhs_index;

    lhs_index = 0u;
    rhs_index = 0u;
    if (order == NULL || positions == NULL) {
        return;
    }
    lhs_index = order[lhs_pos];
    rhs_index = order[rhs_pos];
    order[lhs_pos] = rhs_index;
    order[rhs_pos] = lhs_index;
    positions[lhs_index] = rhs_pos;
    positions[rhs_index] = lhs_pos;
}

static void
sixel_kmedoids_eval_order_fixup(unsigned int index,
                                unsigned int point_count,
                                unsigned int *order,
                                unsigned int *positions,
                                double const *scores)
{
    unsigned int pos;
    unsigned int other_pos;
    unsigned int other_index;

    pos = 0u;
    other_pos = 0u;
    other_index = 0u;
    if (order == NULL || positions == NULL || scores == NULL
            || point_count == 0u || index >= point_count) {
        return;
    }
    pos = positions[index];
    if (pos >= point_count || order[pos] != index) {
        return;
    }
    while (pos > 0u) {
        other_pos = pos - 1u;
        other_index = order[other_pos];
        if (!sixel_kmedoids_residual_is_better(scores[index],
                                               index,
                                               scores[other_index],
                                               other_index)) {
            break;
        }
        sixel_kmedoids_eval_order_swap(order, positions, pos, other_pos);
        pos = other_pos;
    }
    while (pos + 1u < point_count) {
        other_pos = pos + 1u;
        other_index = order[other_pos];
        if (!sixel_kmedoids_residual_is_better(scores[other_index],
                                               other_index,
                                               scores[index],
                                               index)) {
            break;
        }
        sixel_kmedoids_eval_order_swap(order, positions, pos, other_pos);
        pos = other_pos;
    }
}

static void
sixel_kmedoids_eval_order_full_refresh(
    double const *weights,
    double const *nearest_dist,
    unsigned int point_count,
    unsigned int *order,
    unsigned int *positions,
    double *scores,
    sixel_kmedoids_point_weight_rank_t *rank)
{
    unsigned int index;

    index = 0u;
    if (point_count == 0u || order == NULL || positions == NULL
            || scores == NULL || rank == NULL) {
        return;
    }
    for (index = 0u; index < point_count; ++index) {
        scores[index] = sixel_kmedoids_eval_score(weights,
                                                  nearest_dist,
                                                  index);
        rank[index].index = index;
        rank[index].weight = scores[index];
    }
    qsort(rank,
          (size_t)point_count,
          sizeof(sixel_kmedoids_point_weight_rank_t),
          sixel_kmedoids_compare_point_weight_rank);
    for (index = 0u; index < point_count; ++index) {
        order[index] = rank[index].index;
        positions[rank[index].index] = index;
    }
}

static int
sixel_kmedoids_eval_order_apply_delta(double const *weights,
                                      double const *nearest_dist,
                                      unsigned int point_count,
                                      unsigned int const *changed_points,
                                      unsigned int changed_count,
                                      unsigned int delta_threshold,
                                      unsigned int *order,
                                      unsigned int *positions,
                                      double *scores,
                                      sixel_kmedoids_point_weight_rank_t *rank)
{
    unsigned int delta_index;
    unsigned int point_index;

    delta_index = 0u;
    point_index = 0u;
    if (point_count == 0u || order == NULL || positions == NULL
            || scores == NULL || rank == NULL) {
        return 1;
    }
    if (changed_points == NULL || changed_count == 0u) {
        return 0;
    }
    if (delta_threshold > 0u && changed_count > delta_threshold) {
        sixel_kmedoids_eval_order_full_refresh(weights,
                                               nearest_dist,
                                               point_count,
                                               order,
                                               positions,
                                               scores,
                                               rank);
        return 1;
    }
    for (delta_index = 0u; delta_index < changed_count; ++delta_index) {
        point_index = changed_points[delta_index];
        if (point_index >= point_count) {
            continue;
        }
        scores[point_index] = sixel_kmedoids_eval_score(weights,
                                                        nearest_dist,
                                                        point_index);
        sixel_kmedoids_eval_order_fixup(point_index,
                                        point_count,
                                        order,
                                        positions,
                                        scores);
    }
    return 0;
}

static double
sixel_kmedoids_damage_score(double const *weights,
                            double const *nearest_dist,
                            double const *second_dist,
                            unsigned int index)
{
    double weight;
    double nearest;
    double second;
    double damage;

    weight = 1.0;
    nearest = 0.0;
    second = 0.0;
    damage = 0.0;
    if (weights != NULL && isfinite(weights[index])) {
        weight = weights[index];
    }
    if (weight < 0.0) {
        weight = 0.0;
    }
    if (nearest_dist != NULL && isfinite(nearest_dist[index])) {
        nearest = nearest_dist[index];
    }
    if (second_dist != NULL && isfinite(second_dist[index])) {
        second = second_dist[index];
    }
    if (nearest < 0.0) {
        nearest = 0.0;
    }
    if (second < 0.0) {
        second = 0.0;
    }
    damage = (second - nearest) * weight;
    if (!isfinite(damage) || damage < 0.0) {
        damage = 0.0;
    }
    return damage;
}

static void
sixel_kmedoids_clarans_damage_full_refresh(double const *weights,
                                           double const *nearest_dist,
                                           double const *second_dist,
                                           unsigned int point_count,
                                           double *damage_scores)
{
    unsigned int index;

    index = 0u;
    if (damage_scores == NULL) {
        return;
    }
    for (index = 0u; index < point_count; ++index) {
        damage_scores[index] = sixel_kmedoids_damage_score(weights,
                                                           nearest_dist,
                                                           second_dist,
                                                           index);
    }
}

static void
sixel_kmedoids_clarans_slot_orders_full_refresh(
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot,
    unsigned int const *residual_order,
    double const *damage_scores,
    unsigned int *slot_orders,
    unsigned int *slot_positions,
    sixel_kmedoids_point_weight_rank_t *slot_rank_work)
{
    unsigned int slot;
    unsigned int index;
    unsigned int point_index;
    unsigned int assigned_count;
    unsigned int row_base;
    unsigned int *order_row;
    unsigned int *position_row;

    slot = 0u;
    index = 0u;
    point_index = 0u;
    assigned_count = 0u;
    row_base = 0u;
    order_row = NULL;
    position_row = NULL;
    if (point_count == 0u || k == 0u || nearest_slot == NULL
            || residual_order == NULL || damage_scores == NULL
            || slot_orders == NULL || slot_positions == NULL
            || slot_rank_work == NULL) {
        return;
    }

    /*
     * Each slot gets its own deterministic scan order:
     *   1) points currently assigned to the slot by descending damage
     *   2) all remaining points by residual order
     */
    for (slot = 0u; slot < k; ++slot) {
        assigned_count = 0u;
        for (index = 0u; index < point_count; ++index) {
            point_index = residual_order[index];
            if (nearest_slot[point_index] != slot) {
                continue;
            }
            slot_rank_work[assigned_count].index = point_index;
            slot_rank_work[assigned_count].weight = damage_scores[point_index];
            ++assigned_count;
        }
        if (assigned_count > 1u) {
            qsort(slot_rank_work,
                  (size_t)assigned_count,
                  sizeof(sixel_kmedoids_point_weight_rank_t),
                  sixel_kmedoids_compare_point_weight_rank);
        }

        row_base = slot * point_count;
        order_row = slot_orders + row_base;
        position_row = slot_positions + row_base;

        for (index = 0u; index < assigned_count; ++index) {
            order_row[index] = slot_rank_work[index].index;
        }
        for (index = 0u; index < point_count; ++index) {
            point_index = residual_order[index];
            if (nearest_slot[point_index] == slot) {
                continue;
            }
            order_row[assigned_count] = point_index;
            ++assigned_count;
        }
        for (index = 0u; index < point_count; ++index) {
            position_row[order_row[index]] = index;
        }
    }
}

static void
sixel_kmedoids_clarans_slot_generation_next(
    uint32_t *slot_generation_id_io,
    uint32_t *slot_generation,
    unsigned int k)
{
    uint32_t next_id;

    next_id = 0u;
    if (slot_generation_id_io == NULL || slot_generation == NULL || k == 0u) {
        return;
    }
    next_id = *slot_generation_id_io + 1u;
    if (next_id == 0u) {
        memset(slot_generation, 0, (size_t)k * sizeof(uint32_t));
        next_id = 1u;
    }
    *slot_generation_id_io = next_id;
}

static void
sixel_kmedoids_clarans_mark_all_slots_dirty(unsigned char *slot_dirty,
                                            unsigned int k)
{
    unsigned int slot;

    slot = 0u;
    if (slot_dirty == NULL || k == 0u) {
        return;
    }
    for (slot = 0u; slot < k; ++slot) {
        slot_dirty[slot] = 1u;
    }
}

static unsigned int *
sixel_kmedoids_clarans_get_slot_order_row(
    unsigned int point_count,
    unsigned int k,
    unsigned int slot,
    unsigned int const *nearest_slot,
    unsigned int const *residual_order,
    double const *damage_scores,
    unsigned int *slot_orders,
    unsigned char *slot_dirty,
    uint32_t *slot_generation,
    uint32_t slot_generation_id,
    sixel_kmedoids_point_weight_rank_t *slot_rank_work)
{
    unsigned int index;
    unsigned int point_index;
    unsigned int assigned_count;
    unsigned int row_base;
    unsigned int *order_row;

    index = 0u;
    point_index = 0u;
    assigned_count = 0u;
    row_base = 0u;
    order_row = NULL;
    if (point_count == 0u || k == 0u || slot >= k
            || nearest_slot == NULL || residual_order == NULL
            || damage_scores == NULL || slot_orders == NULL
            || slot_dirty == NULL || slot_generation == NULL
            || slot_rank_work == NULL || slot_generation_id == 0u) {
        return NULL;
    }

    row_base = slot * point_count;
    order_row = slot_orders + row_base;
    if (slot_dirty[slot] == 0u
            && slot_generation[slot] == slot_generation_id) {
        return order_row;
    }

    assigned_count = 0u;
    for (index = 0u; index < point_count; ++index) {
        point_index = residual_order[index];
        if (nearest_slot[point_index] != slot) {
            continue;
        }
        slot_rank_work[assigned_count].index = point_index;
        slot_rank_work[assigned_count].weight = damage_scores[point_index];
        ++assigned_count;
    }
    if (assigned_count > 1u) {
        qsort(slot_rank_work,
              (size_t)assigned_count,
              sizeof(sixel_kmedoids_point_weight_rank_t),
              sixel_kmedoids_compare_point_weight_rank);
    }

    for (index = 0u; index < assigned_count; ++index) {
        order_row[index] = slot_rank_work[index].index;
    }
    for (index = 0u; index < point_count; ++index) {
        point_index = residual_order[index];
        if (nearest_slot[point_index] == slot) {
            continue;
        }
        order_row[assigned_count] = point_index;
        ++assigned_count;
    }

    slot_dirty[slot] = 0u;
    slot_generation[slot] = slot_generation_id;
    return order_row;
}

static int
sixel_kmedoids_pair_seen_or_insert(uint64_t pair_key,
                                   uint64_t *seen_pairs,
                                   uint32_t *seen_generation,
                                   uint32_t seen_generation_id,
                                   unsigned int pair_capacity,
                                   unsigned int pair_mask)
{
    unsigned int pair_slot;
    unsigned int probe_count;
    uint64_t pair_state;

    pair_slot = 0u;
    probe_count = 0u;
    pair_state = 0u;
    if (seen_pairs == NULL || seen_generation == NULL
            || pair_capacity == 0u) {
        return 0;
    }
    pair_state = pair_key * 11400714819323198485ULL;
    pair_slot = (unsigned int)(pair_state & (uint64_t)pair_mask);
    for (;;) {
        if (seen_generation[pair_slot] != seen_generation_id) {
            seen_generation[pair_slot] = seen_generation_id;
            seen_pairs[pair_slot] = pair_key;
            return 0;
        }
        if (seen_pairs[pair_slot] == pair_key) {
            return 1;
        }
        ++probe_count;
        if (probe_count >= pair_capacity) {
            return 1;
        }
        pair_slot = (pair_slot + 1u) & pair_mask;
    }
}

static unsigned int
sixel_kmedoids_clarans_slot_probe_target(unsigned int k,
                                         unsigned int hot_slot_count,
                                         unsigned int neighbor_count,
                                         unsigned int stale_limit,
                                         unsigned int eval_total,
                                         unsigned int eval_budget)
{
    uint64_t lhs;
    uint64_t rhs;
    unsigned int target;

    lhs = 0u;
    rhs = 0u;
    target = 0u;
    if (k == 0u) {
        return 0u;
    }
    target = hot_slot_count + 1u;
    if (target < 2u) {
        target = 2u;
    }
    if (target > k) {
        target = k;
    }
    if (k <= 2u) {
        return k;
    }

    if (stale_limit > 0u) {
        lhs = (uint64_t)neighbor_count * 4u;
        rhs = (uint64_t)stale_limit * 3u;
        if (lhs >= rhs) {
            target = (target + 1u) / 2u;
        } else {
            lhs = (uint64_t)neighbor_count * 2u;
            rhs = (uint64_t)stale_limit;
            if (lhs >= rhs) {
                target = (target * 3u + 3u) / 4u;
            }
        }
    }
    if (eval_budget > 0u) {
        lhs = (uint64_t)eval_total * 4u;
        rhs = (uint64_t)eval_budget * 3u;
        if (lhs >= rhs) {
            target = (target + 1u) / 2u;
        } else {
            lhs = (uint64_t)eval_total * 2u;
            rhs = (uint64_t)eval_budget;
            if (lhs >= rhs) {
                target = (target * 3u + 3u) / 4u;
            }
        }
    }
    if (neighbor_count == 0u
            && (eval_budget == 0u || eval_total < eval_budget / 2u)
            && target < k) {
        ++target;
    }
    if (target < 2u) {
        target = 2u;
    }
    if (target > k) {
        target = k;
    }
    return target;
}

static unsigned int
sixel_kmedoids_clarans_candidate_cache_size(unsigned int point_count,
                                            unsigned int k)
{
    unsigned int cache_size;

    cache_size = 8u;
    if (point_count >= 4096u || k >= 24u) {
        cache_size = 16u;
    }
    if (point_count >= 16384u || k >= 64u) {
        cache_size = 32u;
    }
    return cache_size;
}

static unsigned int
sixel_kmedoids_clarans_cheap_prefix_count(unsigned int point_count)
{
    unsigned int prefix_count;

    prefix_count = point_count / 8u;
    if (prefix_count < 24u) {
        prefix_count = 24u;
    }
    if (prefix_count > 192u) {
        prefix_count = 192u;
    }
    if (prefix_count > point_count) {
        prefix_count = point_count;
    }
    return prefix_count;
}

static int
sixel_kmedoids_clarans_evaluate_candidate_slots(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int candidate,
    unsigned int const *slots,
    unsigned int slot_count,
    unsigned int k,
    unsigned int const *residual_order,
    double const *damage_scores,
    unsigned int *slot_orders,
    unsigned char *slot_dirty,
    uint32_t *slot_generation,
    uint32_t slot_generation_id,
    sixel_kmedoids_point_weight_rank_t *slot_rank_work,
    uint64_t *seen_pairs,
    uint32_t *seen_generation,
    uint32_t seen_generation_id,
    unsigned int pair_capacity,
    unsigned int pair_mask,
    unsigned int cache_size,
    unsigned int *cache_keys,
    double *cache_rows,
    uint32_t *cache_generation,
    uint32_t *cache_row_epoch,
    uint32_t *cache_epoch_next,
    unsigned int *cache_slot_next,
    double cutoff,
    unsigned int cheap_prefix_count,
    int enable_cheap_bound,
    unsigned int *best_slot_out,
    double *best_cost_out,
    unsigned int *evaluated_pairs_out)
{
    unsigned int probe_index;
    unsigned int slot;
    unsigned int const *order_row;
    unsigned int best_slot;
    unsigned int evaluated_pairs;
    uint64_t pair_key;
    double best_cost;
    double swap_cost;
    double *candidate_dist_row;
    uint32_t *candidate_generation_row;
    uint32_t candidate_row_epoch;
    unsigned int bound_prefix_count;
    double bound_cost;
    int bound_stop;
    int run_strict;
    int use_lazy_slot_order;
    int improved;

    probe_index = 0u;
    slot = 0u;
    order_row = NULL;
    best_slot = UINT_MAX;
    evaluated_pairs = 0u;
    pair_key = 0u;
    best_cost = cutoff;
    swap_cost = 0.0;
    candidate_dist_row = NULL;
    candidate_generation_row = NULL;
    candidate_row_epoch = 0u;
    bound_prefix_count = 0u;
    bound_cost = 0.0;
    bound_stop = 0;
    run_strict = 1;
    use_lazy_slot_order = 0;
    improved = 0;
    if (best_slot_out != NULL) {
        *best_slot_out = UINT_MAX;
    }
    if (best_cost_out != NULL) {
        *best_cost_out = cutoff;
    }
    if (evaluated_pairs_out != NULL) {
        *evaluated_pairs_out = 0u;
    }
    if (points == NULL || point_count == 0u || nearest_slot == NULL
            || nearest_dist == NULL || second_dist == NULL
            || slots == NULL || slot_count == 0u || k == 0u
            || slot_orders == NULL) {
        return 0;
    }
    if (residual_order != NULL && damage_scores != NULL
            && slot_dirty != NULL && slot_generation != NULL
            && slot_generation_id != 0u
            && slot_rank_work != NULL) {
        use_lazy_slot_order = 1;
    }

    /*
     * Candidate-centric evaluation reuses one cached distance row across
     * multiple slot probes while preserving per-pair dedupe semantics.
     */
    candidate_dist_row = sixel_kmedoids_clarans_get_candidate_distance_row(
        point_count,
        candidate,
        cache_size,
        cache_keys,
        cache_rows,
        cache_generation,
        cache_row_epoch,
        cache_epoch_next,
        cache_slot_next,
        &candidate_generation_row,
        &candidate_row_epoch);
    if (candidate_dist_row == NULL) {
        return 0;
    }
    for (probe_index = 0u; probe_index < slot_count; ++probe_index) {
        slot = slots[probe_index];
        if (slot >= k) {
            continue;
        }
        pair_key = ((uint64_t)slot << 32u) | (uint64_t)candidate;
        if (sixel_kmedoids_pair_seen_or_insert(pair_key,
                                               seen_pairs,
                                               seen_generation,
                                               seen_generation_id,
                                               pair_capacity,
                                               pair_mask)) {
            continue;
        }

        if (use_lazy_slot_order) {
            order_row = sixel_kmedoids_clarans_get_slot_order_row(
                point_count,
                k,
                slot,
                nearest_slot,
                residual_order,
                damage_scores,
                slot_orders,
                slot_dirty,
                slot_generation,
                slot_generation_id,
                slot_rank_work);
            if (order_row == NULL) {
                continue;
            }
        } else {
            order_row = slot_orders + slot * point_count;
        }
        ++evaluated_pairs;

        run_strict = 1;
        if (enable_cheap_bound && cheap_prefix_count > 0u
                && cheap_prefix_count < point_count) {
            bound_prefix_count = cheap_prefix_count;
            if (bound_prefix_count > point_count) {
                bound_prefix_count = point_count;
            }
            bound_stop = 0;
            bound_cost =
                sixel_kmedoids_swap_cost_prefix_with_cutoff_row_generation(
                    points,
                    weights,
                    point_count,
                    bound_prefix_count,
                    nearest_slot,
                    nearest_dist,
                    second_dist,
                    slot,
                    candidate,
                    candidate_dist_row,
                    candidate_generation_row,
                    candidate_row_epoch,
                    order_row,
                    best_cost,
                    &bound_stop);
            if (bound_stop) {
                run_strict = 0;
                swap_cost = bound_cost;
            }
        }
        if (!run_strict) {
            continue;
        }

        swap_cost = sixel_kmedoids_swap_cost_with_cutoff_row_generation(
            points,
            weights,
            point_count,
            nearest_slot,
            nearest_dist,
            second_dist,
            slot,
            candidate,
            candidate_dist_row,
            candidate_generation_row,
            candidate_row_epoch,
            order_row,
            best_cost,
            NULL);
        if (swap_cost + 1.0e-12 < best_cost) {
            best_cost = swap_cost;
            best_slot = slot;
            improved = 1;
        }
    }

    if (best_slot_out != NULL) {
        *best_slot_out = best_slot;
    }
    if (best_cost_out != NULL) {
        *best_cost_out = best_cost;
    }
    if (evaluated_pairs_out != NULL) {
        *evaluated_pairs_out = evaluated_pairs;
    }
    return improved;
}

static void
sixel_kmedoids_build_clarans_guided_sets(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    unsigned char const *flags,
    unsigned int const *non_medoids,
    unsigned int non_count,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    sixel_kmedoids_point_weight_rank_t *point_rank,
    sixel_kmedoids_point_weight_rank_t *slot_rank,
    double *slot_error,
    unsigned int *hot_points,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots,
    unsigned int *hot_slot_count_out)
{
    unsigned int index;
    unsigned int slot;
    unsigned int point_limit;
    unsigned int slot_limit;
    unsigned int count;
    unsigned int slot_count;
    unsigned int insert_position;
    double weight;
    double residual;

    index = 0u;
    slot = 0u;
    point_limit = 0u;
    slot_limit = 0u;
    count = 0u;
    slot_count = 0u;
    insert_position = 0u;
    weight = 1.0;
    residual = 0.0;
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = 0u;
    }
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = 0u;
    }
    if (nearest_slot == NULL || nearest_dist == NULL
            || point_rank == NULL || slot_rank == NULL
            || slot_error == NULL || hot_points == NULL
            || hot_slots == NULL || point_count == 0u || k == 0u) {
        return;
    }

    for (slot = 0u; slot < k; ++slot) {
        slot_error[slot] = 0.0;
    }

    point_limit = hot_point_limit;
    if (point_limit > point_count) {
        point_limit = point_count;
    }
    slot_limit = hot_slot_limit;
    if (slot_limit > k) {
        slot_limit = k;
    }

    /*
     * Keep only the top-K residual contributors via a fixed-size min-heap.
     * This avoids full O(N log N) sorting while preserving deterministic
     * tie-break order: higher residual first, then lower index.
     */
    count = 0u;
    for (index = 0u; index < point_count; ++index) {
        weight = 1.0;
        if (weights != NULL && isfinite(weights[index])) {
            weight = weights[index];
        }
        if (weight < 0.0) {
            weight = 0.0;
        }
        residual = 0.0;
        if (nearest_slot[index] < k) {
            residual = nearest_dist[index] * weight;
            if (!isfinite(residual) || residual < 0.0) {
                residual = 0.0;
            }
            slot_error[nearest_slot[index]] += residual;
        }
        if (flags != NULL && flags[index] != 0u) {
            continue;
        }
        if (point_limit == 0u) {
            continue;
        }
        if (count < point_limit) {
            point_rank[count].index = index;
            point_rank[count].weight = residual;
            sixel_kmedoids_guided_heap_sift_up(point_rank, count);
            ++count;
            continue;
        }
        if (sixel_kmedoids_residual_is_better(residual,
                                              index,
                                              point_rank[0u].weight,
                                              point_rank[0u].index)) {
            point_rank[0u].index = index;
            point_rank[0u].weight = residual;
            sixel_kmedoids_guided_heap_sift_down(point_rank, count, 0u);
        }
    }

    if (count == 0u && non_medoids != NULL && non_count > 0u) {
        for (index = 0u; index < non_count && count < point_limit; ++index) {
            hot_points[count] = non_medoids[index];
            ++count;
        }
    } else if (count > 0u) {
        qsort(point_rank,
              (size_t)count,
              sizeof(sixel_kmedoids_point_weight_rank_t),
              sixel_kmedoids_compare_point_weight_rank);
        for (index = 0u; index < count; ++index) {
            hot_points[index] = point_rank[index].index;
        }
    }
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = count;
    }

    slot_count = 0u;
    for (slot = 0u; slot < k; ++slot) {
        residual = slot_error[slot];
        if (slot_count < slot_limit) {
            insert_position = slot_count;
            ++slot_count;
        } else {
            if (slot_limit == 0u
                    || !sixel_kmedoids_residual_is_better(
                        residual,
                        slot,
                        slot_rank[slot_limit - 1u].weight,
                        slot_rank[slot_limit - 1u].index)) {
                continue;
            }
            insert_position = slot_limit - 1u;
        }
        while (insert_position > 0u
                && sixel_kmedoids_residual_is_better(
                    residual,
                    slot,
                    slot_rank[insert_position - 1u].weight,
                    slot_rank[insert_position - 1u].index)) {
            slot_rank[insert_position] = slot_rank[insert_position - 1u];
            --insert_position;
        }
        slot_rank[insert_position].index = slot;
        slot_rank[insert_position].weight = residual;
    }
    for (index = 0u; index < slot_count; ++index) {
        hot_slots[index] = slot_rank[index].index;
    }
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = slot_count;
    }
}

static double
sixel_kmedoids_weighted_residual(double const *weights,
                                 unsigned int point_index,
                                 double distance)
{
    double weight;
    double residual;

    weight = 1.0;
    residual = 0.0;
    if (weights != NULL && isfinite(weights[point_index])) {
        weight = weights[point_index];
    }
    if (weight < 0.0) {
        weight = 0.0;
    }
    residual = distance * weight;
    if (!isfinite(residual) || residual < 0.0) {
        residual = 0.0;
    }
    return residual;
}

static void
sixel_kmedoids_clarans_guided_heap_swap(
    sixel_kmedoids_point_weight_rank_t *rank,
    unsigned int *heap_pos,
    unsigned int lhs,
    unsigned int rhs)
{
    sixel_kmedoids_point_weight_rank_t temp;

    temp = rank[lhs];
    rank[lhs] = rank[rhs];
    rank[rhs] = temp;
    heap_pos[rank[lhs].index] = lhs;
    heap_pos[rank[rhs].index] = rhs;
}

static void
sixel_kmedoids_clarans_guided_heap_sift_up(
    sixel_kmedoids_point_weight_rank_t *rank,
    unsigned int *heap_pos,
    unsigned int index)
{
    unsigned int parent;

    parent = 0u;
    while (index > 0u) {
        parent = (index - 1u) / 2u;
        if (!sixel_kmedoids_residual_is_worse(rank[index].weight,
                                              rank[index].index,
                                              rank[parent].weight,
                                              rank[parent].index)) {
            break;
        }
        sixel_kmedoids_clarans_guided_heap_swap(rank,
                                                heap_pos,
                                                index,
                                                parent);
        index = parent;
    }
}

static void
sixel_kmedoids_clarans_guided_heap_sift_down(
    sixel_kmedoids_point_weight_rank_t *rank,
    unsigned int *heap_pos,
    unsigned int count,
    unsigned int index)
{
    unsigned int left;
    unsigned int right;
    unsigned int child;

    left = 0u;
    right = 0u;
    child = 0u;
    while (count > 0u) {
        left = index * 2u + 1u;
        if (left >= count) {
            break;
        }
        right = left + 1u;
        child = left;
        if (right < count
                && sixel_kmedoids_residual_is_worse(rank[right].weight,
                                                    rank[right].index,
                                                    rank[left].weight,
                                                    rank[left].index)) {
            child = right;
        }
        if (!sixel_kmedoids_residual_is_worse(rank[child].weight,
                                              rank[child].index,
                                              rank[index].weight,
                                              rank[index].index)) {
            break;
        }
        sixel_kmedoids_clarans_guided_heap_swap(rank,
                                                heap_pos,
                                                index,
                                                child);
        index = child;
    }
}

static void
sixel_kmedoids_clarans_guided_rebuild_hot_points(
    sixel_kmedoids_point_weight_rank_t const *heap_rank,
    unsigned int heap_count,
    sixel_kmedoids_point_weight_rank_t *sorted_rank,
    unsigned int *hot_points,
    unsigned int *hot_point_count_out)
{
    unsigned int index;

    index = 0u;
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = 0u;
    }
    if (heap_rank == NULL || sorted_rank == NULL || hot_points == NULL) {
        return;
    }
    for (index = 0u; index < heap_count; ++index) {
        sorted_rank[index] = heap_rank[index];
    }
    qsort(sorted_rank,
          (size_t)heap_count,
          sizeof(sixel_kmedoids_point_weight_rank_t),
          sixel_kmedoids_compare_point_weight_rank);
    for (index = 0u; index < heap_count; ++index) {
        hot_points[index] = sorted_rank[index].index;
    }
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = heap_count;
    }
}

static void
sixel_kmedoids_clarans_guided_rebuild_hot_slots(
    double const *slot_error,
    unsigned int k,
    unsigned int hot_slot_limit,
    sixel_kmedoids_point_weight_rank_t *slot_rank,
    unsigned int *hot_slots,
    unsigned int *hot_slot_count_out)
{
    unsigned int slot;
    unsigned int slot_limit;
    unsigned int slot_count;
    unsigned int insert_position;
    unsigned int index;
    double residual;

    slot = 0u;
    slot_limit = 0u;
    slot_count = 0u;
    insert_position = 0u;
    index = 0u;
    residual = 0.0;
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = 0u;
    }
    if (slot_error == NULL || slot_rank == NULL || hot_slots == NULL
            || k == 0u) {
        return;
    }
    slot_limit = hot_slot_limit;
    if (slot_limit > k) {
        slot_limit = k;
    }
    for (slot = 0u; slot < k; ++slot) {
        residual = slot_error[slot];
        if (slot_count < slot_limit) {
            insert_position = slot_count;
            ++slot_count;
        } else {
            if (slot_limit == 0u
                    || !sixel_kmedoids_residual_is_better(
                        residual,
                        slot,
                        slot_rank[slot_limit - 1u].weight,
                        slot_rank[slot_limit - 1u].index)) {
                continue;
            }
            insert_position = slot_limit - 1u;
        }
        while (insert_position > 0u
                && sixel_kmedoids_residual_is_better(
                    residual,
                    slot,
                    slot_rank[insert_position - 1u].weight,
                    slot_rank[insert_position - 1u].index)) {
            slot_rank[insert_position] = slot_rank[insert_position - 1u];
            --insert_position;
        }
        slot_rank[insert_position].index = slot;
        slot_rank[insert_position].weight = residual;
    }
    for (index = 0u; index < slot_count; ++index) {
        hot_slots[index] = slot_rank[index].index;
    }
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = slot_count;
    }
}

static void
sixel_kmedoids_clarans_guided_heap_update_point(
    sixel_kmedoids_point_weight_rank_t *heap_rank,
    unsigned int *heap_count_io,
    unsigned int heap_limit,
    unsigned int *heap_pos,
    unsigned char const *flags,
    unsigned int point_index,
    double residual)
{
    unsigned int heap_count;
    unsigned int position;
    unsigned int last;
    unsigned int removed_index;

    heap_count = 0u;
    position = 0u;
    last = 0u;
    removed_index = 0u;
    if (heap_rank == NULL || heap_count_io == NULL || heap_pos == NULL
            || point_index == UINT_MAX) {
        return;
    }
    heap_count = *heap_count_io;
    position = heap_pos[point_index];

    if (flags != NULL && flags[point_index] != 0u) {
        if (position == UINT_MAX || heap_count == 0u) {
            return;
        }
        last = heap_count - 1u;
        removed_index = heap_rank[position].index;
        heap_pos[removed_index] = UINT_MAX;
        if (position != last) {
            heap_rank[position] = heap_rank[last];
            heap_pos[heap_rank[position].index] = position;
        }
        heap_count = last;
        if (heap_count > 0u && position < heap_count) {
            sixel_kmedoids_clarans_guided_heap_sift_up(heap_rank,
                                                       heap_pos,
                                                       position);
            sixel_kmedoids_clarans_guided_heap_sift_down(heap_rank,
                                                         heap_pos,
                                                         heap_count,
                                                         position);
        }
        *heap_count_io = heap_count;
        return;
    }

    if (position != UINT_MAX && position < heap_count) {
        heap_rank[position].weight = residual;
        sixel_kmedoids_clarans_guided_heap_sift_up(heap_rank,
                                                   heap_pos,
                                                   position);
        sixel_kmedoids_clarans_guided_heap_sift_down(heap_rank,
                                                     heap_pos,
                                                     heap_count,
                                                     position);
        return;
    }

    if (heap_limit == 0u) {
        return;
    }
    if (heap_count < heap_limit) {
        heap_rank[heap_count].index = point_index;
        heap_rank[heap_count].weight = residual;
        heap_pos[point_index] = heap_count;
        sixel_kmedoids_clarans_guided_heap_sift_up(heap_rank,
                                                   heap_pos,
                                                   heap_count);
        *heap_count_io = heap_count + 1u;
        return;
    }

    if (!sixel_kmedoids_residual_is_better(residual,
                                           point_index,
                                           heap_rank[0u].weight,
                                           heap_rank[0u].index)) {
        return;
    }
    removed_index = heap_rank[0u].index;
    heap_pos[removed_index] = UINT_MAX;
    heap_rank[0u].index = point_index;
    heap_rank[0u].weight = residual;
    heap_pos[point_index] = 0u;
    sixel_kmedoids_clarans_guided_heap_sift_down(heap_rank,
                                                 heap_pos,
                                                 heap_count,
                                                 0u);
}

static void
sixel_kmedoids_clarans_guided_full_refresh(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    unsigned char const *flags,
    unsigned int const *non_medoids,
    unsigned int non_count,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    sixel_kmedoids_point_weight_rank_t *heap_rank,
    sixel_kmedoids_point_weight_rank_t *sorted_rank,
    sixel_kmedoids_point_weight_rank_t *slot_rank,
    double *slot_error,
    double *residual,
    unsigned int *heap_pos,
    unsigned int *hot_points,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots,
    unsigned int *hot_slot_count_out)
{
    unsigned int index;
    unsigned int heap_count;
    unsigned int point_index;
    unsigned int point_limit;

    index = 0u;
    heap_count = 0u;
    point_index = 0u;
    point_limit = 0u;
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = 0u;
    }
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = 0u;
    }
    if (point_count == 0u || k == 0u || nearest_slot == NULL
            || nearest_dist == NULL || heap_rank == NULL
            || sorted_rank == NULL || slot_rank == NULL
            || slot_error == NULL || residual == NULL
            || heap_pos == NULL || hot_points == NULL
            || hot_slots == NULL) {
        return;
    }

    sixel_kmedoids_build_clarans_guided_sets(weights,
                                             point_count,
                                             k,
                                             nearest_slot,
                                             nearest_dist,
                                             flags,
                                             non_medoids,
                                             non_count,
                                             hot_point_limit,
                                             hot_slot_limit,
                                             heap_rank,
                                             slot_rank,
                                             slot_error,
                                             hot_points,
                                             hot_point_count_out,
                                             hot_slots,
                                             hot_slot_count_out);
    sixel_kmedoids_clarans_guided_full_build_count += 1u;

    for (index = 0u; index < point_count; ++index) {
        residual[index] = sixel_kmedoids_weighted_residual(weights,
                                                           index,
                                                           nearest_dist[index]);
        heap_pos[index] = UINT_MAX;
    }

    point_limit = hot_point_limit;
    if (point_limit > point_count) {
        point_limit = point_count;
    }
    heap_count = hot_point_count_out != NULL ? *hot_point_count_out : 0u;
    if (heap_count > point_limit) {
        heap_count = point_limit;
    }
    for (index = 0u; index < heap_count; ++index) {
        point_index = hot_points[index];
        if (point_index >= point_count) {
            continue;
        }
        if (flags != NULL && flags[point_index] != 0u) {
            continue;
        }
        heap_rank[index].index = point_index;
        heap_rank[index].weight = residual[point_index];
        heap_pos[point_index] = index;
    }
    if (heap_count > 1u) {
        for (index = heap_count / 2u; index > 0u; --index) {
            sixel_kmedoids_clarans_guided_heap_sift_down(heap_rank,
                                                         heap_pos,
                                                         heap_count,
                                                         index - 1u);
        }
    }

    sixel_kmedoids_clarans_guided_rebuild_hot_points(heap_rank,
                                                     heap_count,
                                                     sorted_rank,
                                                     hot_points,
                                                     hot_point_count_out);
    sixel_kmedoids_clarans_guided_rebuild_hot_slots(slot_error,
                                                    k,
                                                    hot_slot_limit,
                                                    slot_rank,
                                                    hot_slots,
                                                    hot_slot_count_out);
}

static int
sixel_kmedoids_clarans_guided_apply_delta(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    unsigned char const *flags,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int old_medoid,
    unsigned int new_medoid,
    double const *slot_delta,
    sixel_kmedoids_point_weight_rank_t *heap_rank,
    sixel_kmedoids_point_weight_rank_t *sorted_rank,
    unsigned int *heap_pos,
    sixel_kmedoids_point_weight_rank_t *slot_rank,
    double *slot_error,
    double *residual,
    unsigned int *hot_points,
    unsigned int *hot_point_count_io,
    unsigned int *hot_slots,
    unsigned int *hot_slot_count_out)
{
    unsigned int index;
    unsigned int point_index;
    unsigned int hot_point_count;

    index = 0u;
    point_index = 0u;
    hot_point_count = 0u;
    if (heap_rank == NULL || sorted_rank == NULL || heap_pos == NULL
            || slot_rank == NULL || slot_error == NULL || residual == NULL
            || hot_points == NULL || hot_point_count_io == NULL
            || hot_slots == NULL || hot_slot_count_out == NULL
            || nearest_dist == NULL || point_count == 0u || k == 0u) {
        return 1;
    }
    (void)nearest_slot;

    hot_point_count = *hot_point_count_io;
    if (hot_point_count > hot_point_limit) {
        return 1;
    }
    if (slot_delta != NULL) {
        for (index = 0u; index < k; ++index) {
            slot_error[index] += slot_delta[index];
            if (!isfinite(slot_error[index]) || slot_error[index] < 0.0) {
                slot_error[index] = 0.0;
            }
        }
    }

    for (index = 0u; index < changed_count; ++index) {
        point_index = changed_points[index];
        if (point_index >= point_count) {
            return 1;
        }
        residual[point_index] = sixel_kmedoids_weighted_residual(
            weights,
            point_index,
            nearest_dist[point_index]);
        sixel_kmedoids_clarans_guided_heap_update_point(
            heap_rank,
            &hot_point_count,
            hot_point_limit,
            heap_pos,
            flags,
            point_index,
            residual[point_index]);
    }
    if (old_medoid < point_count) {
        residual[old_medoid] = sixel_kmedoids_weighted_residual(
            weights,
            old_medoid,
            nearest_dist[old_medoid]);
        sixel_kmedoids_clarans_guided_heap_update_point(heap_rank,
                                                        &hot_point_count,
                                                        hot_point_limit,
                                                        heap_pos,
                                                        flags,
                                                        old_medoid,
                                                        residual[old_medoid]);
    }
    if (new_medoid < point_count) {
        residual[new_medoid] = sixel_kmedoids_weighted_residual(
            weights,
            new_medoid,
            nearest_dist[new_medoid]);
        sixel_kmedoids_clarans_guided_heap_update_point(heap_rank,
                                                        &hot_point_count,
                                                        hot_point_limit,
                                                        heap_pos,
                                                        flags,
                                                        new_medoid,
                                                        residual[new_medoid]);
    }

    sixel_kmedoids_clarans_guided_rebuild_hot_points(heap_rank,
                                                     hot_point_count,
                                                     sorted_rank,
                                                     hot_points,
                                                     hot_point_count_io);
    sixel_kmedoids_clarans_guided_rebuild_hot_slots(slot_error,
                                                    k,
                                                    hot_slot_limit,
                                                    slot_rank,
                                                    hot_slots,
                                                    hot_slot_count_out);
    if (*hot_point_count_io > hot_point_limit
            || *hot_slot_count_out > hot_slot_limit) {
        return 1;
    }
    for (index = 0u; index < *hot_point_count_io; ++index) {
        if (hot_points[index] >= point_count) {
            return 1;
        }
        if (flags != NULL && flags[hot_points[index]] != 0u) {
            return 1;
        }
    }
    return 0;
}

SIXEL_INTERNAL_API void
sixel_kmedoids_test_reset_clarans_guided_full_build_count(void)
{
    sixel_kmedoids_clarans_guided_full_build_count = 0u;
}

SIXEL_INTERNAL_API unsigned int
sixel_kmedoids_test_get_clarans_guided_full_build_count(void)
{
    return sixel_kmedoids_clarans_guided_full_build_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_kmedoids_test_clarans_cache_size(unsigned int point_count,
                                       unsigned int k)
{
    return sixel_kmedoids_clarans_candidate_cache_size(point_count, k);
}

SIXEL_INTERNAL_API unsigned int
sixel_kmedoids_test_clarans_cheap_prefix_count(unsigned int point_count)
{
    return sixel_kmedoids_clarans_cheap_prefix_count(point_count);
}

static void
sixel_kmedoids_set_distance_cache(double const *cache,
                                  unsigned int point_count)
{
    sixel_kmedoids_distance_cache = cache;
    sixel_kmedoids_distance_cache_size = point_count;
}

static double
sixel_kmedoids_rng_unit(uint32_t *state)
{
    uint32_t raw;

    raw = sixel_kmedoids_rng_next(state);
    return ((double)raw) / 4294967295.0;
}

static const char *
sixel_kmedoids_algo_to_string(sixel_kmedoids_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO:
        return "auto";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA:
        return "sample";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS:
        return "random";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM:
        return "bandit";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_PAM:
    default:
        return "pam";
    }
}

static sixel_kmedoids_algo_t
sixel_kmedoids_resolve_algo(sixel_kmedoids_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_PAM:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM:
        return algo;
    default:
        return SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
    }
}

void
sixel_set_kmedoids_algo_override(int enabled,
                                 sixel_kmedoids_algo_t algo)
{
    sixel_kmedoids_algo_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_algo_override_value = sixel_kmedoids_resolve_algo(algo);
}

SIXEL_INTERNAL_API sixel_kmedoids_algo_t
sixel_get_kmedoids_algo(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmedoids_algo_t cached = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;

    env_value = NULL;
    if (sixel_kmedoids_algo_override_enabled) {
        return sixel_kmedoids_algo_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEDOIDS_ALGO");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
        } else if (sixel_compat_strcasecmp(env_value, "pam") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
        } else if (sixel_compat_strcasecmp(env_value, "sample") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA;
        } else if (sixel_compat_strcasecmp(env_value, "random") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS;
        } else if (sixel_compat_strcasecmp(env_value, "bandit") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM;
        }
    }

    return cached;
}

void
sixel_set_kmedoids_seed_override(int enabled,
                                 uint32_t seed)
{
    sixel_kmedoids_seed_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_seed_override_value = seed;
}

SIXEL_INTERNAL_API uint32_t
sixel_get_kmedoids_seed(void)
{
    char const *env_value;
    char *endptr;
    unsigned long long parsed;
    static int loaded = 0;
    static uint32_t cached = 1u;

    env_value = NULL;
    endptr = NULL;
    parsed = 0u;
    if (sixel_kmedoids_seed_override_enabled) {
        return sixel_kmedoids_seed_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEDOIDS_SEED");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed = strtoull(env_value, &endptr, 10);
        if (endptr != env_value && endptr != NULL && endptr[0] == '\0'
                && errno == 0 && parsed <= 0xffffffffULL) {
            cached = (uint32_t)parsed;
        }
    }

    return cached;
}

void
sixel_set_kmedoids_iter_override(int enabled,
                                 unsigned int iter_count)
{
    sixel_kmedoids_iter_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_iter_override_value = iter_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_iter(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_iter_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_iter_override_value,
            1u,
            64u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint("SIXEL_PALETTE_KMEDOIDS_ITER",
                                           0u,
                                           1u,
                                           64u,
                                           0);
    return cached;
}

void
sixel_set_kmedoids_sample_override(int enabled,
                                   unsigned int sample_count)
{
    sixel_kmedoids_sample_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_sample_override_value = sample_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_sample(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_sample_override_enabled) {
        if (sixel_kmedoids_sample_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_sample_override_value,
            64u,
            1048576u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint("SIXEL_PALETTE_KMEDOIDS_SAMPLE",
                                           0u,
                                           64u,
                                           1048576u,
                                           1);
    return cached;
}

void
sixel_set_kmedoids_clara_trials_override(int enabled,
                                         unsigned int trials)
{
    sixel_kmedoids_clara_trials_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clara_trials_override_value = trials;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clara_trials(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clara_trials_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clara_trials_override_value,
            1u,
            32u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        0u,
        1u,
        32u,
        0);
    return cached;
}

void
sixel_set_kmedoids_clara_sample_override(int enabled,
                                         unsigned int sample_count)
{
    sixel_kmedoids_clara_sample_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clara_sample_override_value = sample_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clara_sample(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clara_sample_override_enabled) {
        if (sixel_kmedoids_clara_sample_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clara_sample_override_value,
            64u,
            1048576u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        0u,
        64u,
        1048576u,
        1);
    return cached;
}

void
sixel_set_kmedoids_clarans_local_override(int enabled,
                                          unsigned int local_searches)
{
    sixel_kmedoids_clarans_local_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clarans_local_override_value = local_searches;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clarans_local(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clarans_local_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clarans_local_override_value,
            1u,
            32u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        0u,
        1u,
        32u,
        0);
    return cached;
}

void
sixel_set_kmedoids_clarans_neighbors_override(int enabled,
                                              unsigned int neighbors)
{
    sixel_kmedoids_clarans_neighbors_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clarans_neighbors_override_value = neighbors;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clarans_neighbors(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clarans_neighbors_override_enabled) {
        if (sixel_kmedoids_clarans_neighbors_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clarans_neighbors_override_value,
            1u,
            5000000u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS",
        0u,
        1u,
        5000000u,
        1);
    return cached;
}

void
sixel_set_kmedoids_bandit_iter_override(int enabled,
                                        unsigned int iter_count)
{
    sixel_kmedoids_bandit_iter_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_bandit_iter_override_value = iter_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_iter(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_bandit_iter_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_bandit_iter_override_value,
            1u,
            64u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        0u,
        1u,
        64u,
        0);
    return cached;
}

void
sixel_set_kmedoids_bandit_candidates_override(int enabled,
                                              unsigned int candidates)
{
    sixel_kmedoids_bandit_candidates_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_bandit_candidates_override_value = candidates;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_candidates(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_bandit_candidates_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_bandit_candidates_override_value,
            8u,
            4096u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES",
        0u,
        8u,
        4096u,
        0);
    return cached;
}

void
sixel_set_kmedoids_bandit_batch_override(int enabled,
                                         unsigned int batch_size)
{
    sixel_kmedoids_bandit_batch_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_bandit_batch_override_value = batch_size;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_batch(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_bandit_batch_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_bandit_batch_override_value,
            8u,
            4096u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        0u,
        8u,
        4096u,
        0);
    return cached;
}

void
sixel_set_kmedoids_histbits_override(int enabled,
                                     unsigned int histbits)
{
    sixel_kmedoids_histbits_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_histbits_override_value = histbits;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_histbits(void)
{
    static int loaded = 0;
    static unsigned int cached = 5u;

    if (sixel_kmedoids_histbits_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_histbits_override_value,
            3u,
            6u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_HISTBITS",
        5u,
        3u,
        6u,
        0);
    return cached;
}

void
sixel_set_kmedoids_point_budget_override(int enabled,
                                         unsigned int point_budget)
{
    sixel_kmedoids_point_budget_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_point_budget_override_value = point_budget;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_point_budget(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_point_budget_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_point_budget_override_value,
            64u,
            16384u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET",
        0u,
        64u,
        16384u,
        0);
    return cached;
}

void
sixel_set_kmedoids_rare_keep_override(int enabled,
                                      unsigned int rare_keep)
{
    sixel_kmedoids_rare_keep_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_rare_keep_override_value = rare_keep;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_rare_keep(void)
{
    static int loaded = 0;
    static unsigned int cached = 64u;

    if (sixel_kmedoids_rare_keep_override_enabled) {
        if (sixel_kmedoids_rare_keep_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_rare_keep_override_value,
            0u,
            1024u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP",
        64u,
        0u,
        1024u,
        1);
    return cached;
}

void
sixel_set_kmedoids_prune_mass_override(int enabled,
                                       double prune_mass)
{
    sixel_kmedoids_prune_mass_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_prune_mass_override_value = prune_mass;
}

SIXEL_INTERNAL_API double
sixel_get_kmedoids_prune_mass(void)
{
    static int loaded = 0;
    static double cached = 0.995;

    if (sixel_kmedoids_prune_mass_override_enabled) {
        if (sixel_kmedoids_prune_mass_override_value !=
                sixel_kmedoids_prune_mass_override_value) {
            return 0.995;
        }
        if (sixel_kmedoids_prune_mass_override_value < 0.900) {
            return 0.900;
        }
        if (sixel_kmedoids_prune_mass_override_value > 1.000) {
            return 1.000;
        }
        return sixel_kmedoids_prune_mass_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_double(
        "SIXEL_PALETTE_KMEDOIDS_PRUNE_MASS",
        0.995,
        0.900,
        1.000);
    return cached;
}

/*
 * Keep this helper name k-medoids specific so amalgamation builds can include
 * palette-kmeans.c in the same translation unit without static symbol
 * collisions.
 */
static int
sixel_kmedoids_float32_alpha_visible(double alpha)
{
    if (!isfinite(alpha)) {
        return 0;
    }
    return alpha > 0.0;
}

static unsigned int
sixel_kmedoids_quality_pam_iterations(int quality_mode)
{
    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        return 4u;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        return 16u;
    case SIXEL_QUALITY_FULL:
        return 24u;
    case SIXEL_QUALITY_AUTO:
    default:
        return 8u;
    }
}

static unsigned int
sixel_kmedoids_quality_clara_trials(int quality_mode)
{
    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        return 3u;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        return 7u;
    case SIXEL_QUALITY_FULL:
        return 9u;
    case SIXEL_QUALITY_AUTO:
    default:
        return 5u;
    }
}

static unsigned int
sixel_kmedoids_quality_bandit_iterations(int quality_mode)
{
    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        return 4u;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        return 12u;
    case SIXEL_QUALITY_FULL:
        return 16u;
    case SIXEL_QUALITY_AUTO:
    default:
        return 8u;
    }
}

static unsigned int
sixel_kmedoids_sample_target(unsigned int reqcolors,
                             unsigned int pixel_count,
                             int quality_mode)
{
    unsigned int floor_target;
    unsigned int color_floor;

    floor_target = 0u;
    color_floor = 0u;
    if (reqcolors == 0u) {
        reqcolors = 1u;
    }

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        floor_target = 768u;
        break;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        floor_target = 1536u;
        break;
    case SIXEL_QUALITY_FULL:
        floor_target = 2048u;
        break;
    case SIXEL_QUALITY_AUTO:
    default:
        floor_target = 1024u;
        break;
    }

    color_floor = reqcolors * 4u;
    if (floor_target < color_floor) {
        floor_target = color_floor;
    }
    if (floor_target < reqcolors) {
        floor_target = reqcolors;
    }
    if (floor_target > pixel_count) {
        floor_target = pixel_count;
    }

    return floor_target;
}

static uint32_t
sixel_kmedoids_rng_next(uint32_t *state)
{
    uint32_t x;

    x = 0u;
    if (state == NULL) {
        return 0u;
    }

    x = *state;
    if (x == 0u) {
        x = 0x6d2b79f5u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (x == 0u) {
        x = 0x9e3779b9u;
    }
    *state = x;
    return x;
}

static unsigned int
sixel_kmedoids_rng_bounded(uint32_t *state,
                           unsigned int upper)
{
    uint32_t r;

    r = 0u;
    if (upper == 0u) {
        return 0u;
    }
    r = sixel_kmedoids_rng_next(state);
    return (unsigned int)(r % upper);
}

static uint64_t
sixel_kmedoids_double_bits(double value)
{
    union {
        double d;
        uint64_t u;
    } conv;

    conv.d = value;
    return conv.u;
}

static uint64_t
sixel_kmedoids_hash_keys(uint64_t key0,
                         uint64_t key1,
                         uint64_t key2)
{
    uint64_t h;

    h = 1469598103934665603ULL;
    h ^= key0;
    h *= 1099511628211ULL;
    h ^= key1;
    h *= 1099511628211ULL;
    h ^= key2;
    h *= 1099511628211ULL;
    return h;
}

static unsigned int
sixel_kmedoids_next_power_of_two(unsigned int value)
{
    unsigned int size;

    size = 1u;
    while (size < value && size < (1u << 30)) {
        size <<= 1u;
    }
    if (size < 8u) {
        size = 8u;
    }
    return size;
}

static void
sixel_kmedoids_seen_pairs_next_generation(uint32_t *generation_io,
                                          uint32_t *slot_generation,
                                          unsigned int slot_count)
{
    unsigned int index;

    index = 0u;
    if (generation_io == NULL || slot_generation == NULL || slot_count == 0u) {
        return;
    }

    if (*generation_io == UINT32_MAX) {
        for (index = 0u; index < slot_count; ++index) {
            slot_generation[index] = 0u;
        }
        *generation_io = 1u;
        return;
    }

    *generation_io += 1u;
    if (*generation_io == 0u) {
        *generation_io = 1u;
    }
}

static uint64_t
sixel_kmedoids_mul_u64_sat(uint64_t lhs,
                           uint64_t rhs)
{
    if (lhs == 0u || rhs == 0u) {
        return 0u;
    }
    if (lhs > UINT64_MAX / rhs) {
        return UINT64_MAX;
    }
    return lhs * rhs;
}

static SIXELSTATUS
sixel_kmedoids_collect_samples(unsigned char const *data,
                               unsigned int length,
                               unsigned int channels,
                               unsigned int pixel_stride,
                               int input_is_float32,
                               double const *float_scale,
                               double const *float_offset,
                               unsigned int histbits,
                               unsigned int point_budget,
                               unsigned int rare_keep,
                               double prune_mass,
                               uint32_t seed,
                               double **samples_out,
                               double **sample_weights_out,
                               unsigned int *sample_count_out,
                               unsigned int *visible_count_out,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int hist_levels;
    unsigned int hist_shift;
    uint64_t bin_count64;
    unsigned int bin_count;
    unsigned int *bin_hits;
    unsigned char *bin_seen;
    unsigned char *bin_selected;
    double *bin_colors;
    double *bin_sum;
    double *bin_sum_sq;
    sixel_kmedoids_bin_rank_t *rank_desc;
    sixel_kmedoids_bin_rank_t *rank_asc;
    sixel_kmedoids_bin_variance_rank_t *rank_var;
    unsigned int *selected_slot_by_bin;
    unsigned int *selected_best_index;
    double *selected_best_dist;
    double *selected_colors;
    double *samples;
    double *sample_weights;
    unsigned int pixel_count;
    unsigned int active_count;
    unsigned int index;
    unsigned int active_index;
    unsigned int visible_count;
    unsigned int bin;
    unsigned int rare_limit;
    unsigned int selected_count;
    uint64_t selected_mass;
    double required_mass_f;
    uint64_t required_mass;
    unsigned int fill_count;
    unsigned int sample_slot;
    unsigned int source;
    size_t sum_base;
    unsigned int base;
    unsigned int channel;
    unsigned int bin0;
    unsigned int bin1;
    unsigned int bin2;
    double hit_count;
    double mean;
    double var0;
    double var1;
    double var2;
    double component;
    double mapped;
    double mean0;
    double mean1;
    double mean2;
    double diff0;
    double diff1;
    double diff2;
    double distance;
    double c0;
    double c1;
    double c2;

    status = SIXEL_BAD_ARGUMENT;
    hist_levels = 0u;
    hist_shift = 0u;
    bin_count64 = 0u;
    bin_count = 0u;
    bin_hits = NULL;
    bin_seen = NULL;
    bin_selected = NULL;
    bin_colors = NULL;
    bin_sum = NULL;
    bin_sum_sq = NULL;
    rank_desc = NULL;
    rank_asc = NULL;
    rank_var = NULL;
    selected_slot_by_bin = NULL;
    selected_best_index = NULL;
    selected_best_dist = NULL;
    selected_colors = NULL;
    samples = NULL;
    sample_weights = NULL;
    pixel_count = 0u;
    active_count = 0u;
    index = 0u;
    active_index = 0u;
    visible_count = 0u;
    bin = 0u;
    rare_limit = 0u;
    selected_count = 0u;
    selected_mass = 0u;
    required_mass_f = 0.0;
    required_mass = 0u;
    fill_count = 0u;
    sample_slot = 0u;
    source = 0u;
    sum_base = 0u;
    base = 0u;
    channel = 0u;
    bin0 = 0u;
    bin1 = 0u;
    bin2 = 0u;
    hit_count = 0.0;
    mean = 0.0;
    var0 = 0.0;
    var1 = 0.0;
    var2 = 0.0;
    component = 0.0;
    mapped = 0.0;
    mean0 = 0.0;
    mean1 = 0.0;
    mean2 = 0.0;
    diff0 = 0.0;
    diff1 = 0.0;
    diff2 = 0.0;
    distance = 0.0;
    c0 = 0.0;
    c1 = 0.0;
    c2 = 0.0;

    if (samples_out != NULL) {
        *samples_out = NULL;
    }
    if (sample_weights_out != NULL) {
        *sample_weights_out = NULL;
    }
    if (sample_count_out != NULL) {
        *sample_count_out = 0u;
    }
    if (visible_count_out != NULL) {
        *visible_count_out = 0u;
    }
    if (data == NULL || allocator == NULL || samples_out == NULL
            || sample_weights_out == NULL
            || sample_count_out == NULL || visible_count_out == NULL) {
        return status;
    }
    if (channels != 3u && channels != 4u) {
        return status;
    }
    if (pixel_stride == 0u) {
        return status;
    }

    (void)seed;

    pixel_count = length / pixel_stride;
    if (pixel_count == 0u) {
        return SIXEL_OK;
    }
    if (histbits < 3u) {
        histbits = 3u;
    }
    if (histbits > 6u) {
        histbits = 6u;
    }
    if (point_budget == 0u) {
        point_budget = 1u;
    }
    if (prune_mass != prune_mass || prune_mass < 0.900) {
        prune_mass = 0.900;
    }
    if (prune_mass > 1.000) {
        prune_mass = 1.000;
    }

    hist_levels = 1u << histbits;
    hist_shift = 8u - histbits;
    bin_count64 = (uint64_t)hist_levels
                * (uint64_t)hist_levels
                * (uint64_t)hist_levels;
    if (bin_count64 > (uint64_t)UINT_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }
    bin_count = (unsigned int)bin_count64;

    bin_hits = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * sizeof(unsigned int));
    bin_seen = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * sizeof(unsigned char));
    bin_selected = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * sizeof(unsigned char));
    bin_colors = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * 3u * sizeof(double));
    bin_sum = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * 3u * sizeof(double));
    bin_sum_sq = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * 3u * sizeof(double));
    if (bin_hits == NULL || bin_seen == NULL
            || bin_selected == NULL || bin_colors == NULL
            || bin_sum == NULL || bin_sum_sq == NULL) {
        if (bin_hits != NULL) {
            sixel_allocator_free(allocator, bin_hits);
        }
        if (bin_seen != NULL) {
            sixel_allocator_free(allocator, bin_seen);
        }
        if (bin_selected != NULL) {
            sixel_allocator_free(allocator, bin_selected);
        }
        if (bin_colors != NULL) {
            sixel_allocator_free(allocator, bin_colors);
        }
        if (bin_sum != NULL) {
            sixel_allocator_free(allocator, bin_sum);
        }
        if (bin_sum_sq != NULL) {
            sixel_allocator_free(allocator, bin_sum_sq);
        }
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < bin_count; ++index) {
        bin_hits[index] = 0u;
        bin_seen[index] = 0u;
        bin_selected[index] = 0u;
        bin_sum[index * 3u + 0u] = 0.0;
        bin_sum[index * 3u + 1u] = 0.0;
        bin_sum[index * 3u + 2u] = 0.0;
        bin_sum_sq[index * 3u + 0u] = 0.0;
        bin_sum_sq[index * 3u + 1u] = 0.0;
        bin_sum_sq[index * 3u + 2u] = 0.0;
    }

    for (index = 0u; index < pixel_count; ++index) {
        base = index * pixel_stride;
        if (input_is_float32) {
            float const *fpixels;

            fpixels = (float const *)(void const *)(data + base);
            if (channels == 4u
                    && !sixel_kmedoids_float32_alpha_visible(
                           (double)fpixels[3u])) {
                continue;
            }
            c0 = (double)fpixels[0u];
            c1 = (double)fpixels[1u];
            c2 = (double)fpixels[2u];

            for (channel = 0u; channel < 3u; ++channel) {
                if (channel == 0u) {
                    component = c0;
                } else if (channel == 1u) {
                    component = c1;
                } else {
                    component = c2;
                }
                mapped = component;
                if (float_scale != NULL && float_offset != NULL
                        && float_scale[channel] > 0.0) {
                    mapped = component * float_scale[channel]
                           + float_offset[channel];
                }
                if (!isfinite(mapped) || mapped < 0.0) {
                    mapped = 0.0;
                } else if (mapped > 255.0) {
                    mapped = 255.0;
                }
                if (channel == 0u) {
                    bin0 = ((unsigned int)mapped) >> hist_shift;
                } else if (channel == 1u) {
                    bin1 = ((unsigned int)mapped) >> hist_shift;
                } else {
                    bin2 = ((unsigned int)mapped) >> hist_shift;
                }
            }
            if (bin0 >= hist_levels) {
                bin0 = hist_levels - 1u;
            }
            if (bin1 >= hist_levels) {
                bin1 = hist_levels - 1u;
            }
            if (bin2 >= hist_levels) {
                bin2 = hist_levels - 1u;
            }
            bin = (bin0 << (histbits * 2u))
                | (bin1 << histbits)
                | bin2;
            bin_hits[bin] += 1u;
            sum_base = (size_t)bin * 3u;
            bin_sum[sum_base + 0u] += c0;
            bin_sum[sum_base + 1u] += c1;
            bin_sum[sum_base + 2u] += c2;
            bin_sum_sq[sum_base + 0u] += c0 * c0;
            bin_sum_sq[sum_base + 1u] += c1 * c1;
            bin_sum_sq[sum_base + 2u] += c2 * c2;
            if (bin_seen[bin] == 0u) {
                bin_seen[bin] = 1u;
                bin_colors[bin * 3u + 0u] = c0;
                bin_colors[bin * 3u + 1u] = c1;
                bin_colors[bin * 3u + 2u] = c2;
                ++active_count;
            }
            ++visible_count;
        } else {
            if (channels == 4u && data[base + 3u] == 0u) {
                continue;
            }
            c0 = (double)data[base + 0u];
            c1 = (double)data[base + 1u];
            c2 = (double)data[base + 2u];
            bin0 = data[base + 0u] >> hist_shift;
            bin1 = data[base + 1u] >> hist_shift;
            bin2 = data[base + 2u] >> hist_shift;
            bin = (bin0 << (histbits * 2u))
                | (bin1 << histbits)
                | bin2;
            bin_hits[bin] += 1u;
            sum_base = (size_t)bin * 3u;
            bin_sum[sum_base + 0u] += c0;
            bin_sum[sum_base + 1u] += c1;
            bin_sum[sum_base + 2u] += c2;
            bin_sum_sq[sum_base + 0u] += c0 * c0;
            bin_sum_sq[sum_base + 1u] += c1 * c1;
            bin_sum_sq[sum_base + 2u] += c2 * c2;
            if (bin_seen[bin] == 0u) {
                bin_seen[bin] = 1u;
                bin_colors[bin * 3u + 0u] = c0;
                bin_colors[bin * 3u + 1u] = c1;
                bin_colors[bin * 3u + 2u] = c2;
                ++active_count;
            }
            ++visible_count;
        }
    }
    if (active_count == 0u || visible_count == 0u) {
        sixel_allocator_free(allocator, bin_sum_sq);
        sixel_allocator_free(allocator, bin_sum);
        sixel_allocator_free(allocator, bin_colors);
        sixel_allocator_free(allocator, bin_selected);
        sixel_allocator_free(allocator, bin_seen);
        sixel_allocator_free(allocator, bin_hits);
        return SIXEL_OK;
    }

    if (point_budget > active_count) {
        point_budget = active_count;
    }
    if (point_budget == 0u) {
        point_budget = 1u;
    }

    rank_desc = (sixel_kmedoids_bin_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)active_count * sizeof(sixel_kmedoids_bin_rank_t));
    rank_asc = (sixel_kmedoids_bin_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)active_count * sizeof(sixel_kmedoids_bin_rank_t));
    rank_var = (sixel_kmedoids_bin_variance_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)active_count * sizeof(sixel_kmedoids_bin_variance_rank_t));
    if (rank_desc == NULL || rank_asc == NULL || rank_var == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    active_index = 0u;
    for (bin = 0u; bin < bin_count; ++bin) {
        if (bin_seen[bin] == 0u || bin_hits[bin] == 0u) {
            continue;
        }
        rank_desc[active_index].index = bin;
        rank_desc[active_index].count = bin_hits[bin];
        rank_asc[active_index] = rank_desc[active_index];
        sum_base = (size_t)bin * 3u;
        hit_count = (double)bin_hits[bin];
        mean = bin_sum[sum_base + 0u] / hit_count;
        var0 = bin_sum_sq[sum_base + 0u] / hit_count - mean * mean;
        mean = bin_sum[sum_base + 1u] / hit_count;
        var1 = bin_sum_sq[sum_base + 1u] / hit_count - mean * mean;
        mean = bin_sum[sum_base + 2u] / hit_count;
        var2 = bin_sum_sq[sum_base + 2u] / hit_count - mean * mean;
        if (var0 < 0.0) {
            var0 = 0.0;
        }
        if (var1 < 0.0) {
            var1 = 0.0;
        }
        if (var2 < 0.0) {
            var2 = 0.0;
        }
        rank_var[active_index].index = bin;
        rank_var[active_index].score = hit_count * (var0 + var1 + var2);
        ++active_index;
    }
    if (active_index != active_count) {
        active_count = active_index;
    }

    qsort(rank_desc,
          (size_t)active_count,
          sizeof(sixel_kmedoids_bin_rank_t),
          sixel_kmedoids_compare_rank_desc);
    qsort(rank_asc,
          (size_t)active_count,
          sizeof(sixel_kmedoids_bin_rank_t),
          sixel_kmedoids_compare_rank_asc);
    qsort(rank_var,
          (size_t)active_count,
          sizeof(sixel_kmedoids_bin_variance_rank_t),
          sixel_kmedoids_compare_bin_variance_rank_desc);

    rare_limit = rare_keep;
    if (rare_limit > active_count) {
        rare_limit = active_count;
    }
    if (rare_limit > point_budget) {
        rare_limit = point_budget;
    }
    for (index = 0u; index < rare_limit; ++index) {
        bin = rank_asc[index].index;
        if (bin_selected[bin] != 0u) {
            continue;
        }
        bin_selected[bin] = 1u;
        ++selected_count;
        selected_mass += (uint64_t)bin_hits[bin];
    }

    required_mass_f = prune_mass * (double)visible_count;
    if (required_mass_f <= 0.0) {
        required_mass = 0u;
    } else {
        required_mass = (uint64_t)required_mass_f;
        if ((double)required_mass < required_mass_f) {
            ++required_mass;
        }
    }
    if (required_mass > (uint64_t)visible_count) {
        required_mass = (uint64_t)visible_count;
    }

    for (index = 0u; index < active_count; ++index) {
        if (selected_count >= point_budget) {
            break;
        }
        if (selected_mass >= required_mass && selected_count > 0u) {
            break;
        }
        bin = rank_desc[index].index;
        if (bin_selected[bin] != 0u) {
            continue;
        }
        bin_selected[bin] = 1u;
        ++selected_count;
        selected_mass += (uint64_t)bin_hits[bin];
    }
    for (index = 0u; index < active_count && selected_count < point_budget;
            ++index) {
        bin = rank_var[index].index;
        if (bin_selected[bin] != 0u) {
            continue;
        }
        bin_selected[bin] = 1u;
        ++selected_count;
        selected_mass += (uint64_t)bin_hits[bin];
    }
    for (index = 0u; index < active_count && selected_count < point_budget;
            ++index) {
        bin = rank_desc[index].index;
        if (bin_selected[bin] != 0u) {
            continue;
        }
        bin_selected[bin] = 1u;
        ++selected_count;
        selected_mass += (uint64_t)bin_hits[bin];
    }
    if (selected_count == 0u) {
        bin = rank_desc[0u].index;
        bin_selected[bin] = 1u;
        selected_count = 1u;
    }

    samples = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)selected_count * 3u * sizeof(double));
    sample_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)selected_count * sizeof(double));
    selected_slot_by_bin = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * sizeof(unsigned int));
    selected_best_index = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)selected_count * sizeof(unsigned int));
    selected_best_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)selected_count * sizeof(double));
    selected_colors = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)selected_count * 3u * sizeof(double));
    if (samples == NULL || sample_weights == NULL
            || selected_slot_by_bin == NULL
            || selected_best_index == NULL
            || selected_best_dist == NULL
            || selected_colors == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < bin_count; ++index) {
        selected_slot_by_bin[index] = UINT_MAX;
    }
    fill_count = 0u;
    for (index = 0u; index < active_count && fill_count < selected_count;
            ++index) {
        source = rank_desc[index].index;
        if (bin_selected[source] == 0u) {
            continue;
        }
        selected_slot_by_bin[source] = fill_count;
        sample_weights[fill_count] = (double)bin_hits[source];
        sum_base = (size_t)source * 3u;
        hit_count = (double)bin_hits[source];
        mean0 = bin_sum[sum_base + 0u] / hit_count;
        mean1 = bin_sum[sum_base + 1u] / hit_count;
        mean2 = bin_sum[sum_base + 2u] / hit_count;
        /* Keep bin mean temporarily; final representative color is filled
         * from the nearest observed sample in a second pass below. */
        samples[fill_count * 3u + 0u] = mean0;
        samples[fill_count * 3u + 1u] = mean1;
        samples[fill_count * 3u + 2u] = mean2;
        selected_colors[fill_count * 3u + 0u] = bin_colors[sum_base + 0u];
        selected_colors[fill_count * 3u + 1u] = bin_colors[sum_base + 1u];
        selected_colors[fill_count * 3u + 2u] = bin_colors[sum_base + 2u];
        selected_best_index[fill_count] = UINT_MAX;
        selected_best_dist[fill_count] = 1.0e300;
        ++fill_count;
    }
    if (fill_count == 0u) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (index = 0u; index < pixel_count; ++index) {
        base = index * pixel_stride;
        if (input_is_float32) {
            float const *fpixels;

            fpixels = (float const *)(void const *)(data + base);
            if (channels == 4u
                    && !sixel_kmedoids_float32_alpha_visible(
                           (double)fpixels[3u])) {
                continue;
            }
            c0 = (double)fpixels[0u];
            c1 = (double)fpixels[1u];
            c2 = (double)fpixels[2u];

            for (channel = 0u; channel < 3u; ++channel) {
                if (channel == 0u) {
                    component = c0;
                } else if (channel == 1u) {
                    component = c1;
                } else {
                    component = c2;
                }
                mapped = component;
                if (float_scale != NULL && float_offset != NULL
                        && float_scale[channel] > 0.0) {
                    mapped = component * float_scale[channel]
                           + float_offset[channel];
                }
                if (!isfinite(mapped) || mapped < 0.0) {
                    mapped = 0.0;
                } else if (mapped > 255.0) {
                    mapped = 255.0;
                }
                if (channel == 0u) {
                    bin0 = ((unsigned int)mapped) >> hist_shift;
                } else if (channel == 1u) {
                    bin1 = ((unsigned int)mapped) >> hist_shift;
                } else {
                    bin2 = ((unsigned int)mapped) >> hist_shift;
                }
            }
            if (bin0 >= hist_levels) {
                bin0 = hist_levels - 1u;
            }
            if (bin1 >= hist_levels) {
                bin1 = hist_levels - 1u;
            }
            if (bin2 >= hist_levels) {
                bin2 = hist_levels - 1u;
            }
            bin = (bin0 << (histbits * 2u))
                | (bin1 << histbits)
                | bin2;
        } else {
            if (channels == 4u && data[base + 3u] == 0u) {
                continue;
            }
            c0 = (double)data[base + 0u];
            c1 = (double)data[base + 1u];
            c2 = (double)data[base + 2u];
            bin0 = data[base + 0u] >> hist_shift;
            bin1 = data[base + 1u] >> hist_shift;
            bin2 = data[base + 2u] >> hist_shift;
            bin = (bin0 << (histbits * 2u))
                | (bin1 << histbits)
                | bin2;
        }
        if (bin >= bin_count) {
            continue;
        }
        sample_slot = selected_slot_by_bin[bin];
        if (sample_slot == UINT_MAX || sample_slot >= fill_count) {
            continue;
        }
        diff0 = c0 - samples[sample_slot * 3u + 0u];
        diff1 = c1 - samples[sample_slot * 3u + 1u];
        diff2 = c2 - samples[sample_slot * 3u + 2u];
        distance = diff0 * diff0 + diff1 * diff1 + diff2 * diff2;
        if (distance < selected_best_dist[sample_slot]
                || (distance == selected_best_dist[sample_slot]
                    && index < selected_best_index[sample_slot])) {
            selected_best_dist[sample_slot] = distance;
            selected_best_index[sample_slot] = index;
            selected_colors[sample_slot * 3u + 0u] = c0;
            selected_colors[sample_slot * 3u + 1u] = c1;
            selected_colors[sample_slot * 3u + 2u] = c2;
        }
    }
    for (index = 0u; index < fill_count; ++index) {
        samples[index * 3u + 0u] = selected_colors[index * 3u + 0u];
        samples[index * 3u + 1u] = selected_colors[index * 3u + 1u];
        samples[index * 3u + 2u] = selected_colors[index * 3u + 2u];
    }

    *samples_out = samples;
    *sample_weights_out = sample_weights;
    *sample_count_out = fill_count;
    *visible_count_out = visible_count;
    samples = NULL;
    sample_weights = NULL;
    status = SIXEL_OK;

end:
    if (selected_colors != NULL) {
        sixel_allocator_free(allocator, selected_colors);
    }
    if (selected_best_dist != NULL) {
        sixel_allocator_free(allocator, selected_best_dist);
    }
    if (selected_best_index != NULL) {
        sixel_allocator_free(allocator, selected_best_index);
    }
    if (selected_slot_by_bin != NULL) {
        sixel_allocator_free(allocator, selected_slot_by_bin);
    }
    if (rank_var != NULL) {
        sixel_allocator_free(allocator, rank_var);
    }
    if (rank_asc != NULL) {
        sixel_allocator_free(allocator, rank_asc);
    }
    if (rank_desc != NULL) {
        sixel_allocator_free(allocator, rank_desc);
    }
    if (bin_colors != NULL) {
        sixel_allocator_free(allocator, bin_colors);
    }
    if (bin_sum_sq != NULL) {
        sixel_allocator_free(allocator, bin_sum_sq);
    }
    if (bin_sum != NULL) {
        sixel_allocator_free(allocator, bin_sum);
    }
    if (bin_selected != NULL) {
        sixel_allocator_free(allocator, bin_selected);
    }
    if (bin_seen != NULL) {
        sixel_allocator_free(allocator, bin_seen);
    }
    if (bin_hits != NULL) {
        sixel_allocator_free(allocator, bin_hits);
    }
    if (samples != NULL) {
        sixel_allocator_free(allocator, samples);
    }
    if (sample_weights != NULL) {
        sixel_allocator_free(allocator, sample_weights);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_compress_samples(double const *samples,
                                double const *sample_weights,
                                unsigned int sample_count,
                                double **points_out,
                                double **weights_out,
                                unsigned int *point_count_out,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_kmedoids_unique_slot_t *table;
    double *points;
    double *weights;
    unsigned int capacity;
    unsigned int mask;
    unsigned int index;
    unsigned int slot;
    unsigned int probe;
    unsigned int unique_count;
    uint64_t key0;
    uint64_t key1;
    uint64_t key2;
    uint64_t hash;
    unsigned int entry_index;
    double sample_weight;

    status = SIXEL_BAD_ARGUMENT;
    table = NULL;
    points = NULL;
    weights = NULL;
    capacity = 0u;
    mask = 0u;
    index = 0u;
    slot = 0u;
    probe = 0u;
    unique_count = 0u;
    key0 = 0u;
    key1 = 0u;
    key2 = 0u;
    hash = 0u;
    entry_index = 0u;
    sample_weight = 0.0;

    if (points_out != NULL) {
        *points_out = NULL;
    }
    if (weights_out != NULL) {
        *weights_out = NULL;
    }
    if (point_count_out != NULL) {
        *point_count_out = 0u;
    }
    if (samples == NULL || points_out == NULL || weights_out == NULL
            || point_count_out == NULL || allocator == NULL) {
        return status;
    }
    if (sample_count == 0u) {
        return SIXEL_OK;
    }

    capacity = sixel_kmedoids_next_power_of_two(sample_count * 2u);
    mask = capacity - 1u;
    table = (sixel_kmedoids_unique_slot_t *)sixel_allocator_malloc(
        allocator,
        (size_t)capacity * sizeof(sixel_kmedoids_unique_slot_t));
    points = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_count * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_count * sizeof(double));
    if (table == NULL || points == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < capacity; ++index) {
        table[index].key0 = 0u;
        table[index].key1 = 0u;
        table[index].key2 = 0u;
        table[index].index = 0u;
        table[index].used = 0;
    }

    for (index = 0u; index < sample_count; ++index) {
        sample_weight = 1.0;
        if (sample_weights != NULL && isfinite(sample_weights[index])) {
            sample_weight = sample_weights[index];
        }
        if (sample_weight < 0.0) {
            sample_weight = 0.0;
        }
        key0 = sixel_kmedoids_double_bits(samples[index * 3u + 0u]);
        key1 = sixel_kmedoids_double_bits(samples[index * 3u + 1u]);
        key2 = sixel_kmedoids_double_bits(samples[index * 3u + 2u]);
        hash = sixel_kmedoids_hash_keys(key0, key1, key2);
        slot = (unsigned int)(hash & (uint64_t)mask);
        for (;;) {
            if (!table[slot].used) {
                table[slot].used = 1;
                table[slot].key0 = key0;
                table[slot].key1 = key1;
                table[slot].key2 = key2;
                table[slot].index = unique_count;
                points[unique_count * 3u + 0u] = samples[index * 3u + 0u];
                points[unique_count * 3u + 1u] = samples[index * 3u + 1u];
                points[unique_count * 3u + 2u] = samples[index * 3u + 2u];
                weights[unique_count] = sample_weight;
                ++unique_count;
                break;
            }
            if (table[slot].key0 == key0 && table[slot].key1 == key1
                    && table[slot].key2 == key2) {
                entry_index = table[slot].index;
                weights[entry_index] += sample_weight;
                break;
            }
            probe = (slot + 1u) & mask;
            slot = probe;
        }
    }

    *points_out = points;
    *weights_out = weights;
    *point_count_out = unique_count;
    points = NULL;
    weights = NULL;
    status = SIXEL_OK;

end:
    if (table != NULL) {
        sixel_allocator_free(allocator, table);
    }
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    return status;
}

static double
sixel_kmedoids_distance_sq(double const *points,
                           unsigned int lhs,
                           unsigned int rhs)
{
    size_t cache_index;
    double diff0;
    double diff1;
    double diff2;

    cache_index = 0u;
    if (sixel_kmedoids_distance_cache != NULL
            && sixel_kmedoids_distance_cache_size > 0u
            && lhs < sixel_kmedoids_distance_cache_size
            && rhs < sixel_kmedoids_distance_cache_size) {
        cache_index = (size_t)lhs * (size_t)sixel_kmedoids_distance_cache_size
                    + (size_t)rhs;
        return sixel_kmedoids_distance_cache[cache_index];
    }

    diff0 = points[lhs * 3u + 0u] - points[rhs * 3u + 0u];
    diff1 = points[lhs * 3u + 1u] - points[rhs * 3u + 1u];
    diff2 = points[lhs * 3u + 2u] - points[rhs * 3u + 2u];
    return diff0 * diff0 + diff1 * diff1 + diff2 * diff2;
}

static void
sixel_kmedoids_mark_medoids(unsigned char *flags,
                            unsigned int const *medoids,
                            unsigned int k,
                            unsigned int point_count)
{
    unsigned int index;

    index = 0u;
    if (flags == NULL || medoids == NULL) {
        return;
    }
    for (index = 0u; index < point_count; ++index) {
        flags[index] = 0u;
    }
    for (index = 0u; index < k; ++index) {
        if (medoids[index] < point_count) {
            flags[medoids[index]] = 1u;
        }
    }
}

static void
sixel_kmedoids_choose_initial_deterministic(double const *points,
                                            double const *weights,
                                            unsigned int point_count,
                                            unsigned int k,
                                            unsigned int *medoids,
                                            unsigned char *flags)
{
    unsigned int selected;
    unsigned int index;
    unsigned int seed_index;
    unsigned int candidate;
    double seed_weight;
    double best_score;
    double nearest;
    double distance;
    double weight;
    unsigned int slot;

    selected = 0u;
    index = 0u;
    seed_index = 0u;
    candidate = 0u;
    seed_weight = -1.0;
    best_score = -1.0;
    nearest = 0.0;
    distance = 0.0;
    weight = 1.0;
    slot = 0u;

    if (points == NULL || medoids == NULL || flags == NULL || point_count == 0u
            || k == 0u) {
        return;
    }

    for (index = 0u; index < point_count; ++index) {
        flags[index] = 0u;
        weight = 1.0;
        if (weights != NULL) {
            weight = weights[index];
        }
        if (weight > seed_weight) {
            seed_weight = weight;
            seed_index = index;
        }
    }

    medoids[0u] = seed_index;
    flags[seed_index] = 1u;
    selected = 1u;

    while (selected < k) {
        candidate = 0u;
        best_score = -1.0;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index]) {
                continue;
            }
            nearest = 0.0;
            for (slot = 0u; slot < selected; ++slot) {
                distance = sixel_kmedoids_distance_sq(points,
                                                      index,
                                                      medoids[slot]);
                if (slot == 0u || distance < nearest) {
                    nearest = distance;
                }
            }
            weight = 1.0;
            if (weights != NULL) {
                weight = weights[index];
            }
            distance = nearest * weight;
            if (distance > best_score) {
                best_score = distance;
                candidate = index;
            }
        }
        medoids[selected] = candidate;
        flags[candidate] = 1u;
        ++selected;
    }
}

static SIXELSTATUS
sixel_kmedoids_choose_initial_kpp(double const *points,
                                  double const *weights,
                                  unsigned int point_count,
                                  unsigned int k,
                                  unsigned int *medoids,
                                  unsigned char *flags,
                                  double *nearest_dist,
                                  uint32_t *rng_state)
{
    SIXELSTATUS status;
    unsigned int index;
    unsigned int slot;
    unsigned int candidate;
    unsigned int fallback;
    double total;
    double pick;
    double distance;
    double weight;
    double running;

    status = SIXEL_BAD_ARGUMENT;
    index = 0u;
    slot = 0u;
    candidate = 0u;
    fallback = 0u;
    total = 0.0;
    pick = 0.0;
    distance = 0.0;
    weight = 1.0;
    running = 0.0;

    if (points == NULL || medoids == NULL || flags == NULL
            || nearest_dist == NULL || rng_state == NULL
            || point_count == 0u || k == 0u) {
        return status;
    }

    for (index = 0u; index < point_count; ++index) {
        flags[index] = 0u;
        weight = 1.0;
        if (weights != NULL && isfinite(weights[index])) {
            weight = weights[index];
        }
        if (weight < 0.0) {
            weight = 0.0;
        }
        total += weight;
    }

    candidate = sixel_kmedoids_rng_bounded(rng_state, point_count);
    if (total > 0.0) {
        pick = sixel_kmedoids_rng_unit(rng_state) * total;
        running = 0.0;
        for (index = 0u; index < point_count; ++index) {
            weight = 1.0;
            if (weights != NULL && isfinite(weights[index])) {
                weight = weights[index];
            }
            if (weight < 0.0) {
                weight = 0.0;
            }
            running += weight;
            if (running >= pick) {
                candidate = index;
                break;
            }
        }
    }

    medoids[0u] = candidate;
    flags[candidate] = 1u;

    for (index = 0u; index < point_count; ++index) {
        nearest_dist[index] = sixel_kmedoids_distance_sq(points,
                                                         index,
                                                         candidate);
    }

    for (slot = 1u; slot < k; ++slot) {
        total = 0.0;
        fallback = point_count;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] != 0u) {
                continue;
            }
            if (fallback >= point_count) {
                fallback = index;
            }
            weight = 1.0;
            if (weights != NULL && isfinite(weights[index])) {
                weight = weights[index];
            }
            if (weight < 0.0) {
                weight = 0.0;
            }
            total += nearest_dist[index] * weight;
        }
        if (fallback >= point_count) {
            break;
        }

        candidate = fallback;
        if (total > 0.0) {
            pick = sixel_kmedoids_rng_unit(rng_state) * total;
            running = 0.0;
            for (index = 0u; index < point_count; ++index) {
                if (flags[index] != 0u) {
                    continue;
                }
                weight = 1.0;
                if (weights != NULL && isfinite(weights[index])) {
                    weight = weights[index];
                }
                if (weight < 0.0) {
                    weight = 0.0;
                }
                running += nearest_dist[index] * weight;
                if (running >= pick) {
                    candidate = index;
                    break;
                }
            }
        }

        medoids[slot] = candidate;
        flags[candidate] = 1u;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] != 0u) {
                continue;
            }
            distance = sixel_kmedoids_distance_sq(points, index, candidate);
            if (distance < nearest_dist[index]) {
                nearest_dist[index] = distance;
            }
        }
    }

    return SIXEL_OK;
}

static void
sixel_kmedoids_assign_points(double const *points,
                             double const *weights,
                             unsigned int point_count,
                             unsigned int const *medoids,
                             unsigned int k,
                             unsigned int *nearest_slot,
                             double *nearest_dist,
                             double *second_dist,
                             unsigned int *second_slot,
                             double *cluster_weights,
                             double *cluster_sums,
                             double *cost_out)
{
    unsigned int point_index;
    unsigned int slot;
    unsigned int best_slot;
    unsigned int next_slot;
    double best_distance;
    double next_distance;
    double distance;
    double weight;
    double cost;
    unsigned int channel;
    size_t offset;

    point_index = 0u;
    slot = 0u;
    best_slot = 0u;
    next_slot = 0u;
    best_distance = 0.0;
    next_distance = 0.0;
    distance = 0.0;
    weight = 1.0;
    cost = 0.0;
    channel = 0u;
    offset = 0u;

    if (points == NULL || medoids == NULL || point_count == 0u || k == 0u) {
        if (cost_out != NULL) {
            *cost_out = 0.0;
        }
        return;
    }

    if (cluster_weights != NULL) {
        for (slot = 0u; slot < k; ++slot) {
            cluster_weights[slot] = 0.0;
        }
    }
    if (cluster_sums != NULL) {
        for (slot = 0u; slot < k * 3u; ++slot) {
            cluster_sums[slot] = 0.0;
        }
    }

    for (point_index = 0u; point_index < point_count; ++point_index) {
        best_slot = 0u;
        next_slot = 0u;
        best_distance = 1.0e300;
        next_distance = 1.0e300;
        for (slot = 0u; slot < k; ++slot) {
            distance = sixel_kmedoids_distance_sq(points,
                                                  point_index,
                                                  medoids[slot]);
            if (distance < best_distance) {
                next_distance = best_distance;
                next_slot = best_slot;
                best_distance = distance;
                best_slot = slot;
            } else if (distance < next_distance) {
                next_distance = distance;
                next_slot = slot;
            }
        }
        if (k == 1u || next_distance >= 1.0e300) {
            next_distance = best_distance;
            next_slot = best_slot;
        }
        if (nearest_slot != NULL) {
            nearest_slot[point_index] = best_slot;
        }
        if (nearest_dist != NULL) {
            nearest_dist[point_index] = best_distance;
        }
        if (second_dist != NULL) {
            second_dist[point_index] = next_distance;
        }
        if (second_slot != NULL) {
            second_slot[point_index] = next_slot;
        }
        weight = 1.0;
        if (weights != NULL) {
            weight = weights[point_index];
        }
        cost += weight * best_distance;
        if (cluster_weights != NULL) {
            cluster_weights[best_slot] += weight;
        }
        if (cluster_sums != NULL) {
            offset = (size_t)best_slot * 3u;
            for (channel = 0u; channel < 3u; ++channel) {
                cluster_sums[offset + channel]
                    += points[point_index * 3u + channel] * weight;
            }
        }
    }

    if (cost_out != NULL) {
        *cost_out = cost;
    }
}

static void
sixel_kmedoids_update_assignments_after_swap_ex(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *medoids,
    unsigned int k,
    unsigned int swapped_slot,
    unsigned int new_medoid,
    unsigned int *nearest_slot,
    double *nearest_dist,
    double *second_dist,
    unsigned int *second_slot,
    double *cost_out,
    unsigned int *changed_points,
    unsigned int *changed_count_out,
    double *slot_delta,
    unsigned int *changed_old_slots,
    unsigned int *changed_new_slots)
{
    unsigned int index;
    unsigned int slot;
    unsigned int best_slot;
    unsigned int next_slot;
    unsigned int old_slot;
    unsigned int old_second_slot;
    double best_distance;
    double next_distance;
    double distance;
    double old_nearest;
    double old_second;
    double old_residual;
    double new_residual;
    double weight;
    double cost;
    unsigned int changed_count;

    index = 0u;
    slot = 0u;
    best_slot = 0u;
    next_slot = 0u;
    old_slot = 0u;
    old_second_slot = 0u;
    best_distance = 0.0;
    next_distance = 0.0;
    distance = 0.0;
    old_nearest = 0.0;
    old_second = 0.0;
    old_residual = 0.0;
    new_residual = 0.0;
    weight = 1.0;
    cost = 0.0;
    changed_count = 0u;

    if (points == NULL || medoids == NULL || nearest_slot == NULL
            || nearest_dist == NULL || second_dist == NULL
            || second_slot == NULL || point_count == 0u || k == 0u) {
        if (cost_out != NULL) {
            *cost_out = 0.0;
        }
        if (changed_count_out != NULL) {
            *changed_count_out = 0u;
        }
        return;
    }
    if (slot_delta != NULL) {
        for (slot = 0u; slot < k; ++slot) {
            slot_delta[slot] = 0.0;
        }
    }

    for (index = 0u; index < point_count; ++index) {
        old_slot = nearest_slot[index];
        old_second_slot = second_slot[index];
        old_nearest = nearest_dist[index];
        old_second = second_dist[index];
        if (nearest_slot[index] == swapped_slot) {
            best_slot = 0u;
            next_slot = 0u;
            best_distance = 1.0e300;
            next_distance = 1.0e300;
            for (slot = 0u; slot < k; ++slot) {
                distance = sixel_kmedoids_distance_sq(points,
                                                      index,
                                                      medoids[slot]);
                if (distance < best_distance) {
                    next_distance = best_distance;
                    next_slot = best_slot;
                    best_distance = distance;
                    best_slot = slot;
                } else if (distance < next_distance) {
                    next_distance = distance;
                    next_slot = slot;
                }
            }
            if (k == 1u || next_distance >= 1.0e300) {
                next_distance = best_distance;
                next_slot = best_slot;
            }
            nearest_slot[index] = best_slot;
            nearest_dist[index] = best_distance;
            second_dist[index] = next_distance;
            second_slot[index] = next_slot;
        } else {
            distance = sixel_kmedoids_distance_sq(points,
                                                  index,
                                                  new_medoid);
            if (distance < nearest_dist[index]) {
                second_dist[index] = nearest_dist[index];
                second_slot[index] = nearest_slot[index];
                nearest_dist[index] = distance;
                nearest_slot[index] = swapped_slot;
            } else if (second_slot[index] == swapped_slot) {
                best_distance = 1.0e300;
                best_slot = nearest_slot[index];
                for (slot = 0u; slot < k; ++slot) {
                    if (slot == nearest_slot[index]) {
                        continue;
                    }
                    next_distance = sixel_kmedoids_distance_sq(points,
                                                               index,
                                                               medoids[slot]);
                    if (next_distance < best_distance) {
                        best_distance = next_distance;
                        best_slot = slot;
                    }
                }
                if (k == 1u || best_distance >= 1.0e300) {
                    best_distance = nearest_dist[index];
                    best_slot = nearest_slot[index];
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_slot = swapped_slot;
                }
                second_dist[index] = best_distance;
                second_slot[index] = best_slot;
            } else if (distance < second_dist[index]) {
                second_dist[index] = distance;
                second_slot[index] = swapped_slot;
            }
        }
        if (k == 1u) {
            second_dist[index] = nearest_dist[index];
            second_slot[index] = nearest_slot[index];
        }

        weight = 1.0;
        if (weights != NULL && isfinite(weights[index])) {
            weight = weights[index];
        }
        if (weight < 0.0) {
            weight = 0.0;
        }
        old_residual = old_nearest * weight;
        if (!isfinite(old_residual) || old_residual < 0.0) {
            old_residual = 0.0;
        }
        new_residual = nearest_dist[index] * weight;
        if (!isfinite(new_residual) || new_residual < 0.0) {
            new_residual = 0.0;
        }
        if (slot_delta != NULL) {
            if (old_slot < k) {
                slot_delta[old_slot] -= old_residual;
            }
            if (nearest_slot[index] < k) {
                slot_delta[nearest_slot[index]] += new_residual;
            }
        }
        cost += new_residual;
        if (changed_points != NULL
                && (old_slot != nearest_slot[index]
                    || old_second_slot != second_slot[index]
                    || old_nearest != nearest_dist[index]
                    || old_second != second_dist[index])) {
            changed_points[changed_count] = index;
            if (changed_old_slots != NULL) {
                changed_old_slots[changed_count] = old_slot;
            }
            if (changed_new_slots != NULL) {
                changed_new_slots[changed_count] = nearest_slot[index];
            }
            ++changed_count;
        }
    }

    if (cost_out != NULL) {
        *cost_out = cost;
    }
    if (changed_count_out != NULL) {
        *changed_count_out = changed_count;
    }
}

static void
sixel_kmedoids_update_assignments_after_swap(double const *points,
                                             double const *weights,
                                             unsigned int point_count,
                                             unsigned int const *medoids,
                                             unsigned int k,
                                             unsigned int swapped_slot,
                                             unsigned int new_medoid,
                                             unsigned int *nearest_slot,
                                             double *nearest_dist,
                                             double *second_dist,
                                             unsigned int *second_slot,
                                             double *cost_out)
{
    sixel_kmedoids_update_assignments_after_swap_ex(points,
                                                    weights,
                                                    point_count,
                                                    medoids,
                                                    k,
                                                    swapped_slot,
                                                    new_medoid,
                                                    nearest_slot,
                                                    nearest_dist,
                                                    second_dist,
                                                    second_slot,
                                                    cost_out,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL);
}

SIXEL_INTERNAL_API void
sixel_kmedoids_test_assign_points(double const *points,
                                  double const *weights,
                                  unsigned int point_count,
                                  unsigned int const *medoids,
                                  unsigned int k,
                                  unsigned int *nearest_slot,
                                  double *nearest_dist,
                                  double *second_dist,
                                  unsigned int *second_slot,
                                  double *cost_out)
{
    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 medoids,
                                 k,
                                 nearest_slot,
                                 nearest_dist,
                                 second_dist,
                                 second_slot,
                                 NULL,
                                 NULL,
                                 cost_out);
}

SIXEL_INTERNAL_API void
sixel_kmedoids_test_update_assignments_after_swap(double const *points,
                                                  double const *weights,
                                                  unsigned int point_count,
                                                  unsigned int const *medoids,
                                                  unsigned int k,
                                                  unsigned int swapped_slot,
                                                  unsigned int new_medoid,
                                                  unsigned int *nearest_slot,
                                                  double *nearest_dist,
                                                  double *second_dist,
                                                  unsigned int *second_slot,
                                                  double *cost_out)
{
    sixel_kmedoids_update_assignments_after_swap(points,
                                                 weights,
                                                 point_count,
                                                 medoids,
                                                 k,
                                                 swapped_slot,
                                                 new_medoid,
                                                 nearest_slot,
                                                 nearest_dist,
                                                 second_dist,
                                                 second_slot,
                                                 cost_out);
}

static double
sixel_kmedoids_swap_cost_ordered_with_row(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int enable_cutoff,
    int *early_stop_out)
{
    unsigned int ordered_index;
    unsigned int index;
    double distance;
    double chosen;
    double weight;
    double cost;
    int use_cutoff;
    int early_stop;

    ordered_index = 0u;
    index = 0u;
    distance = 0.0;
    chosen = 0.0;
    weight = 1.0;
    cost = 0.0;
    use_cutoff = 0;
    early_stop = 0;
    if (early_stop_out != NULL) {
        *early_stop_out = 0;
    }

    if (enable_cutoff && cutoff == cutoff) {
        use_cutoff = 1;
    }

    for (ordered_index = 0u; ordered_index < point_count; ++ordered_index) {
        if (order != NULL) {
            index = order[ordered_index];
        } else {
            index = ordered_index;
        }
        if (candidate_dist_row != NULL) {
            if (candidate_generation_row != NULL
                    && candidate_row_epoch != 0u) {
                if (candidate_generation_row[index] == candidate_row_epoch) {
                    distance = candidate_dist_row[index];
                } else {
                    distance = sixel_kmedoids_distance_sq(points,
                                                          index,
                                                          candidate_point);
                    candidate_dist_row[index] = distance;
                    candidate_generation_row[index] = candidate_row_epoch;
                }
            } else {
                distance = candidate_dist_row[index];
                if (!isfinite(distance) || distance < 0.0) {
                    distance = sixel_kmedoids_distance_sq(points,
                                                          index,
                                                          candidate_point);
                    candidate_dist_row[index] = distance;
                }
            }
        } else {
            distance = sixel_kmedoids_distance_sq(points,
                                                  index,
                                                  candidate_point);
        }
        if (nearest_slot[index] == replace_slot) {
            chosen = second_dist[index];
            if (distance < chosen) {
                chosen = distance;
            }
        } else {
            chosen = nearest_dist[index];
            if (distance < chosen) {
                chosen = distance;
            }
        }
        weight = 1.0;
        if (weights != NULL) {
            weight = weights[index];
        }
        cost += chosen * weight;

        /*
         * Guard correctness for unexpected negative weights:
         * disable cutoff and keep full summation semantics.
         */
        if (weight < 0.0) {
            use_cutoff = 0;
        }
        if (use_cutoff && cost >= cutoff) {
            early_stop = 1;
            break;
        }
    }

    if (early_stop_out != NULL) {
        *early_stop_out = early_stop;
    }
    return cost;
}

static double
sixel_kmedoids_swap_cost_with_cutoff(double const *points,
                                     double const *weights,
                                     unsigned int point_count,
                                     unsigned int const *nearest_slot,
                                     double const *nearest_dist,
                                     double const *second_dist,
                                     unsigned int replace_slot,
                                     unsigned int candidate_point,
                                     unsigned int const *order,
                                     double cutoff,
                                     int *early_stop_out)
{
    return sixel_kmedoids_swap_cost_ordered_with_row(points,
                                                     weights,
                                                     point_count,
                                                     nearest_slot,
                                                     nearest_dist,
                                                     second_dist,
                                                     replace_slot,
                                                     candidate_point,
                                                     NULL,
                                                     NULL,
                                                     0u,
                                                     order,
                                                     cutoff,
                                                     1,
                                                     early_stop_out);
}

static double
sixel_kmedoids_swap_cost_prefix_with_cutoff_row_generation(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int prefix_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out)
{
    unsigned int bounded_count;

    bounded_count = prefix_count;
    if (bounded_count > point_count) {
        bounded_count = point_count;
    }
    return sixel_kmedoids_swap_cost_ordered_with_row(points,
                                                     weights,
                                                     bounded_count,
                                                     nearest_slot,
                                                     nearest_dist,
                                                     second_dist,
                                                     replace_slot,
                                                     candidate_point,
                                                     candidate_dist_row,
                                                     candidate_generation_row,
                                                     candidate_row_epoch,
                                                     order,
                                                     cutoff,
                                                     1,
                                                     early_stop_out);
}

static double
sixel_kmedoids_swap_cost_with_cutoff_row_generation(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out)
{
    return sixel_kmedoids_swap_cost_ordered_with_row(points,
                                                     weights,
                                                     point_count,
                                                     nearest_slot,
                                                     nearest_dist,
                                                     second_dist,
                                                     replace_slot,
                                                     candidate_point,
                                                     candidate_dist_row,
                                                     candidate_generation_row,
                                                     candidate_row_epoch,
                                                     order,
                                                     cutoff,
                                                     1,
                                                     early_stop_out);
}

static double
sixel_kmedoids_swap_cost_with_cutoff_row(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out)
{
    return sixel_kmedoids_swap_cost_with_cutoff_row_generation(
        points,
        weights,
        point_count,
        nearest_slot,
        nearest_dist,
        second_dist,
        replace_slot,
        candidate_point,
        candidate_dist_row,
        NULL,
        0u,
        order,
        cutoff,
        early_stop_out);
}

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
                                     int *early_stop_out)
{
    return sixel_kmedoids_swap_cost_with_cutoff(points,
                                                weights,
                                                point_count,
                                                nearest_slot,
                                                nearest_dist,
                                                second_dist,
                                                replace_slot,
                                                candidate_point,
                                                order,
                                                cutoff,
                                                early_stop_out);
}

SIXEL_INTERNAL_API double
sixel_kmedoids_test_swap_cost_cutoff_with_row(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out)
{
    return sixel_kmedoids_swap_cost_with_cutoff_row(points,
                                                    weights,
                                                    point_count,
                                                    nearest_slot,
                                                    nearest_dist,
                                                    second_dist,
                                                    replace_slot,
                                                    candidate_point,
                                                    candidate_dist_row,
                                                    order,
                                                    cutoff,
                                                    early_stop_out);
}

SIXEL_INTERNAL_API double
sixel_kmedoids_test_swap_cost_cutoff_with_row_generation(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int replace_slot,
    unsigned int candidate_point,
    double *candidate_dist_row,
    uint32_t *candidate_generation_row,
    uint32_t candidate_row_epoch,
    unsigned int const *order,
    double cutoff,
    int *early_stop_out)
{
    return sixel_kmedoids_swap_cost_with_cutoff_row_generation(
        points,
        weights,
        point_count,
        nearest_slot,
        nearest_dist,
        second_dist,
        replace_slot,
        candidate_point,
        candidate_dist_row,
        candidate_generation_row,
        candidate_row_epoch,
        order,
        cutoff,
        early_stop_out);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_eval_order_residual(
    double const *weights,
    double const *nearest_dist,
    unsigned int point_count,
    unsigned int *order_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *positions;
    double *scores;
    sixel_kmedoids_point_weight_rank_t *rank;

    status = SIXEL_BAD_ARGUMENT;
    positions = NULL;
    scores = NULL;
    rank = NULL;
    if (order_out == NULL || allocator == NULL || point_count == 0u) {
        return status;
    }
    positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(sixel_kmedoids_point_weight_rank_t));
    if (positions == NULL || scores == NULL || rank == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    sixel_kmedoids_eval_order_full_refresh(weights,
                                           nearest_dist,
                                           point_count,
                                           order_out,
                                           positions,
                                           scores,
                                           rank);
    status = SIXEL_OK;

end:
    if (rank != NULL) {
        sixel_allocator_free(allocator, rank);
    }
    if (scores != NULL) {
        sixel_allocator_free(allocator, scores);
    }
    if (positions != NULL) {
        sixel_allocator_free(allocator, positions);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_apply_eval_order_delta(
    double const *weights,
    double const *nearest_dist_before,
    double const *nearest_dist_after,
    unsigned int point_count,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int delta_threshold,
    unsigned int *order_io,
    int *full_refresh_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *positions;
    double *scores;
    sixel_kmedoids_point_weight_rank_t *rank;
    unsigned int index;
    int used_full_refresh;

    status = SIXEL_BAD_ARGUMENT;
    positions = NULL;
    scores = NULL;
    rank = NULL;
    index = 0u;
    used_full_refresh = 0;
    if (full_refresh_out != NULL) {
        *full_refresh_out = 0;
    }
    if (order_io == NULL || allocator == NULL || point_count == 0u
            || nearest_dist_before == NULL || nearest_dist_after == NULL) {
        return status;
    }
    positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(sixel_kmedoids_point_weight_rank_t));
    if (positions == NULL || scores == NULL || rank == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < point_count; ++index) {
        positions[order_io[index]] = index;
        scores[index] = sixel_kmedoids_eval_score(weights,
                                                  nearest_dist_before,
                                                  index);
        rank[index].index = index;
        rank[index].weight = 0.0;
    }
    used_full_refresh = sixel_kmedoids_eval_order_apply_delta(
        weights,
        nearest_dist_after,
        point_count,
        changed_points,
        changed_count,
        delta_threshold,
        order_io,
        positions,
        scores,
        rank);
    if (full_refresh_out != NULL) {
        *full_refresh_out = used_full_refresh;
    }
    status = SIXEL_OK;

end:
    if (rank != NULL) {
        sixel_allocator_free(allocator, rank);
    }
    if (scores != NULL) {
        sixel_allocator_free(allocator, scores);
    }
    if (positions != NULL) {
        sixel_allocator_free(allocator, positions);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_clarans_slot_order(
    double const *weights,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int point_count,
    unsigned int k,
    unsigned int slot,
    unsigned int *order_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *residual_order;
    unsigned int *residual_positions;
    double *residual_scores;
    sixel_kmedoids_point_weight_rank_t *residual_rank;
    double *damage_scores;
    unsigned int *slot_orders;
    unsigned int *slot_positions;
    sixel_kmedoids_point_weight_rank_t *slot_rank_work;
    uint64_t slot_cells64;
    unsigned int index;

    status = SIXEL_BAD_ARGUMENT;
    residual_order = NULL;
    residual_positions = NULL;
    residual_scores = NULL;
    residual_rank = NULL;
    damage_scores = NULL;
    slot_orders = NULL;
    slot_positions = NULL;
    slot_rank_work = NULL;
    slot_cells64 = 0u;
    index = 0u;
    if (nearest_slot == NULL || nearest_dist == NULL || second_dist == NULL
            || point_count == 0u || k == 0u || slot >= k
            || order_out == NULL || allocator == NULL) {
        return status;
    }

    slot_cells64 = (uint64_t)point_count * (uint64_t)k;
    if (slot_cells64
            > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(unsigned int))) {
        return SIXEL_BAD_ALLOCATION;
    }

    residual_order = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    residual_positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    residual_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    residual_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    damage_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    slot_orders = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)slot_cells64 * sizeof(unsigned int));
    slot_positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)slot_cells64 * sizeof(unsigned int));
    slot_rank_work = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    if (residual_order == NULL || residual_positions == NULL
            || residual_scores == NULL || residual_rank == NULL
            || damage_scores == NULL || slot_orders == NULL
            || slot_positions == NULL || slot_rank_work == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    sixel_kmedoids_eval_order_full_refresh(weights,
                                           nearest_dist,
                                           point_count,
                                           residual_order,
                                           residual_positions,
                                           residual_scores,
                                           residual_rank);
    sixel_kmedoids_clarans_damage_full_refresh(weights,
                                               nearest_dist,
                                               second_dist,
                                               point_count,
                                               damage_scores);
    sixel_kmedoids_clarans_slot_orders_full_refresh(point_count,
                                                    k,
                                                    nearest_slot,
                                                    residual_order,
                                                    damage_scores,
                                                    slot_orders,
                                                    slot_positions,
                                                    slot_rank_work);
    for (index = 0u; index < point_count; ++index) {
        order_out[index] = slot_orders[slot * point_count + index];
    }
    status = SIXEL_OK;

end:
    if (slot_rank_work != NULL) {
        sixel_allocator_free(allocator, slot_rank_work);
    }
    if (slot_orders != NULL) {
        sixel_allocator_free(allocator, slot_orders);
    }
    if (damage_scores != NULL) {
        sixel_allocator_free(allocator, damage_scores);
    }
    if (residual_rank != NULL) {
        sixel_allocator_free(allocator, residual_rank);
    }
    if (residual_scores != NULL) {
        sixel_allocator_free(allocator, residual_scores);
    }
    if (residual_positions != NULL) {
        sixel_allocator_free(allocator, residual_positions);
    }
    if (residual_order != NULL) {
        sixel_allocator_free(allocator, residual_order);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_build_clarans_slot_order_lazy(
    double const *weights,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int point_count,
    unsigned int k,
    unsigned int slot,
    unsigned int const *probe_slots,
    unsigned int probe_count,
    unsigned int *order_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *residual_order;
    unsigned int *residual_positions;
    double *residual_scores;
    sixel_kmedoids_point_weight_rank_t *residual_rank;
    double *damage_scores;
    unsigned int *slot_orders;
    unsigned char *slot_dirty;
    uint32_t *slot_generation;
    sixel_kmedoids_point_weight_rank_t *slot_rank_work;
    uint64_t slot_cells64;
    uint32_t slot_generation_id;
    unsigned int query_slot;
    unsigned int index;
    unsigned int query_index;
    unsigned int *order_row;

    status = SIXEL_BAD_ARGUMENT;
    residual_order = NULL;
    residual_positions = NULL;
    residual_scores = NULL;
    residual_rank = NULL;
    damage_scores = NULL;
    slot_orders = NULL;
    slot_dirty = NULL;
    slot_generation = NULL;
    slot_rank_work = NULL;
    slot_cells64 = 0u;
    slot_generation_id = 0u;
    query_slot = 0u;
    index = 0u;
    query_index = 0u;
    order_row = NULL;
    if (nearest_slot == NULL || nearest_dist == NULL || second_dist == NULL
            || point_count == 0u || k == 0u || slot >= k
            || order_out == NULL || allocator == NULL) {
        return status;
    }
    if (probe_count > 0u && probe_slots == NULL) {
        return status;
    }

    slot_cells64 = (uint64_t)point_count * (uint64_t)k;
    if (slot_cells64
            > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(unsigned int))) {
        return SIXEL_BAD_ALLOCATION;
    }

    residual_order = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    residual_positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    residual_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    residual_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    damage_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    slot_orders = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)slot_cells64 * sizeof(unsigned int));
    slot_dirty = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned char));
    slot_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(uint32_t));
    slot_rank_work = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    if (residual_order == NULL || residual_positions == NULL
            || residual_scores == NULL || residual_rank == NULL
            || damage_scores == NULL || slot_orders == NULL
            || slot_dirty == NULL || slot_generation == NULL
            || slot_rank_work == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < k; ++index) {
        slot_dirty[index] = 1u;
        slot_generation[index] = 0u;
    }
    sixel_kmedoids_eval_order_full_refresh(weights,
                                           nearest_dist,
                                           point_count,
                                           residual_order,
                                           residual_positions,
                                           residual_scores,
                                           residual_rank);
    sixel_kmedoids_clarans_damage_full_refresh(weights,
                                               nearest_dist,
                                               second_dist,
                                               point_count,
                                               damage_scores);
    sixel_kmedoids_clarans_slot_generation_next(&slot_generation_id,
                                                slot_generation,
                                                k);
    sixel_kmedoids_clarans_mark_all_slots_dirty(slot_dirty, k);

    if (probe_count == 0u) {
        order_row = sixel_kmedoids_clarans_get_slot_order_row(
            point_count,
            k,
            slot,
            nearest_slot,
            residual_order,
            damage_scores,
            slot_orders,
            slot_dirty,
            slot_generation,
            slot_generation_id,
            slot_rank_work);
        if (order_row == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    } else {
        for (query_index = 0u; query_index < probe_count; ++query_index) {
            query_slot = probe_slots[query_index];
            if (query_slot >= k) {
                continue;
            }
            order_row = sixel_kmedoids_clarans_get_slot_order_row(
                point_count,
                k,
                query_slot,
                nearest_slot,
                residual_order,
                damage_scores,
                slot_orders,
                slot_dirty,
                slot_generation,
                slot_generation_id,
                slot_rank_work);
            if (order_row == NULL) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
    }

    order_row = sixel_kmedoids_clarans_get_slot_order_row(
        point_count,
        k,
        slot,
        nearest_slot,
        residual_order,
        damage_scores,
        slot_orders,
        slot_dirty,
        slot_generation,
        slot_generation_id,
        slot_rank_work);
    if (order_row == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    for (index = 0u; index < point_count; ++index) {
        order_out[index] = order_row[index];
    }
    status = SIXEL_OK;

end:
    if (slot_rank_work != NULL) {
        sixel_allocator_free(allocator, slot_rank_work);
    }
    if (slot_generation != NULL) {
        sixel_allocator_free(allocator, slot_generation);
    }
    if (slot_dirty != NULL) {
        sixel_allocator_free(allocator, slot_dirty);
    }
    if (slot_orders != NULL) {
        sixel_allocator_free(allocator, slot_orders);
    }
    if (damage_scores != NULL) {
        sixel_allocator_free(allocator, damage_scores);
    }
    if (residual_rank != NULL) {
        sixel_allocator_free(allocator, residual_rank);
    }
    if (residual_scores != NULL) {
        sixel_allocator_free(allocator, residual_scores);
    }
    if (residual_positions != NULL) {
        sixel_allocator_free(allocator, residual_positions);
    }
    if (residual_order != NULL) {
        sixel_allocator_free(allocator, residual_order);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_clarans_slot_order_dirty_rebuild(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot_before,
    double const *nearest_dist_before,
    double const *second_dist_before,
    unsigned int const *nearest_slot_after,
    double const *nearest_dist_after,
    double const *second_dist_after,
    unsigned int swapped_slot,
    unsigned int const *changed_old_slots,
    unsigned int const *changed_new_slots,
    unsigned int changed_count,
    unsigned int *slot_orders_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *residual_order;
    unsigned int *residual_positions;
    double *residual_scores;
    sixel_kmedoids_point_weight_rank_t *residual_rank;
    double *damage_scores;
    unsigned int *slot_orders;
    unsigned char *slot_dirty;
    uint32_t *slot_generation;
    sixel_kmedoids_point_weight_rank_t *slot_rank_work;
    uint64_t slot_cells64;
    uint32_t slot_generation_id;
    unsigned int delta_threshold;
    unsigned int row_base;
    unsigned int slot;
    unsigned int index;
    unsigned int *order_row;

    status = SIXEL_BAD_ARGUMENT;
    residual_order = NULL;
    residual_positions = NULL;
    residual_scores = NULL;
    residual_rank = NULL;
    damage_scores = NULL;
    slot_orders = NULL;
    slot_dirty = NULL;
    slot_generation = NULL;
    slot_rank_work = NULL;
    slot_cells64 = 0u;
    slot_generation_id = 0u;
    delta_threshold = 0u;
    row_base = 0u;
    slot = 0u;
    index = 0u;
    order_row = NULL;
    if (point_count == 0u || k == 0u
            || nearest_slot_before == NULL || nearest_dist_before == NULL
            || second_dist_before == NULL || nearest_slot_after == NULL
            || nearest_dist_after == NULL || second_dist_after == NULL
            || slot_orders_out == NULL || allocator == NULL) {
        return status;
    }
    if (changed_count > 0u
            && (changed_old_slots == NULL || changed_new_slots == NULL)) {
        return status;
    }

    slot_cells64 = (uint64_t)point_count * (uint64_t)k;
    if (slot_cells64
            > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(unsigned int))) {
        return SIXEL_BAD_ALLOCATION;
    }

    residual_order = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    residual_positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    residual_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    residual_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    damage_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    slot_orders = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)slot_cells64 * sizeof(unsigned int));
    slot_dirty = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned char));
    slot_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(uint32_t));
    slot_rank_work = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    if (residual_order == NULL || residual_positions == NULL
            || residual_scores == NULL || residual_rank == NULL
            || damage_scores == NULL || slot_orders == NULL
            || slot_dirty == NULL || slot_generation == NULL
            || slot_rank_work == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (slot = 0u; slot < k; ++slot) {
        slot_dirty[slot] = 1u;
        slot_generation[slot] = 0u;
    }
    sixel_kmedoids_clarans_slot_generation_next(&slot_generation_id,
                                                slot_generation,
                                                k);
    sixel_kmedoids_clarans_mark_all_slots_dirty(slot_dirty, k);

    /*
     * Build the initial cache from the pre-swap assignment state.
     * The dirty-rebuild path then refreshes only affected slots.
     */
    sixel_kmedoids_eval_order_full_refresh(weights,
                                           nearest_dist_before,
                                           point_count,
                                           residual_order,
                                           residual_positions,
                                           residual_scores,
                                           residual_rank);
    sixel_kmedoids_clarans_damage_full_refresh(weights,
                                               nearest_dist_before,
                                               second_dist_before,
                                               point_count,
                                               damage_scores);
    for (slot = 0u; slot < k; ++slot) {
        order_row = sixel_kmedoids_clarans_get_slot_order_row(
            point_count,
            k,
            slot,
            nearest_slot_before,
            residual_order,
            damage_scores,
            slot_orders,
            slot_dirty,
            slot_generation,
            slot_generation_id,
            slot_rank_work);
        if (order_row == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }

    sixel_kmedoids_eval_order_full_refresh(weights,
                                           nearest_dist_after,
                                           point_count,
                                           residual_order,
                                           residual_positions,
                                           residual_scores,
                                           residual_rank);
    sixel_kmedoids_clarans_damage_full_refresh(weights,
                                               nearest_dist_after,
                                               second_dist_after,
                                               point_count,
                                               damage_scores);
    delta_threshold = point_count / 8u;
    if (delta_threshold < 64u) {
        delta_threshold = 64u;
    }
    if (changed_count > delta_threshold) {
        sixel_kmedoids_clarans_mark_all_slots_dirty(slot_dirty, k);
    } else {
        if (swapped_slot < k) {
            slot_dirty[swapped_slot] = 1u;
        }
        for (index = 0u; index < changed_count; ++index) {
            if (changed_old_slots[index] < k) {
                slot_dirty[changed_old_slots[index]] = 1u;
            }
            if (changed_new_slots[index] < k) {
                slot_dirty[changed_new_slots[index]] = 1u;
            }
        }
    }

    for (slot = 0u; slot < k; ++slot) {
        order_row = sixel_kmedoids_clarans_get_slot_order_row(
            point_count,
            k,
            slot,
            nearest_slot_after,
            residual_order,
            damage_scores,
            slot_orders,
            slot_dirty,
            slot_generation,
            slot_generation_id,
            slot_rank_work);
        if (order_row == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        row_base = slot * point_count;
        for (index = 0u; index < point_count; ++index) {
            slot_orders_out[row_base + index] = order_row[index];
        }
    }
    status = SIXEL_OK;

end:
    if (slot_rank_work != NULL) {
        sixel_allocator_free(allocator, slot_rank_work);
    }
    if (slot_generation != NULL) {
        sixel_allocator_free(allocator, slot_generation);
    }
    if (slot_dirty != NULL) {
        sixel_allocator_free(allocator, slot_dirty);
    }
    if (slot_orders != NULL) {
        sixel_allocator_free(allocator, slot_orders);
    }
    if (damage_scores != NULL) {
        sixel_allocator_free(allocator, damage_scores);
    }
    if (residual_rank != NULL) {
        sixel_allocator_free(allocator, residual_rank);
    }
    if (residual_scores != NULL) {
        sixel_allocator_free(allocator, residual_scores);
    }
    if (residual_positions != NULL) {
        sixel_allocator_free(allocator, residual_positions);
    }
    if (residual_order != NULL) {
        sixel_allocator_free(allocator, residual_order);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_clarans_candidate_batch_best(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    unsigned int k,
    unsigned int candidate,
    unsigned int const *slots,
    unsigned int slot_count,
    unsigned int const *slot_orders,
    double cutoff,
    unsigned int *best_slot_out,
    double *best_cost_out,
    unsigned int *evaluated_pairs_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    uint64_t budget64;
    unsigned int pair_capacity;
    unsigned int pair_mask;
    uint64_t *seen_pairs;
    uint32_t *seen_generation;
    uint32_t seen_generation_id;
    unsigned int cache_size;
    unsigned int *cache_keys;
    double *cache_rows;
    uint32_t *cache_generation;
    uint32_t *cache_row_epoch;
    uint64_t cache_cells64;
    size_t cache_cell_count;
    size_t cache_cell_index;
    unsigned int cache_slot_next;
    uint32_t cache_epoch_next;
    int improved;

    status = SIXEL_BAD_ARGUMENT;
    budget64 = 0u;
    pair_capacity = 0u;
    pair_mask = 0u;
    seen_pairs = NULL;
    seen_generation = NULL;
    seen_generation_id = 1u;
    cache_size = 8u;
    cache_keys = NULL;
    cache_rows = NULL;
    cache_generation = NULL;
    cache_row_epoch = NULL;
    cache_cells64 = 0u;
    cache_cell_count = 0u;
    cache_cell_index = 0u;
    cache_slot_next = 0u;
    cache_epoch_next = 0u;
    improved = 0;
    if (best_slot_out != NULL) {
        *best_slot_out = UINT_MAX;
    }
    if (best_cost_out != NULL) {
        *best_cost_out = cutoff;
    }
    if (evaluated_pairs_out != NULL) {
        *evaluated_pairs_out = 0u;
    }
    if (points == NULL || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL || slots == NULL || slot_count == 0u
            || point_count == 0u || k == 0u || candidate >= point_count
            || allocator == NULL) {
        return status;
    }

    budget64 = (uint64_t)slot_count * 2u;
    if (budget64 > (uint64_t)UINT_MAX) {
        budget64 = (uint64_t)UINT_MAX;
    }
    pair_capacity = sixel_kmedoids_next_power_of_two((unsigned int)budget64);
    pair_mask = pair_capacity - 1u;

    seen_pairs = (uint64_t *)sixel_allocator_malloc(
        allocator,
        (size_t)pair_capacity * sizeof(uint64_t));
    seen_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)pair_capacity * sizeof(uint32_t));
    cache_keys = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_size * sizeof(unsigned int));
    cache_cells64 = (uint64_t)cache_size * (uint64_t)point_count;
    if (cache_cells64 > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(double))) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    cache_rows = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_cells64 * sizeof(double));
    cache_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_cells64 * sizeof(uint32_t));
    cache_row_epoch = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_size * sizeof(uint32_t));
    if (seen_pairs == NULL || seen_generation == NULL
            || cache_keys == NULL || cache_rows == NULL
            || cache_generation == NULL || cache_row_epoch == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (cache_slot_next = 0u; cache_slot_next < pair_capacity;
            ++cache_slot_next) {
        seen_generation[cache_slot_next] = 0u;
    }
    for (cache_slot_next = 0u; cache_slot_next < cache_size;
            ++cache_slot_next) {
        cache_keys[cache_slot_next] = UINT_MAX;
        cache_row_epoch[cache_slot_next] = 0u;
    }
    cache_cell_count = (size_t)cache_cells64;
    for (cache_cell_index = 0u;
            cache_cell_index < cache_cell_count;
            ++cache_cell_index) {
        cache_generation[cache_cell_index] = 0u;
    }
    cache_slot_next = 0u;
    cache_epoch_next = 0u;

    improved = sixel_kmedoids_clarans_evaluate_candidate_slots(
        points,
        weights,
        point_count,
        nearest_slot,
        nearest_dist,
        second_dist,
        candidate,
        slots,
        slot_count,
        k,
        NULL,
        NULL,
        (unsigned int *)slot_orders,
        NULL,
        NULL,
        0u,
        NULL,
        seen_pairs,
        seen_generation,
        seen_generation_id,
        pair_capacity,
        pair_mask,
        cache_size,
        cache_keys,
        cache_rows,
        cache_generation,
        cache_row_epoch,
        &cache_epoch_next,
        &cache_slot_next,
        cutoff,
        0u,
        0,
        best_slot_out,
        best_cost_out,
        evaluated_pairs_out);
    (void)improved;
    status = SIXEL_OK;

end:
    if (cache_row_epoch != NULL) {
        sixel_allocator_free(allocator, cache_row_epoch);
    }
    if (cache_generation != NULL) {
        sixel_allocator_free(allocator, cache_generation);
    }
    if (cache_rows != NULL) {
        sixel_allocator_free(allocator, cache_rows);
    }
    if (cache_keys != NULL) {
        sixel_allocator_free(allocator, cache_keys);
    }
    if (seen_generation != NULL) {
        sixel_allocator_free(allocator, seen_generation);
    }
    if (seen_pairs != NULL) {
        sixel_allocator_free(allocator, seen_pairs);
    }
    return status;
}

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
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_kmedoids_point_weight_rank_t *point_rank;
    sixel_kmedoids_point_weight_rank_t *slot_rank;
    double *slot_error;

    status = SIXEL_BAD_ARGUMENT;
    point_rank = NULL;
    slot_rank = NULL;
    slot_error = NULL;
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = 0u;
    }
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = 0u;
    }
    if (allocator == NULL || nearest_slot == NULL || nearest_dist == NULL
            || hot_points == NULL || hot_point_count_out == NULL
            || hot_slots == NULL || hot_slot_count_out == NULL
            || point_count == 0u || k == 0u) {
        return status;
    }

    if (hot_point_limit == 0u || hot_point_limit > point_count) {
        hot_point_limit = point_count;
    }
    if (hot_slot_limit == 0u || hot_slot_limit > k) {
        hot_slot_limit = k;
    }

    point_rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(sixel_kmedoids_point_weight_rank_t));
    slot_rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(sixel_kmedoids_point_weight_rank_t));
    slot_error = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    if (point_rank == NULL || slot_rank == NULL || slot_error == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    sixel_kmedoids_build_clarans_guided_sets(weights,
                                             point_count,
                                             k,
                                             nearest_slot,
                                             nearest_dist,
                                             flags,
                                             NULL,
                                             0u,
                                             hot_point_limit,
                                             hot_slot_limit,
                                             point_rank,
                                             slot_rank,
                                             slot_error,
                                             hot_points,
                                             hot_point_count_out,
                                             hot_slots,
                                             hot_slot_count_out);
    status = SIXEL_OK;

end:
    if (slot_error != NULL) {
        sixel_allocator_free(allocator, slot_error);
    }
    if (slot_rank != NULL) {
        sixel_allocator_free(allocator, slot_rank);
    }
    if (point_rank != NULL) {
        sixel_allocator_free(allocator, point_rank);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_collect_samples(
    unsigned char const *data,
    unsigned int length,
    unsigned int channels,
    unsigned int pixel_stride,
    int input_is_float32,
    unsigned int histbits,
    unsigned int point_budget,
    unsigned int rare_keep,
    double prune_mass,
    uint32_t seed,
    double **samples_out,
    double **sample_weights_out,
    unsigned int *sample_count_out,
    unsigned int *visible_count_out,
    sixel_allocator_t *allocator)
{
    return sixel_kmedoids_collect_samples(data,
                                          length,
                                          channels,
                                          pixel_stride,
                                          input_is_float32,
                                          NULL,
                                          NULL,
                                          histbits,
                                          point_budget,
                                          rare_keep,
                                          prune_mass,
                                          seed,
                                          samples_out,
                                          sample_weights_out,
                                          sample_count_out,
                                          visible_count_out,
                                          allocator);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_apply_clarans_guided_delta(
    double const *weights,
    unsigned int point_count,
    unsigned int k,
    unsigned int const *nearest_slot_before,
    double const *nearest_dist_before,
    unsigned char const *flags_before,
    unsigned int const *nearest_slot_after,
    double const *nearest_dist_after,
    unsigned char const *flags_after,
    unsigned int hot_point_limit,
    unsigned int hot_slot_limit,
    unsigned int const *changed_points,
    unsigned int changed_count,
    unsigned int old_medoid,
    unsigned int new_medoid,
    unsigned int *hot_points_out,
    unsigned int *hot_point_count_out,
    unsigned int *hot_slots_out,
    unsigned int *hot_slot_count_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_kmedoids_point_weight_rank_t *heap_rank;
    sixel_kmedoids_point_weight_rank_t *sorted_rank;
    sixel_kmedoids_point_weight_rank_t *slot_rank;
    double *slot_error;
    double *residual;
    unsigned int *heap_pos;
    double *slot_delta;
    unsigned int index;
    unsigned int point_index;
    unsigned int hot_point_count;
    int need_refresh;

    status = SIXEL_BAD_ARGUMENT;
    heap_rank = NULL;
    sorted_rank = NULL;
    slot_rank = NULL;
    slot_error = NULL;
    residual = NULL;
    heap_pos = NULL;
    slot_delta = NULL;
    index = 0u;
    point_index = 0u;
    hot_point_count = 0u;
    need_refresh = 0;
    if (hot_point_count_out != NULL) {
        *hot_point_count_out = 0u;
    }
    if (hot_slot_count_out != NULL) {
        *hot_slot_count_out = 0u;
    }
    if (allocator == NULL || point_count == 0u || k == 0u
            || nearest_slot_before == NULL || nearest_dist_before == NULL
            || nearest_slot_after == NULL || nearest_dist_after == NULL
            || flags_after == NULL || hot_points_out == NULL
            || hot_point_count_out == NULL || hot_slots_out == NULL
            || hot_slot_count_out == NULL) {
        return status;
    }
    if (hot_point_limit == 0u || hot_point_limit > point_count) {
        hot_point_limit = point_count;
    }
    if (hot_slot_limit == 0u || hot_slot_limit > k) {
        hot_slot_limit = k;
    }

    heap_rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)hot_point_limit * sizeof(sixel_kmedoids_point_weight_rank_t));
    sorted_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)hot_point_limit
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    slot_rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(sixel_kmedoids_point_weight_rank_t));
    slot_error = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    residual = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    heap_pos = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    slot_delta = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    if (heap_rank == NULL || sorted_rank == NULL || slot_rank == NULL
            || slot_error == NULL || residual == NULL
            || heap_pos == NULL || slot_delta == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    sixel_kmedoids_clarans_guided_full_refresh(
        weights,
        point_count,
        k,
        nearest_slot_before,
        nearest_dist_before,
        flags_before,
        NULL,
        0u,
        hot_point_limit,
        hot_slot_limit,
        heap_rank,
        sorted_rank,
        slot_rank,
        slot_error,
        residual,
        heap_pos,
        hot_points_out,
        &hot_point_count,
        hot_slots_out,
        hot_slot_count_out);

    for (index = 0u; index < k; ++index) {
        slot_delta[index] = 0.0;
    }
    for (index = 0u; index < changed_count; ++index) {
        point_index = changed_points[index];
        if (point_index >= point_count) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (nearest_slot_before[point_index] < k) {
            slot_delta[nearest_slot_before[point_index]] -=
                sixel_kmedoids_weighted_residual(weights,
                                                 point_index,
                                                 nearest_dist_before[
                                                     point_index]);
        }
        if (nearest_slot_after[point_index] < k) {
            slot_delta[nearest_slot_after[point_index]] +=
                sixel_kmedoids_weighted_residual(weights,
                                                 point_index,
                                                 nearest_dist_after[
                                                     point_index]);
        }
    }

    need_refresh = sixel_kmedoids_clarans_guided_apply_delta(
        weights,
        point_count,
        k,
        hot_point_limit,
        hot_slot_limit,
        flags_after,
        nearest_slot_after,
        nearest_dist_after,
        changed_points,
        changed_count,
        old_medoid,
        new_medoid,
        slot_delta,
        heap_rank,
        sorted_rank,
        heap_pos,
        slot_rank,
        slot_error,
        residual,
        hot_points_out,
        &hot_point_count,
        hot_slots_out,
        hot_slot_count_out);
    if (need_refresh) {
        status = SIXEL_FALSE;
        goto end;
    }

    *hot_point_count_out = hot_point_count;
    status = SIXEL_OK;

end:
    if (slot_delta != NULL) {
        sixel_allocator_free(allocator, slot_delta);
    }
    if (heap_pos != NULL) {
        sixel_allocator_free(allocator, heap_pos);
    }
    if (residual != NULL) {
        sixel_allocator_free(allocator, residual);
    }
    if (slot_error != NULL) {
        sixel_allocator_free(allocator, slot_error);
    }
    if (slot_rank != NULL) {
        sixel_allocator_free(allocator, slot_rank);
    }
    if (sorted_rank != NULL) {
        sixel_allocator_free(allocator, sorted_rank);
    }
    if (heap_rank != NULL) {
        sixel_allocator_free(allocator, heap_rank);
    }
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_pam_polish_cost(double const *points,
                                    double const *weights,
                                    unsigned int point_count,
                                    unsigned int const *initial_medoids,
                                    unsigned int k,
                                    double *before_cost_out,
                                    double *after_cost_out,
                                    unsigned int *iterations_out,
                                    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *medoids;
    unsigned int slot;
    double before_cost;
    double after_cost;
    unsigned int iterations;

    status = SIXEL_BAD_ARGUMENT;
    medoids = NULL;
    slot = 0u;
    before_cost = 0.0;
    after_cost = 0.0;
    iterations = 0u;
    if (before_cost_out != NULL) {
        *before_cost_out = 0.0;
    }
    if (after_cost_out != NULL) {
        *after_cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || initial_medoids == NULL || point_count == 0u
            || k == 0u || allocator == NULL) {
        return status;
    }

    medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    if (medoids == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (slot = 0u; slot < k; ++slot) {
        medoids[slot] = initial_medoids[slot];
    }
    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 initial_medoids,
                                 k,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &before_cost);
    status = sixel_kmedoids_run_pam(points,
                                    weights,
                                    point_count,
                                    k,
                                    1u,
                                    initial_medoids,
                                    medoids,
                                    &after_cost,
                                    &iterations,
                                    allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, medoids);
        return status;
    }

    if (before_cost_out != NULL) {
        *before_cost_out = before_cost;
    }
    if (after_cost_out != NULL) {
        *after_cost_out = after_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iterations;
    }
    sixel_allocator_free(allocator, medoids);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_two_step_pam_polish_cost(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *initial_medoids,
    unsigned int k,
    double *before_cost_out,
    double *after_first_cost_out,
    double *after_second_cost_out,
    unsigned int *first_iterations_out,
    unsigned int *second_iterations_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *medoids;
    unsigned int slot;
    double before_cost;
    double after_first_cost;
    double after_second_cost;
    unsigned int first_iterations;
    unsigned int second_iterations;

    status = SIXEL_BAD_ARGUMENT;
    medoids = NULL;
    slot = 0u;
    before_cost = 0.0;
    after_first_cost = 0.0;
    after_second_cost = 0.0;
    first_iterations = 0u;
    second_iterations = 0u;
    if (before_cost_out != NULL) {
        *before_cost_out = 0.0;
    }
    if (after_first_cost_out != NULL) {
        *after_first_cost_out = 0.0;
    }
    if (after_second_cost_out != NULL) {
        *after_second_cost_out = 0.0;
    }
    if (first_iterations_out != NULL) {
        *first_iterations_out = 0u;
    }
    if (second_iterations_out != NULL) {
        *second_iterations_out = 0u;
    }
    if (points == NULL || initial_medoids == NULL || point_count == 0u
            || k == 0u || allocator == NULL) {
        return status;
    }

    medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    if (medoids == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (slot = 0u; slot < k; ++slot) {
        medoids[slot] = initial_medoids[slot];
    }

    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 initial_medoids,
                                 k,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &before_cost);
    status = sixel_kmedoids_run_pam(points,
                                    weights,
                                    point_count,
                                    k,
                                    1u,
                                    initial_medoids,
                                    medoids,
                                    &after_first_cost,
                                    &first_iterations,
                                    allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, medoids);
        return status;
    }
    status = sixel_kmedoids_run_pam(points,
                                    weights,
                                    point_count,
                                    k,
                                    1u,
                                    medoids,
                                    medoids,
                                    &after_second_cost,
                                    &second_iterations,
                                    allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, medoids);
        return status;
    }

    if (before_cost_out != NULL) {
        *before_cost_out = before_cost;
    }
    if (after_first_cost_out != NULL) {
        *after_first_cost_out = after_first_cost;
    }
    if (after_second_cost_out != NULL) {
        *after_second_cost_out = after_second_cost;
    }
    if (first_iterations_out != NULL) {
        *first_iterations_out = first_iterations;
    }
    if (second_iterations_out != NULL) {
        *second_iterations_out = second_iterations;
    }
    sixel_allocator_free(allocator, medoids);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_bandit_select_topk(
    unsigned int const *active_in,
    double const *costs_in,
    unsigned int count,
    unsigned int keep,
    unsigned int *selected_out,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_kmedoids_candidate_rank_t *ranks;
    sixel_kmedoids_candidate_rank_t *heap;
    unsigned int index;

    status = SIXEL_BAD_ARGUMENT;
    ranks = NULL;
    heap = NULL;
    index = 0u;
    if (active_in == NULL || costs_in == NULL || selected_out == NULL
            || allocator == NULL || count == 0u || keep == 0u
            || keep > count) {
        return status;
    }

    ranks = (sixel_kmedoids_candidate_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)count * sizeof(sixel_kmedoids_candidate_rank_t));
    heap = (sixel_kmedoids_candidate_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)keep * sizeof(sixel_kmedoids_candidate_rank_t));
    if (ranks == NULL || heap == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < count; ++index) {
        ranks[index].active_index = active_in[index];
        ranks[index].cost = costs_in[index];
    }
    sixel_kmedoids_candidate_rank_select_topk(ranks, count, keep, heap);
    for (index = 0u; index < keep; ++index) {
        selected_out[index] = ranks[index].active_index;
    }
    status = SIXEL_OK;

end:
    if (heap != NULL) {
        sixel_allocator_free(allocator, heap);
    }
    if (ranks != NULL) {
        sixel_allocator_free(allocator, ranks);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_pick_sample_indices(unsigned int point_count,
                                   unsigned int sample_size,
                                   uint32_t *rng_state,
                                   unsigned int **indices_out,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *indices;
    unsigned int *table;
    unsigned int index;
    unsigned int slot;
    unsigned int pick;
    unsigned int value;
    unsigned int base;
    unsigned int capacity;
    unsigned int mask;
    unsigned int hash_slot;
    uint64_t hash_state;
    int exists;

    status = SIXEL_BAD_ARGUMENT;
    indices = NULL;
    table = NULL;
    index = 0u;
    slot = 0u;
    pick = 0u;
    value = 0u;
    base = 0u;
    capacity = 0u;
    mask = 0u;
    hash_slot = 0u;
    hash_state = 0u;
    exists = 0;

    if (indices_out != NULL) {
        *indices_out = NULL;
    }
    if (indices_out == NULL || allocator == NULL || point_count == 0u
            || sample_size == 0u || sample_size > point_count) {
        return status;
    }

    indices = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(unsigned int));
    if (indices == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    if (sample_size == point_count) {
        for (index = 0u; index < sample_size; ++index) {
            indices[index] = index;
        }
        *indices_out = indices;
        return SIXEL_OK;
    }

    capacity = sixel_kmedoids_next_power_of_two(sample_size * 2u);
    mask = capacity - 1u;
    table = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)capacity * sizeof(unsigned int));
    if (table == NULL) {
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < capacity; ++index) {
        table[index] = UINT_MAX;
    }
    base = point_count - sample_size;

    /* Floyd's sampling without replacement, preserving generation order. */
    for (slot = 0u; slot < sample_size; ++slot) {
        index = base + slot;
        pick = sixel_kmedoids_rng_bounded(rng_state, index + 1u);
        exists = 0;
        hash_state = (uint64_t)pick * 11400714819323198485ULL;
        hash_slot = (unsigned int)(hash_state & (uint64_t)mask);
        for (;;) {
            if (table[hash_slot] == UINT_MAX) {
                break;
            }
            if (table[hash_slot] == pick) {
                exists = 1;
                break;
            }
            hash_slot = (hash_slot + 1u) & mask;
        }

        if (exists) {
            value = index;
        } else {
            value = pick;
        }
        indices[slot] = value;

        hash_state = (uint64_t)value * 11400714819323198485ULL;
        hash_slot = (unsigned int)(hash_state & (uint64_t)mask);
        for (;;) {
            if (table[hash_slot] == UINT_MAX || table[hash_slot] == value) {
                table[hash_slot] = value;
                break;
            }
            hash_slot = (hash_slot + 1u) & mask;
        }
    }

    sixel_allocator_free(allocator, table);
    *indices_out = indices;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_kmedoids_test_pick_sample_indices(unsigned int point_count,
                                        unsigned int sample_size,
                                        uint32_t seed,
                                        unsigned int **indices_out,
                                        sixel_allocator_t *allocator)
{
    uint32_t rng_state;

    rng_state = seed;
    if (rng_state == 0u) {
        rng_state = 1u;
    }
    return sixel_kmedoids_pick_sample_indices(point_count,
                                              sample_size,
                                              &rng_state,
                                              indices_out,
                                              allocator);
}

static SIXELSTATUS
sixel_kmedoids_run_pam(double const *points,
                       double const *weights,
                       unsigned int point_count,
                       unsigned int k,
                       unsigned int max_iterations,
                       unsigned int const *initial_medoids,
                       unsigned int *medoids_out,
                       double *cost_out,
                       unsigned int *iterations_out,
                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *medoids;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    double *slot_adjust;
    unsigned char *flags;
    unsigned int iteration;
    unsigned int slot;
    unsigned int candidate;
    unsigned int point_index;
    unsigned int best_slot;
    unsigned int best_candidate;
    double best_cost;
    double current_cost;
    double candidate_cost;
    double base_delta;
    double owner_delta;
    double distance;
    double nearest;
    double second;
    double weight;

    status = SIXEL_BAD_ARGUMENT;
    medoids = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    slot_adjust = NULL;
    flags = NULL;
    iteration = 0u;
    slot = 0u;
    candidate = 0u;
    point_index = 0u;
    best_slot = 0u;
    best_candidate = 0u;
    best_cost = 0.0;
    current_cost = 0.0;
    candidate_cost = 0.0;
    base_delta = 0.0;
    owner_delta = 0.0;
    distance = 0.0;
    nearest = 0.0;
    second = 0.0;
    weight = 1.0;

    if (cost_out != NULL) {
        *cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || point_count == 0u || k == 0u
            || medoids_out == NULL || allocator == NULL) {
        return status;
    }

    medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    slot_adjust = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    if (medoids == NULL || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL || slot_adjust == NULL
            || flags == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (initial_medoids != NULL) {
        for (slot = 0u; slot < k; ++slot) {
            medoids[slot] = initial_medoids[slot];
        }
        sixel_kmedoids_mark_medoids(flags, medoids, k, point_count);
    } else {
        sixel_kmedoids_choose_initial_deterministic(points,
                                                    weights,
                                                    point_count,
                                                    k,
                                                    medoids,
                                                    flags);
    }

    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 medoids,
                                 k,
                                 nearest_slot,
                                 nearest_dist,
                                 second_dist,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &current_cost);

    for (iteration = 0u; iteration < max_iterations; ++iteration) {
        best_cost = current_cost;
        best_slot = 0u;
        best_candidate = medoids[0u];

        /* FastPAM-style delta scan:
         * evaluate one incoming candidate against all replace slots.
         */
        for (candidate = 0u; candidate < point_count; ++candidate) {
            if (flags[candidate] != 0u) {
                continue;
            }

            for (slot = 0u; slot < k; ++slot) {
                slot_adjust[slot] = 0.0;
            }
            base_delta = 0.0;

            for (point_index = 0u; point_index < point_count; ++point_index) {
                distance = sixel_kmedoids_distance_sq(points,
                                                      point_index,
                                                      candidate);
                nearest = nearest_dist[point_index];
                second = second_dist[point_index];
                weight = 1.0;
                if (weights != NULL) {
                    weight = weights[point_index];
                }
                if (distance < nearest) {
                    base_delta += (distance - nearest) * weight;
                    continue;
                }

                if (distance < second) {
                    owner_delta = (distance - nearest) * weight;
                } else {
                    owner_delta = (second - nearest) * weight;
                }
                slot_adjust[nearest_slot[point_index]] += owner_delta;
            }

            for (slot = 0u; slot < k; ++slot) {
                candidate_cost = current_cost + base_delta + slot_adjust[slot];
                if (candidate_cost < best_cost) {
                    best_cost = candidate_cost;
                    best_slot = slot;
                    best_candidate = candidate;
                }
            }
        }
        if (best_cost + 1.0e-12 >= current_cost) {
            break;
        }

        flags[medoids[best_slot]] = 0u;
        medoids[best_slot] = best_candidate;
        flags[best_candidate] = 1u;
        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     medoids,
                                     k,
                                     nearest_slot,
                                     nearest_dist,
                                     second_dist,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &current_cost);
    }

    for (slot = 0u; slot < k; ++slot) {
        medoids_out[slot] = medoids[slot];
    }
    if (cost_out != NULL) {
        *cost_out = current_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iteration;
    }
    status = SIXEL_OK;

end:
    if (slot_adjust != NULL) {
        sixel_allocator_free(allocator, slot_adjust);
    }
    if (flags != NULL) {
        sixel_allocator_free(allocator, flags);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (medoids != NULL) {
        sixel_allocator_free(allocator, medoids);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_run_clara(double const *points,
                         double const *weights,
                         unsigned int point_count,
                         unsigned int k,
                         unsigned int pam_iterations,
                         unsigned int trials,
                         unsigned int sample_size_override,
                         uint32_t *rng_state,
                         unsigned int *medoids_out,
                         double *cost_out,
                         unsigned int *iterations_out,
                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int sample_size;
    unsigned int trial;
    unsigned int *sample_indices;
    double *sample_points;
    double *sample_weights;
    unsigned int *sample_medoids;
    unsigned char *sample_flags;
    double *sample_nearest;
    unsigned int *candidate_medoids;
    unsigned int sample_index;
    unsigned int slot;
    double sample_cost;
    double candidate_cost;
    double best_cost;
    unsigned int iter_count;
    unsigned int iter_total;

    status = SIXEL_BAD_ARGUMENT;
    sample_size = 0u;
    trial = 0u;
    sample_indices = NULL;
    sample_points = NULL;
    sample_weights = NULL;
    sample_medoids = NULL;
    sample_flags = NULL;
    sample_nearest = NULL;
    candidate_medoids = NULL;
    sample_index = 0u;
    slot = 0u;
    sample_cost = 0.0;
    candidate_cost = 0.0;
    best_cost = 0.0;
    iter_count = 0u;
    iter_total = 0u;

    if (cost_out != NULL) {
        *cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || point_count == 0u || k == 0u || medoids_out == NULL
            || allocator == NULL || rng_state == NULL) {
        return status;
    }

    if (sample_size_override > 0u) {
        sample_size = sample_size_override;
        if (sample_size < k) {
            sample_size = k;
        }
    } else {
        sample_size = 40u * k;
        if (sample_size < 1024u) {
            sample_size = 1024u;
        }
    }
    if (sample_size > point_count) {
        sample_size = point_count;
    }

    sample_points = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * 3u * sizeof(double));
    sample_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(double));
    sample_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    sample_flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(unsigned char));
    sample_nearest = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(double));
    candidate_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    if (sample_points == NULL || sample_weights == NULL
            || sample_medoids == NULL || sample_flags == NULL
            || sample_nearest == NULL || candidate_medoids == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (trials == 0u) {
        trials = 1u;
    }

    for (trial = 0u; trial < trials; ++trial) {
        status = sixel_kmedoids_pick_sample_indices(point_count,
                                                    sample_size,
                                                    rng_state,
                                                    &sample_indices,
                                                    allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        for (sample_index = 0u; sample_index < sample_size; ++sample_index) {
            slot = sample_indices[sample_index];
            sample_points[sample_index * 3u + 0u] = points[slot * 3u + 0u];
            sample_points[sample_index * 3u + 1u] = points[slot * 3u + 1u];
            sample_points[sample_index * 3u + 2u] = points[slot * 3u + 2u];
            sample_weights[sample_index] = weights[slot];
        }

        status = sixel_kmedoids_choose_initial_kpp(sample_points,
                                                   sample_weights,
                                                   sample_size,
                                                   k,
                                                   sample_medoids,
                                                   sample_flags,
                                                   sample_nearest,
                                                   rng_state);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = sixel_kmedoids_run_pam(sample_points,
                                        sample_weights,
                                        sample_size,
                                        k,
                                        pam_iterations,
                                        sample_medoids,
                                        sample_medoids,
                                        &sample_cost,
                                        &iter_count,
                                        allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        iter_total += iter_count;

        for (slot = 0u; slot < k; ++slot) {
            candidate_medoids[slot] = sample_indices[sample_medoids[slot]];
        }

        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     candidate_medoids,
                                     k,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &candidate_cost);
        if (trial == 0u || candidate_cost < best_cost) {
            best_cost = candidate_cost;
            for (slot = 0u; slot < k; ++slot) {
                medoids_out[slot] = candidate_medoids[slot];
            }
        }

        if (sample_indices != NULL) {
            sixel_allocator_free(allocator, sample_indices);
            sample_indices = NULL;
        }

        if (sample_size == point_count) {
            break;
        }
    }

    if (cost_out != NULL) {
        *cost_out = best_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iter_total;
    }
    status = SIXEL_OK;

end:
    if (sample_nearest != NULL) {
        sixel_allocator_free(allocator, sample_nearest);
    }
    if (sample_flags != NULL) {
        sixel_allocator_free(allocator, sample_flags);
    }
    if (sample_indices != NULL) {
        sixel_allocator_free(allocator, sample_indices);
    }
    if (candidate_medoids != NULL) {
        sixel_allocator_free(allocator, candidate_medoids);
    }
    if (sample_medoids != NULL) {
        sixel_allocator_free(allocator, sample_medoids);
    }
    if (sample_weights != NULL) {
        sixel_allocator_free(allocator, sample_weights);
    }
    if (sample_points != NULL) {
        sixel_allocator_free(allocator, sample_points);
    }
    return status;
}

static double *
sixel_kmedoids_clarans_get_candidate_distance_row(
    unsigned int point_count,
    unsigned int candidate,
    unsigned int cache_size,
    unsigned int *cache_keys,
    double *cache_rows,
    uint32_t *cache_generation,
    uint32_t *cache_row_epoch,
    uint32_t *next_epoch_io,
    unsigned int *next_slot_io,
    uint32_t **row_generation_out,
    uint32_t *row_epoch_out)
{
    unsigned int cache_slot;
    unsigned int next_slot;
    unsigned int row_slot;
    uint32_t next_epoch;
    size_t row_offset;
    size_t cell_count;
    double *row;
    uint32_t *row_generation;
    int cache_hit;

    cache_slot = 0u;
    next_slot = 0u;
    row_slot = 0u;
    next_epoch = 0u;
    row_offset = 0u;
    cell_count = 0u;
    row = NULL;
    row_generation = NULL;
    cache_hit = 0;
    if (row_generation_out != NULL) {
        *row_generation_out = NULL;
    }
    if (row_epoch_out != NULL) {
        *row_epoch_out = 0u;
    }
    if (point_count == 0u) {
        return NULL;
    }
    if (cache_size == 0u || cache_keys == NULL || cache_rows == NULL
            || cache_generation == NULL || cache_row_epoch == NULL
            || next_epoch_io == NULL || next_slot_io == NULL) {
        return NULL;
    }
    if (point_count > SIZE_MAX / cache_size) {
        return NULL;
    }
    cell_count = (size_t)point_count * (size_t)cache_size;

    for (cache_slot = 0u; cache_slot < cache_size; ++cache_slot) {
        if (cache_keys[cache_slot] == candidate) {
            row_slot = cache_slot;
            cache_hit = 1;
            break;
        }
    }

    if (!cache_hit) {
        next_slot = *next_slot_io;
        if (next_slot >= cache_size) {
            next_slot = 0u;
        }
        row_slot = next_slot;
        cache_keys[row_slot] = candidate;
        next_slot += 1u;
        if (next_slot >= cache_size) {
            next_slot = 0u;
        }
        *next_slot_io = next_slot;
    }

    if (!cache_hit || cache_row_epoch[row_slot] == 0u) {
        next_epoch = *next_epoch_io + 1u;
        if (next_epoch == 0u) {
            memset(cache_generation, 0, cell_count * sizeof(uint32_t));
            memset(cache_row_epoch,
                   0,
                   (size_t)cache_size * sizeof(uint32_t));
            next_epoch = 1u;
        }
        cache_row_epoch[row_slot] = next_epoch;
        *next_epoch_io = next_epoch;
    }

    row_offset = (size_t)row_slot * (size_t)point_count;
    row = cache_rows + row_offset;
    row_generation = cache_generation + row_offset;
    if (row_generation_out != NULL) {
        *row_generation_out = row_generation;
    }
    if (row_epoch_out != NULL) {
        *row_epoch_out = cache_row_epoch[row_slot];
    }
    return row;
}

static SIXELSTATUS
sixel_kmedoids_run_clarans(double const *points,
                           double const *weights,
                           unsigned int point_count,
                           unsigned int k,
                           unsigned int local_searches,
                           unsigned int neighbors,
                           uint32_t *rng_state,
                           unsigned int const *base_medoids,
                           unsigned int *medoids_out,
                           double *cost_out,
                           unsigned int *iterations_out,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *current_medoids;
    unsigned int *non_medoids;
    unsigned int *eval_order;
    unsigned int *eval_positions;
    double *eval_scores;
    sixel_kmedoids_point_weight_rank_t *eval_rank;
    unsigned int *candidate_cache_keys;
    double *candidate_cache_rows;
    uint32_t *candidate_cache_generation;
    uint32_t *candidate_cache_row_epoch;
    unsigned int *slot_orders;
    unsigned char *slot_dirty;
    uint32_t *slot_generation;
    double *slot_damage_scores;
    sixel_kmedoids_point_weight_rank_t *slot_rank_work;
    unsigned int *probe_slots;
    unsigned char *probe_flags;
    sixel_kmedoids_point_weight_rank_t *guided_point_rank;
    sixel_kmedoids_point_weight_rank_t *guided_point_sorted;
    sixel_kmedoids_point_weight_rank_t *guided_slot_rank;
    double *guided_slot_error;
    double *guided_residual;
    unsigned int *guided_heap_pos;
    unsigned int *guided_changed_points;
    unsigned int *guided_changed_old_slots;
    unsigned int *guided_changed_new_slots;
    double *guided_slot_delta;
    unsigned int *hot_points;
    unsigned int *hot_slots;
    unsigned int *candidate_seen_slots;
    unsigned char *candidate_exhausted;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned int *second_slot;
    uint64_t *seen_pairs;
    uint32_t *seen_generation;
    unsigned char *flags;
    unsigned int local_index;
    unsigned int neighbor_count;
    unsigned int non_count;
    unsigned int slot;
    unsigned int changed_slot_old;
    unsigned int changed_slot_new;
    unsigned int pick;
    unsigned int candidate;
    unsigned int old_medoid;
    unsigned int index;
    unsigned int stale_limit;
    unsigned int eval_budget;
    unsigned int eval_total;
    unsigned int attempt_cap;
    unsigned int attempts;
    unsigned int no_op_attempts;
    unsigned int no_op_limit;
    unsigned int adaptive_limit;
    unsigned int budget_floor;
    unsigned int pair_capacity;
    unsigned int pair_mask;
    unsigned int attempt_cap_pair;
    unsigned int hot_point_limit;
    unsigned int hot_slot_limit;
    unsigned int hot_point_count;
    unsigned int hot_slot_count;
    unsigned int slot_probe_target;
    unsigned int slot_probe_count;
    unsigned int probe_slot_index;
    unsigned int candidate_probe_start;
    unsigned int candidate_probe_count;
    unsigned int evaluated_pairs;
    unsigned int mode_pick;
    unsigned int guided_point_pick;
    unsigned int guided_changed_count;
    unsigned int best_candidate_slot;
    unsigned int delta_threshold;
    unsigned int eval_delta_threshold;
    unsigned int candidate_pool_index;
    unsigned int pool_index;
    unsigned int remaining_candidates;
    unsigned int clarans_cache_size;
    unsigned int cheap_prefix_count;
    unsigned int cache_slot_next;
    uint32_t cache_epoch_next;
    uint32_t slot_generation_id;
    uint32_t seen_generation_id;
    int candidate_improved;
    int candidate_found;
    int candidate_selected;
    int cheap_bound_enabled;
    int need_full_refresh;
    int eval_full_refresh;
    uint64_t budget64;
    uint64_t attempt_cap64;
    uint64_t slot_cells64;
    uint64_t cache_cells64;
    size_t cache_cell_count;
    size_t cache_cell_index;
    double candidate_best_cost;
    double current_cost;
    double best_cost;
    unsigned int iter_total;

    status = SIXEL_BAD_ARGUMENT;
    current_medoids = NULL;
    non_medoids = NULL;
    eval_order = NULL;
    eval_positions = NULL;
    eval_scores = NULL;
    eval_rank = NULL;
    candidate_cache_keys = NULL;
    candidate_cache_rows = NULL;
    candidate_cache_generation = NULL;
    candidate_cache_row_epoch = NULL;
    slot_orders = NULL;
    slot_dirty = NULL;
    slot_generation = NULL;
    slot_damage_scores = NULL;
    slot_rank_work = NULL;
    probe_slots = NULL;
    probe_flags = NULL;
    guided_point_rank = NULL;
    guided_point_sorted = NULL;
    guided_slot_rank = NULL;
    guided_slot_error = NULL;
    guided_residual = NULL;
    guided_heap_pos = NULL;
    guided_changed_points = NULL;
    guided_changed_old_slots = NULL;
    guided_changed_new_slots = NULL;
    guided_slot_delta = NULL;
    hot_points = NULL;
    hot_slots = NULL;
    candidate_seen_slots = NULL;
    candidate_exhausted = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    second_slot = NULL;
    seen_pairs = NULL;
    seen_generation = NULL;
    flags = NULL;
    local_index = 0u;
    neighbor_count = 0u;
    non_count = 0u;
    slot = 0u;
    changed_slot_old = 0u;
    changed_slot_new = 0u;
    pick = 0u;
    candidate = 0u;
    old_medoid = 0u;
    index = 0u;
    stale_limit = 0u;
    eval_budget = 0u;
    eval_total = 0u;
    attempt_cap = 0u;
    attempts = 0u;
    no_op_attempts = 0u;
    no_op_limit = 0u;
    adaptive_limit = 0u;
    budget_floor = 0u;
    pair_capacity = 0u;
    pair_mask = 0u;
    attempt_cap_pair = 0u;
    hot_point_limit = 0u;
    hot_slot_limit = 0u;
    hot_point_count = 0u;
    hot_slot_count = 0u;
    slot_probe_target = 0u;
    slot_probe_count = 0u;
    probe_slot_index = 0u;
    candidate_probe_start = 0u;
    candidate_probe_count = 0u;
    evaluated_pairs = 0u;
    mode_pick = 0u;
    guided_point_pick = 0u;
    guided_changed_count = 0u;
    best_candidate_slot = 0u;
    delta_threshold = 0u;
    eval_delta_threshold = 0u;
    candidate_pool_index = 0u;
    pool_index = 0u;
    remaining_candidates = 0u;
    clarans_cache_size = 0u;
    cheap_prefix_count = 0u;
    cache_slot_next = 0u;
    cache_epoch_next = 0u;
    slot_generation_id = 0u;
    seen_generation_id = 0u;
    candidate_improved = 0;
    candidate_found = 0;
    candidate_selected = 0;
    cheap_bound_enabled = 1;
    need_full_refresh = 0;
    eval_full_refresh = 0;
    budget64 = 0u;
    attempt_cap64 = 0u;
    slot_cells64 = 0u;
    cache_cells64 = 0u;
    cache_cell_count = 0u;
    cache_cell_index = 0u;
    candidate_best_cost = 0.0;
    current_cost = 0.0;
    best_cost = 0.0;
    iter_total = 0u;

    if (cost_out != NULL) {
        *cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || point_count == 0u || k == 0u || rng_state == NULL
            || medoids_out == NULL || allocator == NULL) {
        return status;
    }

    current_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    non_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    eval_order = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    eval_positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    eval_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    eval_rank = (sixel_kmedoids_point_weight_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(sixel_kmedoids_point_weight_rank_t));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    hot_point_limit = 16u * k;
    if (hot_point_limit < 64u) {
        hot_point_limit = 64u;
    }
    if (hot_point_limit > point_count) {
        hot_point_limit = point_count;
    }
    hot_slot_limit = k;
    if (hot_slot_limit > 4u) {
        hot_slot_limit = 4u;
    }
    if (hot_slot_limit == 0u) {
        hot_slot_limit = 1u;
    }
    clarans_cache_size = sixel_kmedoids_clarans_candidate_cache_size(
        point_count,
        k);
    cheap_prefix_count = sixel_kmedoids_clarans_cheap_prefix_count(
        point_count);
    cache_cells64 = (uint64_t)clarans_cache_size * (uint64_t)point_count;
    if (cache_cells64 == 0u
            || cache_cells64
                > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(double))
            || cache_cells64
                > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(uint32_t))) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    candidate_cache_keys = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)clarans_cache_size * sizeof(unsigned int));
    candidate_cache_rows = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_cells64 * sizeof(double));
    candidate_cache_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_cells64 * sizeof(uint32_t));
    candidate_cache_row_epoch = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)clarans_cache_size * sizeof(uint32_t));
    slot_cells64 = (uint64_t)k * (uint64_t)point_count;
    if (slot_cells64 > 0u
            && slot_cells64
                <= ((uint64_t)SIZE_MAX / (uint64_t)sizeof(unsigned int))) {
        slot_orders = (unsigned int *)sixel_allocator_malloc(
            allocator,
            (size_t)slot_cells64 * sizeof(unsigned int));
    }
    slot_dirty = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned char));
    slot_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(uint32_t));
    if (point_count > 0u) {
        slot_damage_scores = (double *)sixel_allocator_malloc(
            allocator,
            (size_t)point_count * sizeof(double));
        slot_rank_work = (sixel_kmedoids_point_weight_rank_t *)
            sixel_allocator_malloc(
                allocator,
                (size_t)point_count
                    * sizeof(sixel_kmedoids_point_weight_rank_t));
    }
    probe_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    probe_flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned char));
    guided_point_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)hot_point_limit
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    guided_point_sorted = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)hot_point_limit
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    guided_slot_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)k * sizeof(sixel_kmedoids_point_weight_rank_t));
    guided_slot_error = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    guided_residual = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    guided_heap_pos = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    guided_changed_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    guided_changed_old_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    guided_changed_new_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    guided_slot_delta = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    hot_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)hot_point_limit * sizeof(unsigned int));
    hot_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)hot_slot_limit * sizeof(unsigned int));
    candidate_seen_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    candidate_exhausted = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    if (current_medoids == NULL || non_medoids == NULL
            || eval_order == NULL || eval_positions == NULL
            || eval_scores == NULL || eval_rank == NULL
            || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL || second_slot == NULL
            || candidate_cache_keys == NULL
            || candidate_cache_rows == NULL
            || candidate_cache_generation == NULL
            || candidate_cache_row_epoch == NULL
            || slot_orders == NULL
            || slot_dirty == NULL || slot_generation == NULL
            || slot_damage_scores == NULL || slot_rank_work == NULL
            || probe_slots == NULL || probe_flags == NULL
            || flags == NULL || guided_point_rank == NULL
            || guided_point_sorted == NULL
            || guided_slot_rank == NULL || guided_slot_error == NULL
            || guided_residual == NULL || guided_heap_pos == NULL
            || guided_changed_points == NULL
            || guided_changed_old_slots == NULL
            || guided_changed_new_slots == NULL
            || guided_slot_delta == NULL
            || hot_points == NULL || hot_slots == NULL
            || candidate_seen_slots == NULL
            || candidate_exhausted == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (index = 0u; index < clarans_cache_size; ++index) {
        candidate_cache_keys[index] = UINT_MAX;
        candidate_cache_row_epoch[index] = 0u;
    }
    for (index = 0u; index < k; ++index) {
        slot_generation[index] = 0u;
        slot_dirty[index] = 1u;
    }
    cache_cell_count = (size_t)cache_cells64;
    for (cache_cell_index = 0u;
            cache_cell_index < cache_cell_count;
            ++cache_cell_index) {
        candidate_cache_generation[cache_cell_index] = 0u;
    }
    cache_slot_next = 0u;
    cache_epoch_next = 0u;

    if (local_searches == 0u) {
        local_searches = 1u;
    }
    if (neighbors == 0u) {
        neighbors = 1u;
    }
    adaptive_limit = 8u * k;
    if (adaptive_limit < 64u) {
        adaptive_limit = 64u;
    }
    stale_limit = neighbors;
    if (stale_limit > adaptive_limit) {
        stale_limit = adaptive_limit;
    }
    if (stale_limit == 0u) {
        stale_limit = 1u;
    }
    budget64 = (uint64_t)32u * (uint64_t)k
             + (uint64_t)(point_count / 4u);
    if (budget64 > (uint64_t)UINT_MAX) {
        budget_floor = UINT_MAX;
    } else {
        budget_floor = (unsigned int)budget64;
    }
    eval_budget = stale_limit;
    if (eval_budget < budget_floor) {
        eval_budget = budget_floor;
    }
    if (eval_budget > neighbors) {
        eval_budget = neighbors;
    }
    if (eval_budget == 0u) {
        eval_budget = 1u;
    }
    attempt_cap64 = (uint64_t)eval_budget * (uint64_t)16u;
    if (attempt_cap64 > (uint64_t)UINT_MAX) {
        attempt_cap = UINT_MAX;
    } else {
        attempt_cap = (unsigned int)attempt_cap64;
    }
    if (attempt_cap < stale_limit) {
        attempt_cap = stale_limit;
    }
    if (attempt_cap == 0u) {
        attempt_cap = 1u;
    }
    no_op_limit = attempt_cap;

    budget64 = (uint64_t)stale_limit * (uint64_t)8u;
    if (budget64 == 0u) {
        budget64 = 8u;
    }
    if (budget64 > (uint64_t)(1u << 30)) {
        budget64 = (uint64_t)(1u << 30);
    }
    pair_capacity = sixel_kmedoids_next_power_of_two((unsigned int)budget64);
    if (pair_capacity < 8u) {
        pair_capacity = 8u;
    }
    pair_mask = pair_capacity - 1u;
    seen_pairs = (uint64_t *)sixel_allocator_malloc(
        allocator,
        (size_t)pair_capacity * sizeof(uint64_t));
    seen_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)pair_capacity * sizeof(uint32_t));
    if (seen_pairs == NULL || seen_generation == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (index = 0u; index < pair_capacity; ++index) {
        seen_generation[index] = 0u;
    }
    seen_generation_id = 1u;

    attempt_cap_pair = pair_capacity;
    if (attempt_cap_pair > UINT_MAX / 4u) {
        attempt_cap_pair = UINT_MAX;
    } else {
        attempt_cap_pair *= 4u;
    }
    if (attempt_cap > attempt_cap_pair) {
        attempt_cap = attempt_cap_pair;
    }
    if (attempt_cap < stale_limit) {
        attempt_cap = stale_limit;
    }
    if (no_op_limit < attempt_cap_pair) {
        no_op_limit = attempt_cap_pair;
    }
    if (no_op_limit == 0u) {
        no_op_limit = 1u;
    }

    eval_delta_threshold = point_count / 8u;
    if (eval_delta_threshold < 64u) {
        eval_delta_threshold = 64u;
    }
    if (weights != NULL) {
        for (index = 0u; index < point_count; ++index) {
            if (weights[index] < 0.0) {
                cheap_bound_enabled = 0;
                break;
            }
        }
    }

    for (local_index = 0u; local_index < local_searches; ++local_index) {
        sixel_kmedoids_seen_pairs_next_generation(&seen_generation_id,
                                                  seen_generation,
                                                  pair_capacity);
        if (local_index == 0u && base_medoids != NULL) {
            for (slot = 0u; slot < k; ++slot) {
                current_medoids[slot] = base_medoids[slot];
            }
            sixel_kmedoids_mark_medoids(flags,
                                        current_medoids,
                                        k,
                                        point_count);
        } else {
            status = sixel_kmedoids_choose_initial_kpp(points,
                                                       weights,
                                                       point_count,
                                                       k,
                                                       current_medoids,
                                                       flags,
                                                       nearest_dist,
                                                       rng_state);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     current_medoids,
                                     k,
                                     nearest_slot,
                                     nearest_dist,
                                     second_dist,
                                     second_slot,
                                     NULL,
                                     NULL,
                                     &current_cost);
        /* Keep an explicit non-medoid pool so CLARANS avoids reject loops. */
        non_count = 0u;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] == 0u) {
                non_medoids[non_count] = index;
                ++non_count;
            }
            candidate_seen_slots[index] = 0u;
            candidate_exhausted[index] = 0u;
        }
        remaining_candidates = non_count;
        sixel_kmedoids_clarans_guided_full_refresh(
            weights,
            point_count,
            k,
            nearest_slot,
            nearest_dist,
            flags,
            non_medoids,
            non_count,
            hot_point_limit,
            hot_slot_limit,
            guided_point_rank,
            guided_point_sorted,
            guided_slot_rank,
            guided_slot_error,
            guided_residual,
            guided_heap_pos,
            hot_points,
            &hot_point_count,
            hot_slots,
            &hot_slot_count);
        sixel_kmedoids_eval_order_full_refresh(weights,
                                               nearest_dist,
                                               point_count,
                                               eval_order,
                                               eval_positions,
                                               eval_scores,
                                               eval_rank);
        sixel_kmedoids_clarans_damage_full_refresh(weights,
                                                   nearest_dist,
                                                   second_dist,
                                                   point_count,
                                                   slot_damage_scores);
        sixel_kmedoids_clarans_slot_generation_next(&slot_generation_id,
                                                    slot_generation,
                                                    k);
        sixel_kmedoids_clarans_mark_all_slots_dirty(slot_dirty, k);

        neighbor_count = 0u;
        eval_total = 0u;
        attempts = 0u;
        no_op_attempts = 0u;
        while (neighbor_count < stale_limit
                && eval_total < eval_budget
                && attempts < attempt_cap
                && no_op_attempts < no_op_limit) {
            if (non_count == 0u) {
                break;
            }
            if (remaining_candidates == 0u) {
                break;
            }

            mode_pick = sixel_kmedoids_rng_bounded(rng_state, 100u);
            pick = sixel_kmedoids_rng_bounded(rng_state, non_count);
            guided_point_pick = sixel_kmedoids_rng_bounded(
                rng_state,
                hot_point_count > 0u ? hot_point_count : 1u);
            candidate_selected = 0;
            candidate = non_medoids[pick];
            if (mode_pick < 75u
                    && hot_point_count > 0u) {
                candidate = hot_points[guided_point_pick % hot_point_count];
                if (candidate >= point_count || flags[candidate] != 0u) {
                    candidate = non_medoids[pick];
                }
            }
            if (candidate < point_count
                    && flags[candidate] == 0u
                    && candidate_exhausted[candidate] == 0u) {
                candidate_selected = 1;
            }
            if (!candidate_selected) {
                candidate_probe_start = sixel_kmedoids_rng_bounded(rng_state,
                                                                    non_count);
                for (candidate_probe_count = 0u;
                        candidate_probe_count < non_count;
                        ++candidate_probe_count) {
                    pool_index = candidate_probe_start + candidate_probe_count;
                    if (pool_index >= non_count) {
                        pool_index -= non_count;
                    }
                    candidate = non_medoids[pool_index];
                    if (candidate >= point_count || flags[candidate] != 0u
                            || candidate_exhausted[candidate] != 0u) {
                        continue;
                    }
                    candidate_selected = 1;
                    break;
                }
            }
            if (!candidate_selected) {
                remaining_candidates = 0u;
                break;
            }

            /*
             * Probe multiple replacement slots per candidate so the same
             * candidate distance row can be reused within one attempt.
             */
            slot_probe_target = sixel_kmedoids_clarans_slot_probe_target(
                k,
                hot_slot_count,
                neighbor_count,
                stale_limit,
                eval_total,
                eval_budget);
            for (index = 0u; index < k; ++index) {
                probe_flags[index] = 0u;
            }
            slot_probe_count = 0u;
            for (probe_slot_index = 0u;
                    probe_slot_index < hot_slot_count
                    && slot_probe_count < slot_probe_target;
                    ++probe_slot_index) {
                slot = hot_slots[probe_slot_index];
                if (slot >= k || probe_flags[slot] != 0u) {
                    continue;
                }
                probe_slots[slot_probe_count] = slot;
                probe_flags[slot] = 1u;
                ++slot_probe_count;
            }
            while (slot_probe_count < slot_probe_target) {
                slot = sixel_kmedoids_rng_bounded(rng_state, k);
                if (probe_flags[slot] != 0u) {
                    continue;
                }
                probe_slots[slot_probe_count] = slot;
                probe_flags[slot] = 1u;
                ++slot_probe_count;
            }
            candidate_improved =
                sixel_kmedoids_clarans_evaluate_candidate_slots(
                    points,
                    weights,
                    point_count,
                    nearest_slot,
                    nearest_dist,
                    second_dist,
                    candidate,
                    probe_slots,
                    slot_probe_count,
                    k,
                    eval_order,
                    slot_damage_scores,
                    slot_orders,
                    slot_dirty,
                    slot_generation,
                    slot_generation_id,
                    slot_rank_work,
                    seen_pairs,
                    seen_generation,
                    seen_generation_id,
                    pair_capacity,
                    pair_mask,
                    clarans_cache_size,
                    candidate_cache_keys,
                    candidate_cache_rows,
                    candidate_cache_generation,
                    candidate_cache_row_epoch,
                    &cache_epoch_next,
                    &cache_slot_next,
                    current_cost,
                    cheap_prefix_count,
                    cheap_bound_enabled,
                    &best_candidate_slot,
                    &candidate_best_cost,
                    &evaluated_pairs);
            iter_total += evaluated_pairs;
            eval_total += evaluated_pairs;
            if (evaluated_pairs == 0u) {
                ++no_op_attempts;
                continue;
            }
            no_op_attempts = 0u;
            ++attempts;
            if (candidate_seen_slots[candidate] < k) {
                candidate_seen_slots[candidate] += evaluated_pairs;
                if (candidate_seen_slots[candidate] >= k) {
                    candidate_seen_slots[candidate] = k;
                    if (candidate_exhausted[candidate] == 0u) {
                        candidate_exhausted[candidate] = 1u;
                        if (remaining_candidates > 0u) {
                            --remaining_candidates;
                        }
                    }
                }
            }
            if (candidate_improved
                    && candidate_best_cost + 1.0e-12 < current_cost) {
                slot = best_candidate_slot;
                old_medoid = current_medoids[slot];
                flags[old_medoid] = 0u;
                current_medoids[slot] = candidate;
                flags[candidate] = 1u;
                candidate_pool_index = pick;
                candidate_found = 0;
                if (candidate_pool_index < non_count
                        && non_medoids[candidate_pool_index] == candidate) {
                    candidate_found = 1;
                } else {
                    for (pool_index = 0u; pool_index < non_count;
                            ++pool_index) {
                        if (non_medoids[pool_index] == candidate) {
                            candidate_pool_index = pool_index;
                            candidate_found = 1;
                            break;
                        }
                    }
                }
                if (candidate_found) {
                    non_medoids[candidate_pool_index] = old_medoid;
                } else {
                    non_count = 0u;
                    for (pool_index = 0u; pool_index < point_count;
                            ++pool_index) {
                        if (flags[pool_index] == 0u) {
                            non_medoids[non_count] = pool_index;
                            ++non_count;
                        }
                    }
                }
                for (pool_index = 0u; pool_index < point_count;
                        ++pool_index) {
                    candidate_seen_slots[pool_index] = 0u;
                    candidate_exhausted[pool_index] = 0u;
                }
                remaining_candidates = non_count;

                sixel_kmedoids_update_assignments_after_swap_ex(
                    points,
                    weights,
                    point_count,
                    current_medoids,
                    k,
                    slot,
                    candidate,
                    nearest_slot,
                    nearest_dist,
                    second_dist,
                    second_slot,
                    &current_cost,
                    guided_changed_points,
                    &guided_changed_count,
                    guided_slot_delta,
                    guided_changed_old_slots,
                    guided_changed_new_slots);

                delta_threshold = point_count / 8u;
                if (delta_threshold < 64u) {
                    delta_threshold = 64u;
                }
                need_full_refresh = 0;
                if (guided_changed_count > delta_threshold) {
                    need_full_refresh = 1;
                }
                if (!need_full_refresh) {
                    need_full_refresh =
                        sixel_kmedoids_clarans_guided_apply_delta(
                            weights,
                            point_count,
                            k,
                            hot_point_limit,
                            hot_slot_limit,
                            flags,
                            nearest_slot,
                            nearest_dist,
                            guided_changed_points,
                            guided_changed_count,
                            old_medoid,
                            candidate,
                            guided_slot_delta,
                            guided_point_rank,
                            guided_point_sorted,
                            guided_heap_pos,
                            guided_slot_rank,
                            guided_slot_error,
                            guided_residual,
                            hot_points,
                            &hot_point_count,
                            hot_slots,
                            &hot_slot_count);
                }
                if (need_full_refresh) {
                    non_count = 0u;
                    for (pool_index = 0u; pool_index < point_count;
                            ++pool_index) {
                        if (flags[pool_index] == 0u) {
                            non_medoids[non_count] = pool_index;
                            ++non_count;
                        }
                    }
                    sixel_kmedoids_clarans_guided_full_refresh(
                        weights,
                        point_count,
                        k,
                        nearest_slot,
                        nearest_dist,
                        flags,
                        non_medoids,
                        non_count,
                        hot_point_limit,
                        hot_slot_limit,
                        guided_point_rank,
                        guided_point_sorted,
                        guided_slot_rank,
                        guided_slot_error,
                        guided_residual,
                        guided_heap_pos,
                        hot_points,
                        &hot_point_count,
                        hot_slots,
                        &hot_slot_count);
                }
                eval_full_refresh = sixel_kmedoids_eval_order_apply_delta(
                    weights,
                    nearest_dist,
                    point_count,
                    guided_changed_points,
                    guided_changed_count,
                    eval_delta_threshold,
                    eval_order,
                    eval_positions,
                    eval_scores,
                    eval_rank);
                if (need_full_refresh || eval_full_refresh
                        || guided_changed_count > delta_threshold) {
                    sixel_kmedoids_clarans_mark_all_slots_dirty(slot_dirty, k);
                    sixel_kmedoids_clarans_damage_full_refresh(
                        weights,
                        nearest_dist,
                        second_dist,
                        point_count,
                        slot_damage_scores);
                } else {
                    if (slot < k) {
                        slot_dirty[slot] = 1u;
                    }
                    for (index = 0u; index < guided_changed_count; ++index) {
                        if (guided_changed_points[index] >= point_count) {
                            continue;
                        }
                        slot_damage_scores[guided_changed_points[index]] =
                            sixel_kmedoids_damage_score(
                                weights,
                                nearest_dist,
                                second_dist,
                                guided_changed_points[index]);
                        changed_slot_old = guided_changed_old_slots[index];
                        changed_slot_new = guided_changed_new_slots[index];
                        if (changed_slot_old < k) {
                            slot_dirty[changed_slot_old] = 1u;
                        }
                        if (changed_slot_new < k) {
                            slot_dirty[changed_slot_new] = 1u;
                        }
                    }
                }
                neighbor_count = 0u;
                attempts = 0u;
                no_op_attempts = 0u;
                sixel_kmedoids_seen_pairs_next_generation(&seen_generation_id,
                                                          seen_generation,
                                                          pair_capacity);
            } else {
                ++neighbor_count;
            }
        }

        if (local_index == 0u || current_cost < best_cost) {
            best_cost = current_cost;
            for (slot = 0u; slot < k; ++slot) {
                medoids_out[slot] = current_medoids[slot];
            }
        }
    }

    if (cost_out != NULL) {
        *cost_out = best_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iter_total;
    }
    status = SIXEL_OK;

end:
    if (probe_flags != NULL) {
        sixel_allocator_free(allocator, probe_flags);
    }
    if (probe_slots != NULL) {
        sixel_allocator_free(allocator, probe_slots);
    }
    if (slot_rank_work != NULL) {
        sixel_allocator_free(allocator, slot_rank_work);
    }
    if (slot_damage_scores != NULL) {
        sixel_allocator_free(allocator, slot_damage_scores);
    }
    if (slot_orders != NULL) {
        sixel_allocator_free(allocator, slot_orders);
    }
    if (candidate_exhausted != NULL) {
        sixel_allocator_free(allocator, candidate_exhausted);
    }
    if (candidate_seen_slots != NULL) {
        sixel_allocator_free(allocator, candidate_seen_slots);
    }
    if (hot_slots != NULL) {
        sixel_allocator_free(allocator, hot_slots);
    }
    if (hot_points != NULL) {
        sixel_allocator_free(allocator, hot_points);
    }
    if (guided_slot_delta != NULL) {
        sixel_allocator_free(allocator, guided_slot_delta);
    }
    if (guided_changed_points != NULL) {
        sixel_allocator_free(allocator, guided_changed_points);
    }
    if (guided_changed_new_slots != NULL) {
        sixel_allocator_free(allocator, guided_changed_new_slots);
    }
    if (guided_changed_old_slots != NULL) {
        sixel_allocator_free(allocator, guided_changed_old_slots);
    }
    if (guided_heap_pos != NULL) {
        sixel_allocator_free(allocator, guided_heap_pos);
    }
    if (guided_residual != NULL) {
        sixel_allocator_free(allocator, guided_residual);
    }
    if (guided_slot_error != NULL) {
        sixel_allocator_free(allocator, guided_slot_error);
    }
    if (guided_slot_rank != NULL) {
        sixel_allocator_free(allocator, guided_slot_rank);
    }
    if (guided_point_sorted != NULL) {
        sixel_allocator_free(allocator, guided_point_sorted);
    }
    if (guided_point_rank != NULL) {
        sixel_allocator_free(allocator, guided_point_rank);
    }
    if (seen_generation != NULL) {
        sixel_allocator_free(allocator, seen_generation);
    }
    if (seen_pairs != NULL) {
        sixel_allocator_free(allocator, seen_pairs);
    }
    if (candidate_cache_rows != NULL) {
        sixel_allocator_free(allocator, candidate_cache_rows);
    }
    if (candidate_cache_row_epoch != NULL) {
        sixel_allocator_free(allocator, candidate_cache_row_epoch);
    }
    if (candidate_cache_generation != NULL) {
        sixel_allocator_free(allocator, candidate_cache_generation);
    }
    if (candidate_cache_keys != NULL) {
        sixel_allocator_free(allocator, candidate_cache_keys);
    }
    if (non_medoids != NULL) {
        sixel_allocator_free(allocator, non_medoids);
    }
    if (flags != NULL) {
        sixel_allocator_free(allocator, flags);
    }
    if (second_slot != NULL) {
        sixel_allocator_free(allocator, second_slot);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (current_medoids != NULL) {
        sixel_allocator_free(allocator, current_medoids);
    }
    if (eval_rank != NULL) {
        sixel_allocator_free(allocator, eval_rank);
    }
    if (eval_scores != NULL) {
        sixel_allocator_free(allocator, eval_scores);
    }
    if (eval_positions != NULL) {
        sixel_allocator_free(allocator, eval_positions);
    }
    if (eval_order != NULL) {
        sixel_allocator_free(allocator, eval_order);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_bandit_prune_candidates(double const *points,
                                       double const *weights,
                                       unsigned int point_count,
                                       unsigned int const *nearest_slot,
                                       double const *nearest_dist,
                                       double const *second_dist,
                                       unsigned int *candidate_slots,
                                       unsigned int *candidate_points,
                                       unsigned int *active,
                                       unsigned int *active_count,
                                       unsigned int batch_limit,
                                       unsigned int cache_size,
                                       unsigned int *cache_keys,
                                       double *cache_rows,
                                       uint32_t *cache_generation,
                                       uint32_t *cache_row_epoch,
                                       uint32_t *cache_epoch_next,
                                       unsigned int *cache_slot_next,
                                       uint32_t *rng_state,
                                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *sample_points;
    sixel_kmedoids_candidate_rank_t *ranks;
    sixel_kmedoids_candidate_rank_t *heap;
    unsigned int batch_size;
    unsigned int batch_ceiling;
    unsigned int index;
    unsigned int keep;
    unsigned int slot;
    unsigned int candidate;
    double *candidate_dist_row;
    uint32_t *candidate_generation_row;
    uint32_t candidate_row_epoch;
    double cost;

    status = SIXEL_BAD_ARGUMENT;
    sample_points = NULL;
    ranks = NULL;
    heap = NULL;
    batch_size = 0u;
    batch_ceiling = 0u;
    index = 0u;
    keep = 0u;
    slot = 0u;
    candidate = 0u;
    candidate_dist_row = NULL;
    candidate_generation_row = NULL;
    candidate_row_epoch = 0u;
    cost = 0.0;

    if (points == NULL || weights == NULL || nearest_slot == NULL
            || nearest_dist == NULL || second_dist == NULL
            || candidate_slots == NULL || candidate_points == NULL
            || active == NULL || active_count == NULL
            || cache_size == 0u || cache_keys == NULL
            || cache_rows == NULL || cache_generation == NULL
            || cache_row_epoch == NULL || cache_epoch_next == NULL
            || cache_slot_next == NULL
            || rng_state == NULL || allocator == NULL) {
        return status;
    }

    if (*active_count <= 4u || point_count == 0u) {
        return SIXEL_OK;
    }
    if (batch_limit == 0u) {
        batch_limit = 64u;
    }
    if (batch_limit < 8u) {
        batch_limit = 8u;
    }
    batch_ceiling = point_count;
    if (batch_ceiling > batch_limit) {
        batch_ceiling = batch_limit;
    }
    if (batch_ceiling == 0u) {
        return SIXEL_OK;
    }

    sample_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)batch_ceiling * sizeof(unsigned int));
    ranks = (sixel_kmedoids_candidate_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)(*active_count) * sizeof(sixel_kmedoids_candidate_rank_t));
    heap = (sixel_kmedoids_candidate_rank_t *)sixel_allocator_malloc(
        allocator,
        (size_t)(*active_count) * sizeof(sixel_kmedoids_candidate_rank_t));
    if (sample_points == NULL || ranks == NULL || heap == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    batch_size = batch_ceiling;
    if (batch_size > 64u) {
        batch_size = 64u;
    }

    while (*active_count > 4u) {
        keep = *active_count / 2u;
        if (keep < 4u) {
            keep = 4u;
        }

        for (index = 0u; index < batch_size; ++index) {
            sample_points[index] = sixel_kmedoids_rng_bounded(rng_state,
                                                              point_count);
        }

        for (index = 0u; index < *active_count; ++index) {
            slot = candidate_slots[active[index]];
            candidate = candidate_points[active[index]];
            candidate_dist_row =
                sixel_kmedoids_clarans_get_candidate_distance_row(
                    point_count,
                    candidate,
                    cache_size,
                    cache_keys,
                    cache_rows,
                    cache_generation,
                    cache_row_epoch,
                    cache_epoch_next,
                    cache_slot_next,
                    &candidate_generation_row,
                    &candidate_row_epoch);
            if (candidate_dist_row == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            cost = sixel_kmedoids_swap_cost_prefix_with_cutoff_row_generation(
                points,
                weights,
                point_count,
                batch_size,
                nearest_slot,
                nearest_dist,
                second_dist,
                slot,
                candidate,
                candidate_dist_row,
                candidate_generation_row,
                candidate_row_epoch,
                sample_points,
                1.0e300,
                NULL);
            ranks[index].active_index = active[index];
            ranks[index].cost = cost;
        }
        sixel_kmedoids_candidate_rank_select_topk(ranks,
                                                  *active_count,
                                                  keep,
                                                  heap);
        for (index = 0u; index < keep; ++index) {
            active[index] = ranks[index].active_index;
        }
        *active_count = keep;

        if (batch_size == batch_ceiling) {
            break;
        }
        if (batch_size > batch_ceiling / 2u) {
            batch_size = batch_ceiling;
        } else {
            batch_size *= 2u;
        }
    }

    status = SIXEL_OK;

end:
    if (heap != NULL) {
        sixel_allocator_free(allocator, heap);
    }
    if (ranks != NULL) {
        sixel_allocator_free(allocator, ranks);
    }
    if (sample_points != NULL) {
        sixel_allocator_free(allocator, sample_points);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_run_banditpam(double const *points,
                             double const *weights,
                             unsigned int point_count,
                             unsigned int k,
                             unsigned int max_iterations,
                             unsigned int candidate_budget,
                             unsigned int batch_limit,
                             uint32_t *rng_state,
                             unsigned int *medoids_io,
                             double *cost_io,
                             unsigned int *iterations_out,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *eval_order;
    unsigned int *eval_positions;
    double *eval_scores;
    sixel_kmedoids_point_weight_rank_t *eval_rank;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned int *second_slot;
    unsigned char *flags;
    unsigned int *non_medoids;
    unsigned int *candidate_slots;
    unsigned int *candidate_points;
    unsigned int *active;
    unsigned int *changed_points;
    sixel_kmedoids_point_weight_rank_t *guided_point_rank;
    sixel_kmedoids_point_weight_rank_t *guided_slot_rank;
    double *guided_slot_error;
    unsigned int *hot_points;
    unsigned int *hot_slots;
    uint64_t *pair_hash;
    uint32_t *pair_generation;
    unsigned int *candidate_cache_keys;
    double *candidate_cache_rows;
    uint32_t *candidate_cache_generation;
    uint32_t *candidate_cache_row_epoch;
    unsigned int iteration;
    unsigned int slot;
    unsigned int index;
    unsigned int non_count;
    unsigned int candidate_cap;
    unsigned int candidate_limit;
    unsigned int candidate_count;
    unsigned int active_count;
    unsigned int hot_point_limit;
    unsigned int hot_slot_limit;
    unsigned int hot_point_count;
    unsigned int hot_slot_count;
    unsigned int pick;
    unsigned int mode_pick;
    unsigned int guided_slot_pick;
    unsigned int guided_point_pick;
    unsigned int slot_uniform;
    unsigned int candidate_uniform;
    unsigned int candidate;
    unsigned int best_slot;
    unsigned int best_candidate;
    unsigned int attempts;
    unsigned int pair_capacity;
    unsigned int pair_mask;
    unsigned int bandit_cache_size;
    unsigned int cache_slot_next;
    unsigned int pair_slot;
    unsigned int probe_count;
    unsigned int changed_count;
    unsigned int eval_delta_threshold;
    uint64_t candidate_total;
    uint64_t cache_cells64;
    size_t cache_cell_index;
    size_t cache_cell_count;
    uint64_t pair_key;
    uint64_t pair_state;
    uint32_t cache_epoch_next;
    uint32_t pair_generation_id;
    uint32_t *candidate_generation_row;
    uint32_t candidate_row_epoch;
    double current_cost;
    double swap_cost;
    double best_cost;
    double *candidate_dist_row;
    int eval_full_refresh;
    int current_assigned;
    int pair_seen;

    status = SIXEL_BAD_ARGUMENT;
    eval_order = NULL;
    eval_positions = NULL;
    eval_scores = NULL;
    eval_rank = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    second_slot = NULL;
    flags = NULL;
    non_medoids = NULL;
    candidate_slots = NULL;
    candidate_points = NULL;
    active = NULL;
    changed_points = NULL;
    guided_point_rank = NULL;
    guided_slot_rank = NULL;
    guided_slot_error = NULL;
    hot_points = NULL;
    hot_slots = NULL;
    pair_hash = NULL;
    pair_generation = NULL;
    candidate_cache_keys = NULL;
    candidate_cache_rows = NULL;
    candidate_cache_generation = NULL;
    candidate_cache_row_epoch = NULL;
    iteration = 0u;
    slot = 0u;
    index = 0u;
    non_count = 0u;
    candidate_cap = 0u;
    candidate_limit = 0u;
    candidate_count = 0u;
    active_count = 0u;
    hot_point_limit = 0u;
    hot_slot_limit = 0u;
    hot_point_count = 0u;
    hot_slot_count = 0u;
    pick = 0u;
    mode_pick = 0u;
    guided_slot_pick = 0u;
    guided_point_pick = 0u;
    slot_uniform = 0u;
    candidate_uniform = 0u;
    candidate = 0u;
    best_slot = 0u;
    best_candidate = 0u;
    attempts = 0u;
    pair_capacity = 0u;
    pair_mask = 0u;
    bandit_cache_size = 0u;
    cache_slot_next = 0u;
    pair_slot = 0u;
    probe_count = 0u;
    changed_count = 0u;
    eval_delta_threshold = 0u;
    candidate_total = 0u;
    cache_cells64 = 0u;
    cache_cell_index = 0u;
    cache_cell_count = 0u;
    pair_key = 0u;
    pair_state = 0u;
    cache_epoch_next = 0u;
    pair_generation_id = 0u;
    candidate_generation_row = NULL;
    candidate_row_epoch = 0u;
    current_cost = 0.0;
    swap_cost = 0.0;
    best_cost = 0.0;
    candidate_dist_row = NULL;
    eval_full_refresh = 0;
    current_assigned = 0;
    pair_seen = 0;

    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || weights == NULL || point_count == 0u || k == 0u
            || medoids_io == NULL || cost_io == NULL || rng_state == NULL
            || allocator == NULL) {
        return status;
    }

    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    non_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    eval_order = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    eval_positions = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    eval_scores = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    eval_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)point_count
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    candidate_cap = candidate_budget;
    if (candidate_cap == 0u) {
        candidate_cap = k * 64u;
        if (candidate_cap < k) {
            candidate_cap = k;
        }
    }
    if (candidate_cap < 8u) {
        candidate_cap = 8u;
    }
    if (candidate_cap > 4096u) {
        candidate_cap = 4096u;
    }
    candidate_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)candidate_cap * sizeof(unsigned int));
    candidate_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)candidate_cap * sizeof(unsigned int));
    active = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)candidate_cap * sizeof(unsigned int));
    changed_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    hot_point_limit = 16u * k;
    if (hot_point_limit < 64u) {
        hot_point_limit = 64u;
    }
    if (hot_point_limit > point_count) {
        hot_point_limit = point_count;
    }
    if (hot_point_limit == 0u) {
        hot_point_limit = 1u;
    }
    hot_slot_limit = k;
    if (hot_slot_limit > 4u) {
        hot_slot_limit = 4u;
    }
    guided_point_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)hot_point_limit
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    guided_slot_rank = (sixel_kmedoids_point_weight_rank_t *)
        sixel_allocator_malloc(
            allocator,
            (size_t)hot_slot_limit
                * sizeof(sixel_kmedoids_point_weight_rank_t));
    guided_slot_error = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    hot_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)hot_point_limit * sizeof(unsigned int));
    hot_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)hot_slot_limit * sizeof(unsigned int));
    pair_capacity = sixel_kmedoids_next_power_of_two(candidate_cap * 2u);
    pair_mask = pair_capacity - 1u;
    pair_hash = (uint64_t *)sixel_allocator_malloc(
        allocator,
        (size_t)pair_capacity * sizeof(uint64_t));
    pair_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)pair_capacity * sizeof(uint32_t));
    bandit_cache_size = 8u;
    cache_cells64 = (uint64_t)bandit_cache_size * (uint64_t)point_count;
    if (cache_cells64 > ((uint64_t)SIZE_MAX / (uint64_t)sizeof(double))) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    candidate_cache_keys = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)bandit_cache_size * sizeof(unsigned int));
    candidate_cache_rows = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_cells64 * sizeof(double));
    candidate_cache_generation = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)cache_cells64 * sizeof(uint32_t));
    candidate_cache_row_epoch = (uint32_t *)sixel_allocator_malloc(
        allocator,
        (size_t)bandit_cache_size * sizeof(uint32_t));
    if (nearest_slot == NULL || nearest_dist == NULL || second_dist == NULL
            || second_slot == NULL || flags == NULL || non_medoids == NULL
            || eval_order == NULL || eval_positions == NULL
            || eval_scores == NULL || eval_rank == NULL
            || candidate_slots == NULL || candidate_points == NULL
            || active == NULL || changed_points == NULL
            || guided_point_rank == NULL
            || guided_slot_rank == NULL || guided_slot_error == NULL
            || hot_points == NULL || hot_slots == NULL
            || pair_hash == NULL
            || pair_generation == NULL
            || candidate_cache_keys == NULL
            || candidate_cache_rows == NULL
            || candidate_cache_generation == NULL
            || candidate_cache_row_epoch == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (index = 0u; index < pair_capacity; ++index) {
        pair_generation[index] = 0u;
    }
    pair_generation_id = 1u;
    for (index = 0u; index < bandit_cache_size; ++index) {
        candidate_cache_keys[index] = UINT_MAX;
        candidate_cache_row_epoch[index] = 0u;
    }
    cache_cell_count = (size_t)cache_cells64;
    for (cache_cell_index = 0u;
            cache_cell_index < cache_cell_count;
            ++cache_cell_index) {
        candidate_cache_generation[cache_cell_index] = 0u;
    }
    cache_epoch_next = 0u;
    cache_slot_next = 0u;
    eval_delta_threshold = point_count / 8u;
    if (eval_delta_threshold < 64u) {
        eval_delta_threshold = 64u;
    }

    current_cost = *cost_io;
    for (iteration = 0u; iteration < max_iterations; ++iteration) {
        sixel_kmedoids_mark_medoids(flags, medoids_io, k, point_count);
        if (!current_assigned) {
            sixel_kmedoids_assign_points(points,
                                         weights,
                                         point_count,
                                         medoids_io,
                                         k,
                                         nearest_slot,
                                         nearest_dist,
                                         second_dist,
                                         second_slot,
                                         NULL,
                                         NULL,
                                         &current_cost);
            sixel_kmedoids_eval_order_full_refresh(weights,
                                                   nearest_dist,
                                                   point_count,
                                                   eval_order,
                                                   eval_positions,
                                                   eval_scores,
                                                   eval_rank);
            current_assigned = 1;
        }

        non_count = 0u;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] == 0u) {
                non_medoids[non_count] = index;
                ++non_count;
            }
        }
        if (non_count == 0u) {
            break;
        }
        sixel_kmedoids_build_clarans_guided_sets(weights,
                                                 point_count,
                                                 k,
                                                 nearest_slot,
                                                 nearest_dist,
                                                 flags,
                                                 non_medoids,
                                                 non_count,
                                                 hot_point_limit,
                                                 hot_slot_limit,
                                                 guided_point_rank,
                                                 guided_slot_rank,
                                                 guided_slot_error,
                                                 hot_points,
                                                 &hot_point_count,
                                                 hot_slots,
                                                 &hot_slot_count);

        candidate_total = (uint64_t)non_count * (uint64_t)k;
        candidate_limit = candidate_cap;
        if ((uint64_t)candidate_limit > candidate_total) {
            candidate_limit = (unsigned int)candidate_total;
        }
        sixel_kmedoids_seen_pairs_next_generation(&pair_generation_id,
                                                  pair_generation,
                                                  pair_capacity);

        candidate_count = 0u;
        attempts = 0u;
        while (candidate_count < candidate_limit
                && attempts < candidate_limit * 16u) {
            mode_pick = sixel_kmedoids_rng_bounded(rng_state, 100u);
            slot_uniform = sixel_kmedoids_rng_bounded(rng_state, k);
            pick = sixel_kmedoids_rng_bounded(rng_state, non_count);
            candidate_uniform = non_medoids[pick];
            slot = slot_uniform;
            candidate = candidate_uniform;
            if (hot_slot_count > 0u) {
                guided_slot_pick = sixel_kmedoids_rng_bounded(
                    rng_state,
                    hot_slot_count);
            } else {
                guided_slot_pick = 0u;
            }
            if (hot_point_count > 0u) {
                guided_point_pick = sixel_kmedoids_rng_bounded(
                    rng_state,
                    hot_point_count);
            } else {
                guided_point_pick = 0u;
            }
            if (mode_pick < 75u
                    && hot_slot_count > 0u
                    && hot_point_count > 0u) {
                slot = hot_slots[guided_slot_pick % hot_slot_count];
                candidate = hot_points[guided_point_pick % hot_point_count];
                if (candidate >= point_count || flags[candidate] != 0u) {
                    slot = slot_uniform;
                    candidate = candidate_uniform;
                }
            }

            pair_key = ((uint64_t)slot << 32u) | (uint64_t)candidate;
            pair_state = pair_key * 11400714819323198485ULL;
            pair_slot = (unsigned int)(pair_state & (uint64_t)pair_mask);
            pair_seen = 0;
            probe_count = 0u;
            for (;;) {
                if (pair_generation[pair_slot] != pair_generation_id) {
                    pair_generation[pair_slot] = pair_generation_id;
                    pair_hash[pair_slot] = pair_key;
                    candidate_slots[candidate_count] = slot;
                    candidate_points[candidate_count] = candidate;
                    ++candidate_count;
                    break;
                }
                if (pair_hash[pair_slot] == pair_key) {
                    pair_seen = 1;
                    break;
                }
                ++probe_count;
                if (probe_count >= pair_capacity) {
                    break;
                }
                pair_slot = (pair_slot + 1u) & pair_mask;
            }
            if (!pair_seen && probe_count >= pair_capacity) {
                ++attempts;
                continue;
            }
            ++attempts;
        }
        if (candidate_count == 0u) {
            break;
        }

        active_count = candidate_count;
        for (index = 0u; index < active_count; ++index) {
            active[index] = index;
        }

        status = sixel_kmedoids_bandit_prune_candidates(points,
                                                        weights,
                                                        point_count,
                                                        nearest_slot,
                                                        nearest_dist,
                                                        second_dist,
                                                        candidate_slots,
                                                        candidate_points,
                                                        active,
                                                        &active_count,
                                                        batch_limit,
                                                        bandit_cache_size,
                                                        candidate_cache_keys,
                                                        candidate_cache_rows,
                                                        candidate_cache_generation,
                                                        candidate_cache_row_epoch,
                                                        &cache_epoch_next,
                                                        &cache_slot_next,
                                                        rng_state,
                                                        allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        best_cost = current_cost;
        best_slot = 0u;
        best_candidate = medoids_io[0u];
        for (index = 0u; index < active_count; ++index) {
            slot = candidate_slots[active[index]];
            candidate = candidate_points[active[index]];
            candidate_dist_row =
                sixel_kmedoids_clarans_get_candidate_distance_row(
                    point_count,
                    candidate,
                    bandit_cache_size,
                    candidate_cache_keys,
                    candidate_cache_rows,
                    candidate_cache_generation,
                    candidate_cache_row_epoch,
                    &cache_epoch_next,
                    &cache_slot_next,
                    &candidate_generation_row,
                    &candidate_row_epoch);
            if (candidate_dist_row != NULL) {
                swap_cost = sixel_kmedoids_swap_cost_with_cutoff_row_generation(
                    points,
                    weights,
                    point_count,
                    nearest_slot,
                    nearest_dist,
                    second_dist,
                    slot,
                    candidate,
                    candidate_dist_row,
                    candidate_generation_row,
                    candidate_row_epoch,
                    eval_order,
                    best_cost,
                    NULL);
            } else {
                swap_cost = sixel_kmedoids_swap_cost_with_cutoff(
                    points,
                    weights,
                    point_count,
                    nearest_slot,
                    nearest_dist,
                    second_dist,
                    slot,
                    candidate,
                    eval_order,
                    best_cost,
                    NULL);
            }
            if (swap_cost < best_cost) {
                best_cost = swap_cost;
                best_slot = slot;
                best_candidate = candidate;
            }
        }

        if (best_cost + 1.0e-12 >= current_cost) {
            break;
        }

        medoids_io[best_slot] = best_candidate;
        sixel_kmedoids_update_assignments_after_swap_ex(points,
                                                        weights,
                                                        point_count,
                                                        medoids_io,
                                                        k,
                                                        best_slot,
                                                        best_candidate,
                                                        nearest_slot,
                                                        nearest_dist,
                                                        second_dist,
                                                        second_slot,
                                                        &current_cost,
                                                        changed_points,
                                                        &changed_count,
                                                        NULL,
                                                        NULL,
                                                        NULL);
        eval_full_refresh = sixel_kmedoids_eval_order_apply_delta(
            weights,
            nearest_dist,
            point_count,
            changed_points,
            changed_count,
            eval_delta_threshold,
            eval_order,
            eval_positions,
            eval_scores,
            eval_rank);
        if (eval_full_refresh) {
            sixel_kmedoids_eval_order_full_refresh(weights,
                                                   nearest_dist,
                                                   point_count,
                                                   eval_order,
                                                   eval_positions,
                                                   eval_scores,
                                                   eval_rank);
        }
    }

    *cost_io = current_cost;
    if (iterations_out != NULL) {
        *iterations_out = iteration;
    }
    status = SIXEL_OK;

end:
    if (hot_slots != NULL) {
        sixel_allocator_free(allocator, hot_slots);
    }
    if (hot_points != NULL) {
        sixel_allocator_free(allocator, hot_points);
    }
    if (guided_slot_error != NULL) {
        sixel_allocator_free(allocator, guided_slot_error);
    }
    if (guided_slot_rank != NULL) {
        sixel_allocator_free(allocator, guided_slot_rank);
    }
    if (guided_point_rank != NULL) {
        sixel_allocator_free(allocator, guided_point_rank);
    }
    if (pair_generation != NULL) {
        sixel_allocator_free(allocator, pair_generation);
    }
    if (candidate_cache_row_epoch != NULL) {
        sixel_allocator_free(allocator, candidate_cache_row_epoch);
    }
    if (candidate_cache_generation != NULL) {
        sixel_allocator_free(allocator, candidate_cache_generation);
    }
    if (candidate_cache_rows != NULL) {
        sixel_allocator_free(allocator, candidate_cache_rows);
    }
    if (candidate_cache_keys != NULL) {
        sixel_allocator_free(allocator, candidate_cache_keys);
    }
    if (eval_rank != NULL) {
        sixel_allocator_free(allocator, eval_rank);
    }
    if (eval_scores != NULL) {
        sixel_allocator_free(allocator, eval_scores);
    }
    if (eval_positions != NULL) {
        sixel_allocator_free(allocator, eval_positions);
    }
    if (eval_order != NULL) {
        sixel_allocator_free(allocator, eval_order);
    }
    if (second_slot != NULL) {
        sixel_allocator_free(allocator, second_slot);
    }
    if (pair_hash != NULL) {
        sixel_allocator_free(allocator, pair_hash);
    }
    if (active != NULL) {
        sixel_allocator_free(allocator, active);
    }
    if (changed_points != NULL) {
        sixel_allocator_free(allocator, changed_points);
    }
    if (candidate_points != NULL) {
        sixel_allocator_free(allocator, candidate_points);
    }
    if (candidate_slots != NULL) {
        sixel_allocator_free(allocator, candidate_slots);
    }
    if (non_medoids != NULL) {
        sixel_allocator_free(allocator, non_medoids);
    }
    if (flags != NULL) {
        sixel_allocator_free(allocator, flags);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    return status;
}

static double
sixel_palette_kmedoids_sum_float_to_byte(double component,
                                         double sample_total,
                                         unsigned int channel,
                                         double const *scale,
                                         double const *offset)
{
    double scaled;

    scaled = component;
    if (scale == NULL || offset == NULL) {
        return scaled;
    }
    if (scale[channel] <= 0.0) {
        return 0.0;
    }

    scaled *= scale[channel];
    scaled += sample_total * offset[channel];
    return scaled;
}

static double
sixel_palette_kmedoids_sum_byte_to_float(double component,
                                         double sample_total,
                                         unsigned int channel,
                                         double const *scale,
                                         double const *offset)
{
    double restored;

    restored = component;
    if (scale == NULL || offset == NULL) {
        return restored;
    }
    if (scale[channel] <= 0.0) {
        return 0.0;
    }

    restored -= sample_total * offset[channel];
    restored /= scale[channel];
    return restored;
}

static SIXELSTATUS
build_palette_kmedoids(unsigned char **result,
                       float **result_float32,
                       unsigned char const *data,
                       unsigned int length,
                       unsigned int depth,
                       unsigned int reqcolors,
                       unsigned int *ncolors,
                       unsigned int *origcolors,
                       int quality_mode,
                       int force_palette,
                       int use_reversible,
                       int final_merge_mode,
                       sixel_allocator_t *allocator,
                       int pixelformat,
                       int treat_input_as_float32,
                       sixel_logger_t *logger,
                       int *job_seq,
                       char const *engine_name,
                       sixel_palette_telemetry_t *telemetry)
{
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int pixel_count;
    unsigned int point_budget;
    unsigned int sample_count;
    unsigned int visible_count;
    double *samples;
    double *sample_weights;
    double *points;
    double *weights;
    unsigned int point_count;
    sixel_kmedoids_algo_t algo;
    sixel_kmedoids_algo_t resolved_algo;
    uint32_t seed;
    uint32_t rng_state;
    unsigned int pam_iterations;
    unsigned int iter_override;
    unsigned int sample_override;
    unsigned int point_budget_override;
    unsigned int clara_trials;
    unsigned int clara_sample_override;
    unsigned int clarans_local;
    unsigned int clarans_neighbors_override;
    unsigned int bandit_iterations;
    unsigned int bandit_candidates;
    unsigned int bandit_batch;
    unsigned int histbits;
    unsigned int rare_keep;
    double prune_mass;
    unsigned int clarans_neighbors;
    unsigned int k;
    unsigned int overshoot;
    unsigned int *medoids;
    unsigned int *base_medoids;
    unsigned int slot;
    unsigned int index;
    unsigned int channel;
    unsigned int iteration_count;
    unsigned int merge_iterations;
    unsigned int total_iterations;
    double solution_cost;
    double *cluster_weights;
    double *cluster_sums;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned long *merge_weights;
    double *merge_sums;
    int resolved_merge;
    int apply_merge;
    int cluster_total;
    double merge_component;
    double restored_component;
    double weight_value;
    double polish_before_cost;
    double first_polish_gain;
    double *final_centers;
    unsigned int final_count;
    unsigned char *palette;
    float *float_palette;
    unsigned int *order;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    double *sort_weights;
    double float32_channel_scale[3];
    double float32_channel_offset[3];
    int input_is_float32;
    int job_init;
    int job_iter;
    int job_merge;
    int job_export;
    char log_detail[128];
    double wall_start;
    double init_stop;
    double iterate_start;
    double iterate_stop;
    double merge_start;
    double merge_stop;
    double export_start;
    double export_stop;
    unsigned int polish_iterations;
    double *distance_cache;
    uint64_t cache_cells;
    uint64_t auto_pam_est;
    uint64_t auto_clara_est;
    uint64_t auto_bandit_est;
    uint64_t auto_tmp_est;
    double cache_distance;
    size_t cache_left;
    size_t cache_right;
    unsigned int auto_sample_size;
    unsigned int auto_candidate_cap;
    int run_second_polish;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_stride = depth;
    pixel_count = 0u;
    point_budget = 0u;
    sample_count = 0u;
    visible_count = 0u;
    samples = NULL;
    sample_weights = NULL;
    points = NULL;
    weights = NULL;
    point_count = 0u;
    algo = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
    resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
    seed = 1u;
    rng_state = 1u;
    pam_iterations = 0u;
    iter_override = 0u;
    sample_override = 0u;
    point_budget_override = 0u;
    clara_trials = 0u;
    clara_sample_override = 0u;
    clarans_local = 0u;
    clarans_neighbors_override = 0u;
    bandit_iterations = 0u;
    bandit_candidates = 0u;
    bandit_batch = 0u;
    histbits = 0u;
    rare_keep = 0u;
    prune_mass = 0.995;
    clarans_neighbors = 0u;
    k = 0u;
    overshoot = 0u;
    medoids = NULL;
    base_medoids = NULL;
    slot = 0u;
    index = 0u;
    channel = 0u;
    iteration_count = 0u;
    merge_iterations = 0u;
    total_iterations = 0u;
    solution_cost = 0.0;
    cluster_weights = NULL;
    cluster_sums = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    merge_weights = NULL;
    merge_sums = NULL;
    resolved_merge = SIXEL_FINAL_MERGE_NONE;
    apply_merge = 0;
    cluster_total = 0;
    merge_component = 0.0;
    restored_component = 0.0;
    weight_value = 0.0;
    polish_before_cost = 0.0;
    first_polish_gain = 0.0;
    final_centers = NULL;
    final_count = 0u;
    palette = NULL;
    float_palette = NULL;
    order = NULL;
    fill = 0u;
    source = 0u;
    swap_temp = 0u;
    sort_weights = NULL;
    float32_channel_scale[0] = 0.0;
    float32_channel_scale[1] = 0.0;
    float32_channel_scale[2] = 0.0;
    float32_channel_offset[0] = 0.0;
    float32_channel_offset[1] = 0.0;
    float32_channel_offset[2] = 0.0;
    input_is_float32 = 0;
    job_init = -1;
    job_iter = -1;
    job_merge = -1;
    job_export = -1;
    log_detail[0] = '\0';
    wall_start = sixel_timer_now();
    init_stop = wall_start;
    iterate_start = wall_start;
    iterate_stop = wall_start;
    merge_start = wall_start;
    merge_stop = wall_start;
    export_start = wall_start;
    export_stop = wall_start;
    polish_iterations = 0u;
    distance_cache = NULL;
    cache_cells = 0u;
    auto_pam_est = 0u;
    auto_clara_est = 0u;
    auto_bandit_est = 0u;
    auto_tmp_est = 0u;
    cache_distance = 0.0;
    cache_left = 0u;
    cache_right = 0u;
    auto_sample_size = 0u;
    auto_candidate_cap = 0u;
    run_second_polish = 0;

    if (result != NULL) {
        *result = NULL;
    }
    if (result_float32 != NULL) {
        *result_float32 = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0u;
    }
    if (origcolors != NULL) {
        *origcolors = 0u;
    }
    if (allocator == NULL || data == NULL || result == NULL) {
        return status;
    }

    input_is_float32 = (treat_input_as_float32
                        && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat));
    if (input_is_float32) {
        for (channel = 0u; channel < 3u; ++channel) {
            float float_minimum;
            float float_maximum;
            double range;

#if HAVE_FLOAT_H
# define SIXEL_KMEDOIDS_FLOAT_BOUND FLT_MAX
#else
# define SIXEL_KMEDOIDS_FLOAT_BOUND 1.0e9f
#endif
            float_minimum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                -SIXEL_KMEDOIDS_FLOAT_BOUND);
            float_maximum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                SIXEL_KMEDOIDS_FLOAT_BOUND);
#undef SIXEL_KMEDOIDS_FLOAT_BOUND
            range = (double)float_maximum - (double)float_minimum;
            if (range <= 0.0) {
                float32_channel_scale[channel] = 0.0;
                float32_channel_offset[channel] = 0.0;
            } else {
                float32_channel_scale[channel] = 255.0 / range;
                float32_channel_offset[channel]
                    = -((double)float_minimum)
                    * float32_channel_scale[channel];
            }
        }
        if (depth == 0u || depth % (unsigned int)sizeof(float) != 0u) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = depth / (unsigned int)sizeof(float);
        pixel_stride = channels * (unsigned int)sizeof(float);
    }

    if (channels != 3u && channels != 4u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixel_stride == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    pixel_count = length / pixel_stride;
    if (pixel_count == 0u) {
        return SIXEL_OK;
    }

    if (reqcolors == 0u) {
        reqcolors = 1u;
    }

    job_init = sixel_palette_kmedoids_log_start(logger,
                                                job_seq,
                                                engine_name,
                                                "palette/init",
                                                "init");

    algo = sixel_get_kmedoids_algo();
    seed = sixel_get_kmedoids_seed();
    if (seed == 0u) {
        seed = 1u;
    }
    rng_state = seed;

    pam_iterations = sixel_kmedoids_quality_pam_iterations(quality_mode);
    iter_override = sixel_get_kmedoids_iter();
    if (iter_override > 0u) {
        pam_iterations = iter_override;
    }
    clara_trials = sixel_kmedoids_quality_clara_trials(quality_mode);
    iter_override = sixel_get_kmedoids_clara_trials();
    if (iter_override > 0u) {
        clara_trials = iter_override;
    }
    clara_sample_override = sixel_get_kmedoids_clara_sample();
    clarans_local = sixel_get_kmedoids_clarans_local();
    clarans_neighbors_override = sixel_get_kmedoids_clarans_neighbors();
    bandit_iterations = sixel_kmedoids_quality_bandit_iterations(quality_mode);
    iter_override = sixel_get_kmedoids_bandit_iter();
    if (iter_override > 0u) {
        bandit_iterations = iter_override;
    }
    bandit_candidates = sixel_get_kmedoids_bandit_candidates();
    bandit_batch = sixel_get_kmedoids_bandit_batch();

    point_budget = sixel_kmedoids_sample_target(reqcolors,
                                                pixel_count,
                                                quality_mode);
    if (point_budget < 64u) {
        point_budget = 64u;
    }
    if (point_budget > 16384u) {
        point_budget = 16384u;
    }
    sample_override = sixel_get_kmedoids_sample();
    point_budget_override = sixel_get_kmedoids_point_budget();
    if (sample_override > 0u && point_budget_override > 0u) {
        point_budget = sample_override;
        if (point_budget_override < point_budget) {
            point_budget = point_budget_override;
        }
    } else if (sample_override > 0u) {
        point_budget = sample_override;
    } else if (point_budget_override > 0u) {
        point_budget = point_budget_override;
    }
    if (point_budget > pixel_count) {
        point_budget = pixel_count;
    }
    if (point_budget == 0u) {
        point_budget = 1u;
    }
    histbits = sixel_get_kmedoids_histbits();
    rare_keep = sixel_get_kmedoids_rare_keep();
    prune_mass = sixel_get_kmedoids_prune_mass();
    status = sixel_kmedoids_collect_samples(data,
                                            length,
                                            channels,
                                            pixel_stride,
                                            input_is_float32,
                                            float32_channel_scale,
                                            float32_channel_offset,
                                            histbits,
                                            point_budget,
                                            rare_keep,
                                            prune_mass,
                                            seed,
                                            &samples,
                                            &sample_weights,
                                            &sample_count,
                                            &visible_count,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors != NULL) {
        *origcolors = visible_count;
    }
    if (sample_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    status = sixel_kmedoids_compress_samples(samples,
                                             sample_weights,
                                             sample_count,
                                             &points,
                                             &weights,
                                             &point_count,
                                             allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (point_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    resolved_algo = algo;
    if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO) {
        k = reqcolors;
        if (k == 0u) {
            k = 1u;
        }
        if (k > point_count) {
            k = point_count;
        }

        auto_sample_size = clara_sample_override;
        if (auto_sample_size > 0u) {
            if (auto_sample_size < k) {
                auto_sample_size = k;
            }
        } else {
            auto_sample_size = 40u * k;
            if (auto_sample_size < 1024u) {
                auto_sample_size = 1024u;
            }
        }
        if (auto_sample_size > point_count) {
            auto_sample_size = point_count;
        }

        auto_candidate_cap = bandit_candidates;
        if (auto_candidate_cap == 0u) {
            auto_candidate_cap = k * 64u;
            if (auto_candidate_cap < k) {
                auto_candidate_cap = k;
            }
        }
        if (auto_candidate_cap < 8u) {
            auto_candidate_cap = 8u;
        }
        if (auto_candidate_cap > 4096u) {
            auto_candidate_cap = 4096u;
        }

        auto_pam_est = sixel_kmedoids_mul_u64_sat(
            (uint64_t)point_count - (uint64_t)k,
            (uint64_t)point_count + (uint64_t)k);

        auto_clara_est = sixel_kmedoids_mul_u64_sat(
            (uint64_t)auto_sample_size - (uint64_t)k,
            (uint64_t)auto_sample_size + (uint64_t)k);
        auto_tmp_est = sixel_kmedoids_mul_u64_sat((uint64_t)point_count,
                                                   (uint64_t)k);
        if (auto_clara_est > UINT64_MAX - auto_tmp_est) {
            auto_clara_est = UINT64_MAX;
        } else {
            auto_clara_est += auto_tmp_est;
        }
        if (clara_trials == 0u) {
            clara_trials = 1u;
        }
        auto_clara_est = sixel_kmedoids_mul_u64_sat(auto_clara_est,
                                                     (uint64_t)clara_trials);

        auto_bandit_est = sixel_kmedoids_mul_u64_sat(
            (uint64_t)point_count,
            (uint64_t)auto_candidate_cap);
        if (bandit_iterations == 0u) {
            bandit_iterations = 1u;
        }
        auto_bandit_est = sixel_kmedoids_mul_u64_sat(
            auto_bandit_est,
            (uint64_t)bandit_iterations);

        if (auto_pam_est <= auto_clara_est
                && auto_pam_est <= auto_bandit_est) {
            resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
        } else if (auto_clara_est <= auto_bandit_est) {
            resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA;
        } else {
            resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM;
        }
    }

    if (point_count <= 2048u) {
        cache_cells = (uint64_t)point_count * (uint64_t)point_count;
        if (cache_cells > 0u
                && cache_cells
                    <= ((uint64_t)SIZE_MAX / (uint64_t)sizeof(double))) {
            distance_cache = (double *)sixel_allocator_malloc(
                allocator,
                (size_t)cache_cells * sizeof(double));
        }
        if (distance_cache != NULL) {
            for (index = 0u; index < point_count; ++index) {
                for (slot = index; slot < point_count; ++slot) {
                    if (index == slot) {
                        cache_distance = 0.0;
                    } else {
                        cache_distance = sixel_kmedoids_distance_sq(points,
                                                                    index,
                                                                    slot);
                    }
                    cache_left = (size_t)index * (size_t)point_count
                               + (size_t)slot;
                    cache_right = (size_t)slot * (size_t)point_count
                                + (size_t)index;
                    distance_cache[cache_left] = cache_distance;
                    distance_cache[cache_right] = cache_distance;
                }
            }
            sixel_kmedoids_set_distance_cache(distance_cache, point_count);
        }
    }

    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    overshoot = reqcolors;
    if (apply_merge) {
        sixel_final_merge_load_env();
        overshoot = sixel_final_merge_target(reqcolors, resolved_merge);
    }
    if (overshoot > point_count) {
        overshoot = point_count;
    }
    if (overshoot == 0u) {
        overshoot = 1u;
    }
    k = overshoot;

    medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    base_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    if (medoids == NULL || base_medoids == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    init_stop = sixel_timer_now();
    iterate_start = init_stop;
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "samples=%u unique=%u k=%u algo=%s/%s seed=%u "
                                "histbits=%u budget=%u prune=%.3f",
                                sample_count,
                                point_count,
                                k,
                                sixel_kmedoids_algo_to_string(algo),
                                sixel_kmedoids_algo_to_string(resolved_algo),
                                seed,
                                histbits,
                                point_budget,
                                prune_mass);
    sixel_palette_kmedoids_log_finish(logger,
                                      job_init,
                                      engine_name,
                                      "palette/init",
                                      "init",
                                      log_detail);

    job_iter = sixel_palette_kmedoids_log_start(logger,
                                                job_seq,
                                                engine_name,
                                                "palette/iterate",
                                                "iterate");

    if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_PAM) {
        status = sixel_kmedoids_run_pam(points,
                                        weights,
                                        point_count,
                                        k,
                                        pam_iterations,
                                        NULL,
                                        medoids,
                                        &solution_cost,
                                        &iteration_count,
                                        allocator);
        total_iterations = iteration_count;
    } else if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA) {
        status = sixel_kmedoids_run_clara(points,
                                          weights,
                                          point_count,
                                          k,
                                          pam_iterations,
                                          clara_trials,
                                          clara_sample_override,
                                          &rng_state,
                                          medoids,
                                          &solution_cost,
                                          &iteration_count,
                                          allocator);
        total_iterations = iteration_count;
    } else if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS) {
        status = sixel_kmedoids_run_clara(points,
                                          weights,
                                          point_count,
                                          k,
                                          pam_iterations,
                                          clara_trials,
                                          clara_sample_override,
                                          &rng_state,
                                          base_medoids,
                                          &solution_cost,
                                          &iteration_count,
                                          allocator);
        total_iterations = iteration_count;
        if (SIXEL_SUCCEEDED(status)) {
            if (clarans_local == 0u) {
                clarans_local = clara_trials;
            }
            if (clarans_neighbors_override > 0u) {
                clarans_neighbors = clarans_neighbors_override;
            } else if (point_count > k) {
                uint64_t neigh64;

                neigh64 = (uint64_t)point_count
                        * (uint64_t)(point_count - k);
                clarans_neighbors = 250u * k;
                if (neigh64 < (uint64_t)clarans_neighbors) {
                    clarans_neighbors = (unsigned int)neigh64;
                }
            } else {
                clarans_neighbors = 1u;
            }
            status = sixel_kmedoids_run_clarans(points,
                                                weights,
                                                point_count,
                                                k,
                                                clarans_local,
                                                clarans_neighbors,
                                                &rng_state,
                                                base_medoids,
                                                medoids,
                                                &solution_cost,
                                                &iteration_count,
                                                allocator);
            total_iterations += iteration_count;
        }
    } else {
        status = sixel_kmedoids_run_clara(points,
                                          weights,
                                          point_count,
                                          k,
                                          pam_iterations,
                                          clara_trials,
                                          clara_sample_override,
                                          &rng_state,
                                          medoids,
                                          &solution_cost,
                                          &iteration_count,
                                          allocator);
        total_iterations = iteration_count;
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_kmedoids_run_banditpam(points,
                                                  weights,
                                                  point_count,
                                                  k,
                                                  bandit_iterations,
                                                  bandit_candidates,
                                                  bandit_batch,
                                                  &rng_state,
                                                  medoids,
                                                  &solution_cost,
                                                  &iteration_count,
                                                  allocator);
            total_iterations += iteration_count;
        }
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /*
     * Apply a conservative PAM polish on small/medium problems.
     * First step keeps existing behavior, and a second step is enabled only
     * for high/full quality when the first step actually improves cost.
     */
    if (resolved_algo != SIXEL_PALETTE_KMEDOIDS_ALGO_PAM
            && quality_mode != SIXEL_QUALITY_LOW
            && k <= 128u
            && point_count <= 8192u) {
        for (index = 0u; index < k; ++index) {
            base_medoids[index] = medoids[index];
        }
        polish_before_cost = solution_cost;
        status = sixel_kmedoids_run_pam(points,
                                        weights,
                                        point_count,
                                        k,
                                        1u,
                                        medoids,
                                        medoids,
                                        &solution_cost,
                                        &polish_iterations,
                                        allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        total_iterations += polish_iterations;
        if (solution_cost > polish_before_cost + 1.0e-12) {
            for (index = 0u; index < k; ++index) {
                medoids[index] = base_medoids[index];
            }
            solution_cost = polish_before_cost;
        }
        first_polish_gain = polish_before_cost - solution_cost;
        run_second_polish = 0;
        if (first_polish_gain > 1.0e-12
                && (quality_mode == SIXEL_QUALITY_HIGH
                    || quality_mode == SIXEL_QUALITY_FULL)
                && k <= 64u
                && point_count <= 4096u) {
            run_second_polish = 1;
        }
        if (run_second_polish) {
            for (index = 0u; index < k; ++index) {
                base_medoids[index] = medoids[index];
            }
            polish_before_cost = solution_cost;
            status = sixel_kmedoids_run_pam(points,
                                            weights,
                                            point_count,
                                            k,
                                            1u,
                                            medoids,
                                            medoids,
                                            &solution_cost,
                                            &polish_iterations,
                                            allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            total_iterations += polish_iterations;
            if (solution_cost > polish_before_cost + 1.0e-12) {
                for (index = 0u; index < k; ++index) {
                    medoids[index] = base_medoids[index];
                }
                solution_cost = polish_before_cost;
            }
        }
    }

    iterate_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "algo=%s iter=%u cost=%.4f",
                                sixel_kmedoids_algo_to_string(resolved_algo),
                                total_iterations,
                                solution_cost);
    sixel_palette_kmedoids_log_finish(logger,
                                      job_iter,
                                      engine_name,
                                      "palette/iterate",
                                      "iterate",
                                      log_detail);

    merge_start = iterate_stop;
    merge_stop = iterate_stop;
    final_centers = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    cluster_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    cluster_sums = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    if (final_centers == NULL || cluster_weights == NULL || cluster_sums == NULL
            || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 medoids,
                                 k,
                                 nearest_slot,
                                 nearest_dist,
                                 second_dist,
                                 NULL,
                                 cluster_weights,
                                 cluster_sums,
                                 &solution_cost);

    for (slot = 0u; slot < k; ++slot) {
        final_centers[slot * 3u + 0u] = points[medoids[slot] * 3u + 0u];
        final_centers[slot * 3u + 1u] = points[medoids[slot] * 3u + 1u];
        final_centers[slot * 3u + 2u] = points[medoids[slot] * 3u + 2u];
    }
    final_count = k;

    if (apply_merge && k > reqcolors) {
        merge_start = sixel_timer_now();
        job_merge = sixel_palette_kmedoids_log_start(logger,
                                                     job_seq,
                                                     engine_name,
                                                     "palette/merge",
                                                     "merge");
        merge_weights = (unsigned long *)sixel_allocator_malloc(
            allocator,
            (size_t)k * sizeof(unsigned long));
        merge_sums = (double *)sixel_allocator_malloc(
            allocator,
            (size_t)k * 3u * sizeof(double));
        if (merge_weights == NULL || merge_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        for (slot = 0u; slot < k; ++slot) {
            weight_value = cluster_weights[slot];
            if (weight_value <= 0.0) {
                merge_weights[slot] = 1ul;
                merge_sums[slot * 3u + 0u] = 0.0;
                merge_sums[slot * 3u + 1u] = 0.0;
                merge_sums[slot * 3u + 2u] = 0.0;
                continue;
            }
            if (weight_value > (double)ULONG_MAX) {
                merge_weights[slot] = ULONG_MAX;
            } else {
                merge_weights[slot] = (unsigned long)(weight_value + 0.5);
                if (merge_weights[slot] == 0ul) {
                    merge_weights[slot] = 1ul;
                }
            }
            for (channel = 0u; channel < 3u; ++channel) {
                merge_component = cluster_sums[slot * 3u + channel]
                    / cluster_weights[slot];
                if (input_is_float32) {
                    merge_component = sixel_palette_kmedoids_sum_float_to_byte(
                        merge_component,
                        1.0,
                        channel,
                        float32_channel_scale,
                        float32_channel_offset);
                }
                if (merge_component < 0.0) {
                    merge_component = 0.0;
                }
                if (merge_component > 255.0) {
                    merge_component = 255.0;
                }
                merge_sums[slot * 3u + channel]
                    = merge_component * (double)merge_weights[slot];
            }
        }

        cluster_total = sixel_palette_apply_merge(merge_weights,
                                                  merge_sums,
                                                  3u,
                                                  (int)k,
                                                  (int)reqcolors,
                                                  resolved_merge,
                                                  use_reversible,
                                                  pixelformat,
                                                  allocator);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > reqcolors) {
            cluster_total = (int)reqcolors;
        }
        final_count = (unsigned int)cluster_total;
        if (final_count == 0u) {
            final_count = 1u;
        }

        for (slot = 0u; slot < final_count; ++slot) {
            weight_value = (double)merge_weights[slot];
            if (weight_value <= 0.0) {
                weight_value = 1.0;
            }
            cluster_weights[slot] = weight_value;
            for (channel = 0u; channel < 3u; ++channel) {
                restored_component = merge_sums[slot * 3u + channel]
                    / weight_value;
                if (input_is_float32) {
                    restored_component
                        = sixel_palette_kmedoids_sum_byte_to_float(
                            restored_component,
                            1.0,
                            channel,
                            float32_channel_scale,
                            float32_channel_offset);
                }
                final_centers[slot * 3u + channel] = restored_component;
            }
        }
        merge_iterations = 1u;
        merge_stop = sixel_timer_now();
        (void)sixel_compat_snprintf(log_detail,
                                    sizeof(log_detail),
                                    "clusters=%u merge=%d",
                                    final_count,
                                    resolved_merge);
        sixel_palette_kmedoids_log_finish(logger,
                                          job_merge,
                                          engine_name,
                                          "palette/merge",
                                          "merge",
                                          log_detail);
    }

    export_start = sixel_timer_now();
    job_export = sixel_palette_kmedoids_log_start(logger,
                                                  job_seq,
                                                  engine_name,
                                                  "palette/export",
                                                  "export");

    palette = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)final_count * 3u);
    if (palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (result_float32 != NULL && input_is_float32 && final_count > 0u) {
        float_palette = (float *)sixel_allocator_malloc(
            allocator,
            (size_t)final_count * 3u * sizeof(float));
        if (float_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    for (slot = 0u; slot < final_count; ++slot) {
        for (channel = 0u; channel < 3u; ++channel) {
            restored_component = final_centers[slot * 3u + channel];
            if (float_palette != NULL) {
                float_palette[slot * 3u + channel]
                    = sixel_pixelformat_float_channel_clamp(
                        pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            if (input_is_float32) {
                restored_component
                    = (double)sixel_pixelformat_float_channel_to_byte(
                        pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            if (restored_component < 0.0) {
                restored_component = 0.0;
            }
            if (restored_component > 255.0) {
                restored_component = 255.0;
            }
            palette[slot * 3u + channel]
                = (unsigned char)(restored_component + 0.5);
        }
    }

    if (force_palette && final_count < reqcolors && final_count > 0u) {
        unsigned char *grown_palette;
        float *grown_float;

        grown_palette = NULL;
        grown_float = NULL;
        grown_palette = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)reqcolors * 3u);
        sort_weights = (double *)sixel_allocator_malloc(
            allocator,
            (size_t)final_count * sizeof(double));
        order = (unsigned int *)sixel_allocator_malloc(
            allocator,
            (size_t)final_count * sizeof(unsigned int));
        if (grown_palette == NULL || sort_weights == NULL || order == NULL) {
            if (grown_palette != NULL) {
                sixel_allocator_free(allocator, grown_palette);
            }
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (float_palette != NULL) {
            grown_float = (float *)sixel_allocator_malloc(
                allocator,
                (size_t)reqcolors * 3u * sizeof(float));
            if (grown_float == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_allocator_free(allocator, grown_palette);
                goto end;
            }
        }

        for (index = 0u; index < final_count * 3u; ++index) {
            grown_palette[index] = palette[index];
        }
        for (index = 0u; index < final_count; ++index) {
            order[index] = index;
            sort_weights[index] = cluster_weights[index];
        }
        for (index = 0u; index + 1u < final_count; ++index) {
            for (slot = index + 1u; slot < final_count; ++slot) {
                if (sort_weights[order[slot]] > sort_weights[order[index]]) {
                    swap_temp = order[index];
                    order[index] = order[slot];
                    order[slot] = swap_temp;
                }
            }
        }
        if (grown_float != NULL) {
            for (index = 0u; index < final_count * 3u; ++index) {
                grown_float[index] = float_palette[index];
            }
        }

        fill = final_count;
        source = 0u;
        while (fill < reqcolors) {
            slot = order[source];
            for (channel = 0u; channel < 3u; ++channel) {
                grown_palette[fill * 3u + channel]
                    = palette[slot * 3u + channel];
                if (grown_float != NULL) {
                    grown_float[fill * 3u + channel]
                        = float_palette[slot * 3u + channel];
                }
            }
            ++fill;
            ++source;
            if (source >= final_count) {
                source = 0u;
            }
        }

        sixel_allocator_free(allocator, palette);
        palette = grown_palette;
        if (float_palette != NULL) {
            sixel_allocator_free(allocator, float_palette);
            float_palette = grown_float;
        }
        final_count = reqcolors;
    }

    *result = palette;
    palette = NULL;
    if (result_float32 != NULL) {
        *result_float32 = float_palette;
        float_palette = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = final_count;
    }

    export_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "colors=%u",
                                final_count);
    sixel_palette_kmedoids_log_finish(logger,
                                      job_export,
                                      engine_name,
                                      "palette/export",
                                      "export",
                                      log_detail);

    status = SIXEL_OK;

end:
    sixel_kmedoids_set_distance_cache(NULL, 0u);
    if (telemetry != NULL) {
        double now;
        double init_span;
        double iterate_span;
        double merge_span;
        double export_span;

        now = sixel_timer_now();
        if (init_stop < wall_start) {
            init_stop = now;
        }
        if (iterate_stop < iterate_start) {
            iterate_stop = init_stop;
        }
        if (merge_stop < merge_start) {
            merge_stop = iterate_stop;
        }
        if (export_stop < export_start) {
            export_stop = now;
        }

        init_span = init_stop - wall_start;
        if (init_span < 0.0) {
            init_span = 0.0;
        }
        iterate_span = iterate_stop - iterate_start;
        if (iterate_span < 0.0) {
            iterate_span = 0.0;
        }
        merge_span = merge_stop - merge_start;
        if (merge_span < 0.0) {
            merge_span = 0.0;
        }
        export_span = export_stop - export_start;
        if (export_span < 0.0) {
            export_span = 0.0;
        }

        telemetry->init_ms = init_span * 1000.0;
        telemetry->iterate_ms = iterate_span * 1000.0;
        telemetry->merge_ms = merge_span * 1000.0;
        telemetry->export_ms = export_span * 1000.0;
        telemetry->iterate_count = total_iterations;
        telemetry->merge_iterate_count = merge_iterations;
        telemetry->merge_mode = (apply_merge && k > reqcolors)
            ? resolved_merge
            : SIXEL_FINAL_MERGE_NONE;
    }

    if (float_palette != NULL) {
        sixel_allocator_free(allocator, float_palette);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (sort_weights != NULL) {
        sixel_allocator_free(allocator, sort_weights);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (merge_sums != NULL) {
        sixel_allocator_free(allocator, merge_sums);
    }
    if (merge_weights != NULL) {
        sixel_allocator_free(allocator, merge_weights);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weights != NULL) {
        sixel_allocator_free(allocator, cluster_weights);
    }
    if (final_centers != NULL) {
        sixel_allocator_free(allocator, final_centers);
    }
    if (base_medoids != NULL) {
        sixel_allocator_free(allocator, base_medoids);
    }
    if (medoids != NULL) {
        sixel_allocator_free(allocator, medoids);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    if (sample_weights != NULL) {
        sixel_allocator_free(allocator, sample_weights);
    }
    if (samples != NULL) {
        sixel_allocator_free(allocator, samples);
    }
    if (distance_cache != NULL) {
        sixel_allocator_free(allocator, distance_cache);
    }
    return status;
}

static SIXELSTATUS
sixel_palette_build_kmedoids_internal(sixel_palette_t *palette,
                                      unsigned char const *data,
                                      unsigned int length,
                                      int pixelformat,
                                      sixel_allocator_t *allocator,
                                      sixel_logger_t *logger,
                                      int *job_seq,
                                      char const *engine_name,
                                      int treat_input_as_float32,
                                      sixel_palette_telemetry_t *telemetry)
{
    SIXELSTATUS status;
    sixel_allocator_t *work_allocator;
    unsigned char *entries;
    float *entries_float32;
    unsigned int ncolors;
    unsigned int origcolors;
    unsigned int input_depth;
    unsigned int entry_depth;
    int depth_result;
    size_t payload_size;

    status = SIXEL_BAD_ARGUMENT;
    work_allocator = allocator;
    entries = NULL;
    entries_float32 = NULL;
    ncolors = 0u;
    origcolors = 0u;
    input_depth = 0u;
    entry_depth = 0u;
    depth_result = 0;
    payload_size = 0u;

    if (palette == NULL) {
        return status;
    }
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return status;
    }

    depth_result = sixel_helper_compute_depth(pixelformat);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kmedoids: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    input_depth = (unsigned int)depth_result;

    depth_result = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kmedoids: rgb888 depth lookup failed.");
        return SIXEL_BAD_ARGUMENT;
    }
    entry_depth = (unsigned int)depth_result;

    status = build_palette_kmedoids(&entries,
                                    &entries_float32,
                                    data,
                                    length,
                                    input_depth,
                                    palette->requested_colors,
                                    &ncolors,
                                    &origcolors,
                                    palette->quality_mode,
                                    palette->force_palette,
                                    palette->use_reversible,
                                    palette->final_merge_mode,
                                    work_allocator,
                                    pixelformat,
                                    treat_input_as_float32,
                                    logger,
                                    job_seq,
                                    engine_name,
                                    telemetry);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (palette->use_reversible && entries != NULL) {
        sixel_palette_reversible_palette(entries,
                                         ncolors,
                                         SIXEL_PIXELFORMAT_RGB888);
    }

    payload_size = (size_t)ncolors * (size_t)entry_depth;
    status = sixel_palette_resize(palette,
                                  ncolors,
                                  (int)entry_depth,
                                  work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (payload_size > 0u) {
        if (palette->entries == NULL || entries == NULL) {
            sixel_helper_set_additional_message(
                "sixel_palette_build_kmedoids: palette payload is missing.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(palette->entries, entries, payload_size);
    }
    palette->entry_count = ncolors;
    palette->original_colors = origcolors;
    palette->depth = (int)entry_depth;

    if (entries_float32 != NULL) {
        status = sixel_palette_set_entries_float32(
            palette,
            entries_float32,
            ncolors,
            (int)(3u * (unsigned int)sizeof(float)),
            work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        status = sixel_palette_set_entries_float32(palette,
                                                   NULL,
                                                   0u,
                                                   0,
                                                   work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    if (entries_float32 != NULL) {
        sixel_allocator_free(work_allocator, entries_float32);
    }
    if (entries != NULL) {
        sixel_allocator_free(work_allocator, entries);
    }
    return status;
}

SIXELSTATUS
sixel_palette_build_kmedoids(sixel_palette_t *palette,
                             unsigned char const *data,
                             unsigned int length,
                             int pixelformat,
                             sixel_allocator_t *allocator,
                             sixel_logger_t *logger,
                             int *job_seq,
                             char const *engine_name,
                             sixel_palette_telemetry_t *telemetry)
{
    return sixel_palette_build_kmedoids_internal(palette,
                                                 data,
                                                 length,
                                                 pixelformat,
                                                 allocator,
                                                 logger,
                                                 job_seq,
                                                 engine_name,
                                                 0,
                                                 telemetry);
}

SIXELSTATUS
sixel_palette_build_kmedoids_float32(sixel_palette_t *palette,
                                     float const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     sixel_logger_t *logger,
                                     int *job_seq,
                                     char const *engine_name,
                                     sixel_palette_telemetry_t *telemetry)
{
    return sixel_palette_build_kmedoids_internal(palette,
                                                 (unsigned char const *)data,
                                                 length,
                                                 pixelformat,
                                                 allocator,
                                                 logger,
                                                 job_seq,
                                                 engine_name,
                                                 1,
                                                 telemetry);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
