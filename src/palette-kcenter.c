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
 * k-center palette builder.
 *
 * The solver keeps centers anchored to sampled points and optimizes the
 * maximum assignment radius.  Three concrete strategies are available:
 *   - fft: Gonzalez farthest-first traversal.
 *   - swap: randomized initialization with worst-point local swaps.
 *   - hybrid: farthest-first initialization followed by swap refinement.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "timeline-logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kcenter.h"
#include "palette-private.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"

#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
# define SIXEL_KCENTER_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_TLS _Thread_local
# define SIXEL_KCENTER_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_TLS __thread
# define SIXEL_KCENTER_TLS_AVAILABLE 1
#else
# define SIXEL_TLS
# define SIXEL_KCENTER_TLS_AVAILABLE 0
#endif

static SIXEL_TLS int sixel_kcenter_algo_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_algo_t sixel_kcenter_algo_override_value
    = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
static SIXEL_TLS int sixel_kcenter_profile_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_profile_t sixel_kcenter_profile_override_value
    = SIXEL_PALETTE_KCENTER_PROFILE_LEGACY;
static SIXEL_TLS int sixel_kcenter_seed_override_enabled = 0;
static SIXEL_TLS uint32_t sixel_kcenter_seed_override_value = 1u;
static SIXEL_TLS int sixel_kcenter_restarts_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_restarts_override_value = 1u;
static SIXEL_TLS int sixel_kcenter_init_seeds_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_init_seeds_override_value = 1u;
static SIXEL_TLS int sixel_kcenter_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_iter_override_value = 16u;
static SIXEL_TLS int sixel_kcenter_histbits_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_histbits_override_value = 5u;
static SIXEL_TLS int sixel_kcenter_point_budget_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_point_budget_override_value = 0u;
static SIXEL_TLS int sixel_kcenter_prune_mass_override_enabled = 0;
static SIXEL_TLS double sixel_kcenter_prune_mass_override_value = 0.995;
static SIXEL_TLS int sixel_kcenter_auto_policy_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_auto_policy_t
    sixel_kcenter_auto_policy_override_value
    = SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
static SIXEL_TLS int sixel_kcenter_auto_fft_threshold_override_enabled = 0;
static SIXEL_TLS unsigned int
    sixel_kcenter_auto_fft_threshold_override_value = 2048u;
static SIXEL_TLS int sixel_kcenter_space_policy_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_space_policy_t
    sixel_kcenter_space_policy_override_value
    = SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
static SIXEL_TLS int sixel_kcenter_candidate_policy_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_candidate_policy_t
    sixel_kcenter_candidate_policy_override_value
    = SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
static SIXEL_TLS int sixel_kcenter_rare_keep_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_rare_keep_override_value = 0u;
static SIXEL_TLS int sixel_kcenter_budget_policy_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_budget_policy_t
    sixel_kcenter_budget_policy_override_value
    = SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
static SIXEL_TLS int sixel_kcenter_budget_scale_override_enabled = 0;
static SIXEL_TLS double sixel_kcenter_budget_scale_override_value = 1.0;
static SIXEL_TLS int sixel_kcenter_swap_topk_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_swap_topk_override_value = 1u;
static SIXEL_TLS int sixel_kcenter_swap_update_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_swap_update_t
    sixel_kcenter_swap_update_override_value
    = SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
static SIXEL_TLS int sixel_kcenter_swap_patience_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_swap_patience_override_value = 0u;
static SIXEL_TLS int sixel_kcenter_swap_min_gain_override_enabled = 0;
static SIXEL_TLS double sixel_kcenter_swap_min_gain_override_value = 0.0;
static SIXEL_TLS int sixel_kcenter_last_polish_applied = 0;
static SIXEL_TLS unsigned int sixel_kcenter_last_polish_updates = 0u;
static SIXEL_TLS double sixel_kcenter_last_polish_pre_radius2 = 0.0;
static SIXEL_TLS double sixel_kcenter_last_polish_post_radius2 = 0.0;

#define SIXEL_KCENTER_AUTO_FFT_THRESHOLD_DEFAULT 2048u
#define SIXEL_KCENTER_RARE_KEEP_DEFAULT 0u
#define SIXEL_KCENTER_BUDGET_SCALE_DEFAULT 1.0
#define SIXEL_KCENTER_SWAP_TOPK_DEFAULT 1u
#define SIXEL_KCENTER_SWAP_PATIENCE_DEFAULT 0u
#define SIXEL_KCENTER_INIT_SEEDS_DEFAULT 1u
#define SIXEL_KCENTER_SWAP_MIN_GAIN_DEFAULT 0.0
#define SIXEL_KCENTER_CHROMA_BUCKETS 2u
#define SIXEL_KCENTER_HUE_BUCKETS 8u
#define SIXEL_KCENTER_LUMA_BUCKETS 4u
#define SIXEL_KCENTER_STRATA_BUCKETS \
    (SIXEL_KCENTER_HUE_BUCKETS * SIXEL_KCENTER_LUMA_BUCKETS \
        * SIXEL_KCENTER_CHROMA_BUCKETS)
#define SIXEL_KCENTER_PI 3.14159265358979323846

#undef SIXEL_TLS

#if SIXEL_ENABLE_THREADS && !SIXEL_KCENTER_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#  if !defined(UNICODE)
#   define UNICODE
#  endif
#  if !defined(_UNICODE)
#   define _UNICODE
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
static CRITICAL_SECTION sixel_kcenter_override_mutex;
static INIT_ONCE sixel_kcenter_override_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_kcenter_override_lock_init_once(PINIT_ONCE once,
                                      PVOID parameter,
                                      PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&sixel_kcenter_override_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
static pthread_mutex_t sixel_kcenter_override_mutex;
static pthread_once_t sixel_kcenter_override_mutex_once = PTHREAD_ONCE_INIT;
static int sixel_kcenter_override_mutex_ready = 0;

static void
sixel_kcenter_override_lock_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&sixel_kcenter_override_mutex, NULL);
    if (rc == 0) {
        sixel_kcenter_override_mutex_ready = 1;
    }
}
# endif
#endif

/*
 * K-center tunables are thread-local when TLS exists.  Without TLS they become
 * process globals, so serialize override access in threaded builds.
 */
static int
sixel_kcenter_override_lock_acquire(void)
{
#if SIXEL_ENABLE_THREADS && !SIXEL_KCENTER_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&sixel_kcenter_override_once,
                                      sixel_kcenter_override_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        return 0;
    }
    EnterCriticalSection(&sixel_kcenter_override_mutex);
    return 1;
# else
    int rc;
    int once_status;

    once_status = pthread_once(&sixel_kcenter_override_mutex_once,
                               sixel_kcenter_override_lock_init_once);
    if (once_status != 0 || !sixel_kcenter_override_mutex_ready) {
        return 0;
    }

    rc = pthread_mutex_lock(&sixel_kcenter_override_mutex);
    if (rc != 0) {
        return 0;
    }
    return 1;
# endif
#else
    return 0;
#endif
}

static void
sixel_kcenter_override_lock_release(int acquired)
{
    if (acquired == 0) {
        return;
    }
#if SIXEL_ENABLE_THREADS && !SIXEL_KCENTER_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&sixel_kcenter_override_mutex);
# else
    if (!sixel_kcenter_override_mutex_ready) {
        return;
    }
    (void)pthread_mutex_unlock(&sixel_kcenter_override_mutex);
# endif
#endif
}

#undef SIXEL_KCENTER_TLS_AVAILABLE

typedef struct sixel_kcenter_bin {
    unsigned int index;
    unsigned int count;
    double r;
    double g;
    double b;
} sixel_kcenter_bin_t;

typedef struct sixel_kcenter_dispersion_rank {
    unsigned int index;
    double score;
} sixel_kcenter_dispersion_rank_t;

/* Keep solver call signatures compact for strict C compilers. */
typedef struct sixel_kcenter_swap_ctx {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int *centers;
    unsigned int k;
    unsigned char *center_mask;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    double *radius2_io;
    double *sse_io;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    unsigned int swap_topk;
    sixel_kcenter_swap_update_t swap_update;
    double swap_min_gain;
    int use_cluster_candidates;
    uint32_t *rng_state;
} sixel_kcenter_swap_ctx_t;

/*
 * Keep cached-swap apply signatures compact to avoid strict-compiler parser
 * and backend edge cases.
 */
typedef struct sixel_kcenter_swap_apply_ctx {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int *centers;
    unsigned int k;
    unsigned int swapped_slot;
    unsigned int swapped_center;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    double const *new_center_dist2;
    double *radius2_out;
    double *sse_out;
} sixel_kcenter_swap_apply_ctx_t;

static void
sixel_kcenter_swap_apply_ctx_clear(sixel_kcenter_swap_apply_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
}

/*
 * Keep swap-step orchestration state compact while preserving objective order:
 * radius is minimized first, and weighted SSE breaks ties.
 */
typedef struct sixel_kcenter_swap_step {
    unsigned int candidate_list[16];
    unsigned int candidate_count;
    unsigned int topk;
    unsigned int best_slot;
    unsigned int best_candidate;
    double best_radius2;
    double best_sse;
    double allowed_radius2;
    double *best_new_dist;
    double swap_min_gain;
    int found;
    int used_incremental_eval;
} sixel_kcenter_swap_step_t;

static void
sixel_kcenter_swap_step_clear(sixel_kcenter_swap_step_t *step)
{
    if (step == NULL) {
        return;
    }
    memset(step, 0, sizeof(*step));
}

typedef struct sixel_kcenter_solver_ctx {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int k;
    sixel_kcenter_algo_t resolved_algo;
    sixel_kcenter_profile_t profile;
    unsigned int init_seeds;
    unsigned int iter_limit;
    uint32_t *rng_state;
    unsigned int *centers;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    unsigned int swap_topk;
    sixel_kcenter_swap_update_t swap_update;
    unsigned int swap_patience;
    double swap_min_gain;
    int use_cluster_candidates;
    double *radius2_out;
    double *sse_out;
    unsigned int *iterations_out;
    unsigned int *scratch_indices;
    double *fft_dist_cache;
    unsigned char *center_mask;
} sixel_kcenter_solver_ctx_t;

/*
 * Keep run_solver mutable state in one place so staged helpers can preserve
 * the exact trial order and tie-break behavior.
 */
typedef struct sixel_kcenter_solver_run_state {
    SIXELSTATUS status;
    unsigned int init_seeds;
    unsigned int swap_topk;
    unsigned int swap_patience;
    double swap_min_gain;
    unsigned int init_trial;
    unsigned int iterations;
    unsigned int trial_iterations;
    double radius2;
    double sse;
    double tiny_gain2;
    double best_trial_radius2;
    double best_trial_sse;
    unsigned int best_trial_iterations;
    uint32_t base_state;
    uint32_t trial_state;
    int adaptive_swap_controls;
    sixel_kcenter_swap_ctx_t swap_ctx;
} sixel_kcenter_solver_run_state_t;

static void
sixel_kcenter_solver_run_state_clear(sixel_kcenter_solver_run_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

typedef struct sixel_kcenter_polish_ctx {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int *centers;
    unsigned int k;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    unsigned char *center_mask;
    double *cluster_weights;
    double *cluster_sums;
    double *radius2_io;
    double *sse_io;
} sixel_kcenter_polish_ctx_t;

typedef struct sixel_kcenter_build_ctx {
    unsigned char **result;
    float **result_float32;
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    unsigned int reqcolors;
    unsigned int *ncolors;
    unsigned int *origcolors;
    int quality_mode;
    int force_palette;
    int use_reversible;
    int final_merge_mode;
    sixel_allocator_t *allocator;
    int pixelformat;
    int treat_input_as_float32;
    sixel_timeline_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    sixel_palette_telemetry_t *telemetry;
} sixel_kcenter_build_ctx_t;

typedef struct sixel_kcenter_internal_ctx {
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_timeline_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    int treat_input_as_float32;
    sixel_palette_telemetry_t *telemetry;
} sixel_kcenter_internal_ctx_t;

static int
sixel_palette_kcenter_log_start(sixel_timeline_logger_t *logger,
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
        job_id = sixel_palette_timeline_next_job_id();
    }
    sixel_timeline_logger_logf(logger,
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
sixel_palette_kcenter_log_finish(sixel_timeline_logger_t *logger,
                                 int job_id,
                                 char const *engine_name,
                                 char const *role,
                                 char const *phase,
                                 char const *detail)
{
    char const *suffix;

    if (logger == NULL || job_id < 0) {
        return;
    }
    suffix = "";
    if (detail != NULL && detail[0] != '\0') {
        suffix = detail;
    }
    sixel_timeline_logger_logf(logger,
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

static sixel_kcenter_algo_t
sixel_kcenter_resolve_algo(sixel_kcenter_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KCENTER_ALGO_FFT:
    case SIXEL_PALETTE_KCENTER_ALGO_SWAP:
    case SIXEL_PALETTE_KCENTER_ALGO_HYBRID:
    case SIXEL_PALETTE_KCENTER_ALGO_AUTO:
        return algo;
    default:
        return SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    }
}

static char const *
sixel_kcenter_algo_to_string(sixel_kcenter_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KCENTER_ALGO_FFT:
        return "fft";
    case SIXEL_PALETTE_KCENTER_ALGO_SWAP:
        return "swap";
    case SIXEL_PALETTE_KCENTER_ALGO_HYBRID:
        return "hybrid";
    case SIXEL_PALETTE_KCENTER_ALGO_AUTO:
    default:
        return "auto";
    }
}

static sixel_kcenter_profile_t
sixel_kcenter_resolve_profile(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
        return profile;
    default:
        return SIXEL_PALETTE_KCENTER_PROFILE_LEGACY;
    }
}

static char const *
sixel_kcenter_profile_to_string(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return "speed";
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
        return "balance";
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return "quality";
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return "legacy";
    }
}

static sixel_kcenter_auto_policy_t
sixel_kcenter_resolve_auto_policy(sixel_kcenter_auto_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE:
    case SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY:
        return policy;
    default:
        return SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
    }
}

static char const *
sixel_kcenter_auto_policy_to_string(sixel_kcenter_auto_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE:
        return "adaptive";
    case SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY:
    default:
        return "legacy";
    }
}

static sixel_kcenter_candidate_policy_t
sixel_kcenter_resolve_candidate_policy(
    sixel_kcenter_candidate_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID:
    case SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY:
        return policy;
    default:
        return SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
    }
}

static char const *
sixel_kcenter_candidate_policy_to_string(
    sixel_kcenter_candidate_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID:
        return "hybrid";
    case SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY:
    default:
        return "legacy";
    }
}

static sixel_kcenter_space_policy_t
sixel_kcenter_resolve_space_policy(sixel_kcenter_space_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL:
    case SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY:
        return policy;
    default:
        return SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
    }
}

static char const *
sixel_kcenter_space_policy_to_string(sixel_kcenter_space_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL:
        return "perceptual";
    case SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY:
    default:
        return "legacy";
    }
}

static sixel_kcenter_budget_policy_t
sixel_kcenter_resolve_budget_policy(sixel_kcenter_budget_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE:
    case SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY:
        return policy;
    default:
        return SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
    }
}

static char const *
sixel_kcenter_budget_policy_to_string(sixel_kcenter_budget_policy_t policy)
{
    switch (policy) {
    case SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE:
        return "adaptive";
    case SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY:
    default:
        return "legacy";
    }
}

static sixel_kcenter_swap_update_t
sixel_kcenter_resolve_swap_update(sixel_kcenter_swap_update_t swap_update)
{
    switch (swap_update) {
    case SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL:
    case SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL:
        return swap_update;
    default:
        return SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
    }
}

static char const *
sixel_kcenter_swap_update_to_string(sixel_kcenter_swap_update_t swap_update)
{
    switch (swap_update) {
    case SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL:
        return "incremental";
    case SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL:
    default:
        return "full";
    }
}

static sixel_kcenter_auto_policy_t
sixel_kcenter_profile_default_auto_policy(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE;
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
    }
}

static unsigned int
sixel_kcenter_profile_default_auto_fft_threshold(
    sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return 1024u;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return 4096u;
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_AUTO_FFT_THRESHOLD_DEFAULT;
    }
}

static sixel_kcenter_candidate_policy_t
sixel_kcenter_profile_default_candidate_policy(
    sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID;
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
    }
}

static sixel_kcenter_space_policy_t
sixel_kcenter_profile_default_space_policy(
    sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL;
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
    }
}

static unsigned int
sixel_kcenter_profile_default_rare_keep(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return 16u;
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
        return 64u;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return 192u;
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_RARE_KEEP_DEFAULT;
    }
}

static sixel_kcenter_budget_policy_t
sixel_kcenter_profile_default_budget_policy(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE;
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
    }
}

static double
sixel_kcenter_profile_default_budget_scale(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return 0.80;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return 1.40;
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_BUDGET_SCALE_DEFAULT;
    }
}

static unsigned int
sixel_kcenter_profile_default_swap_topk(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return 2u;
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
        return 4u;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return 8u;
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_SWAP_TOPK_DEFAULT;
    }
}

static sixel_kcenter_swap_update_t
sixel_kcenter_profile_default_swap_update(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL;
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
    }
}

static unsigned int
sixel_kcenter_profile_default_swap_patience(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return 1u;
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
        return 2u;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return 4u;
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_SWAP_PATIENCE_DEFAULT;
    }
}

static unsigned int
sixel_kcenter_profile_default_init_seeds(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
        return 2u;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
        return 3u;
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_INIT_SEEDS_DEFAULT;
    }
}

static double
sixel_kcenter_profile_default_swap_min_gain(sixel_kcenter_profile_t profile)
{
    switch (profile) {
    case SIXEL_PALETTE_KCENTER_PROFILE_SPEED:
        return 0.10;
    case SIXEL_PALETTE_KCENTER_PROFILE_BALANCE:
        return 0.03;
    case SIXEL_PALETTE_KCENTER_PROFILE_QUALITY:
    case SIXEL_PALETTE_KCENTER_PROFILE_LEGACY:
    default:
        return SIXEL_KCENTER_SWAP_MIN_GAIN_DEFAULT;
    }
}

static uint32_t
sixel_kcenter_rng_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    if (value == 0u) {
        value = 1u;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static unsigned int
sixel_kcenter_rng_bounded(uint32_t *state,
                          unsigned int upper)
{
    uint32_t value;

    if (upper <= 1u) {
        return 0u;
    }
    value = sixel_kcenter_rng_next(state);
    return (unsigned int)(value % upper);
}

static unsigned int
sixel_kcenter_parse_env_uint(char const *name,
                             unsigned int min_value,
                             unsigned int max_value,
                             unsigned int fallback,
                             int allow_zero)
{
    char const *env;
    unsigned long parsed;
    char *endptr;

    env = NULL;
    parsed = 0ul;
    endptr = NULL;

    if (name == NULL) {
        return fallback;
    }

    env = sixel_compat_getenv(name);
    if (env == NULL || env[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtoul(env, &endptr, 10);
    if (endptr == env || endptr == NULL || endptr[0] != '\0' || errno != 0) {
        return fallback;
    }
    if (allow_zero && parsed == 0ul) {
        return 0u;
    }
    if (parsed < (unsigned long)min_value
            || parsed > (unsigned long)max_value) {
        return fallback;
    }
    return (unsigned int)parsed;
}

static double
sixel_kcenter_parse_env_double(char const *name,
                               double min_value,
                               double max_value,
                               double fallback)
{
    char const *env;
    double parsed;
    char *endptr;

    env = NULL;
    parsed = 0.0;
    endptr = NULL;

    if (name == NULL) {
        return fallback;
    }

    env = sixel_compat_getenv(name);
    if (env == NULL || env[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtod(env, &endptr);
    if (endptr == env || endptr == NULL || endptr[0] != '\0' || errno != 0) {
        return fallback;
    }
    if (parsed != parsed || parsed < min_value || parsed > max_value) {
        return fallback;
    }

    return parsed;
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_algo_override(int enabled,
                                sixel_kcenter_algo_t algo)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_algo_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_algo_override_value = sixel_kcenter_resolve_algo(algo);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_algo_t
sixel_get_kcenter_algo(void)
{
    char const *env_value;
    sixel_kcenter_algo_t parsed;
    static int cached = 0;
    static sixel_kcenter_algo_t cached_value = SIXEL_PALETTE_KCENTER_ALGO_AUTO;

    if (sixel_kcenter_algo_override_enabled) {
        return sixel_kcenter_resolve_algo(sixel_kcenter_algo_override_value);
    }

    if (cached) {
        return cached_value;
    }

    parsed = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_ALGO");
    if (env_value != NULL) {
        if (strcmp(env_value, "fft") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_FFT;
        } else if (strcmp(env_value, "swap") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_SWAP;
        } else if (strcmp(env_value, "hybrid") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_HYBRID;
        } else {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
        }
    }
    cached_value = sixel_kcenter_resolve_algo(parsed);
    cached = 1;
    return cached_value;
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_profile_override(int enabled,
                                   sixel_kcenter_profile_t profile)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_profile_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_profile_override_value
        = sixel_kcenter_resolve_profile(profile);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_profile_t
sixel_get_kcenter_profile(void)
{
    char const *env_value;
    sixel_kcenter_profile_t parsed;

    if (sixel_kcenter_profile_override_enabled) {
        return sixel_kcenter_resolve_profile(
            sixel_kcenter_profile_override_value);
    }

    parsed = SIXEL_PALETTE_KCENTER_PROFILE_LEGACY;
    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_PROFILE");
    if (env_value != NULL) {
        if (strcmp(env_value, "speed") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_PROFILE_SPEED;
        } else if (strcmp(env_value, "balance") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_PROFILE_BALANCE;
        } else if (strcmp(env_value, "quality") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_PROFILE_QUALITY;
        }
    }
    return sixel_kcenter_resolve_profile(parsed);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_seed_override(int enabled,
                                uint32_t seed)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_seed_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_seed_override_value = seed;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API uint32_t
sixel_get_kcenter_seed(void)
{
    unsigned int value;

    if (sixel_kcenter_seed_override_enabled) {
        value = (unsigned int)sixel_kcenter_seed_override_value;
        if (value == 0u) {
            value = 1u;
        }
        return (uint32_t)value;
    }

    value = sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_SEED",
                                         0u,
                                         UINT_MAX,
                                         1u,
                                         1);
    if (value == 0u) {
        value = 1u;
    }
    return (uint32_t)value;
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_restarts_override(int enabled,
                                    unsigned int restarts)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_restarts_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_restarts_override_value = restarts;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_restarts(void)
{
    if (sixel_kcenter_restarts_override_enabled) {
        if (sixel_kcenter_restarts_override_value < 1u) {
            return 1u;
        }
        if (sixel_kcenter_restarts_override_value > 32u) {
            return 32u;
        }
        return sixel_kcenter_restarts_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_RESTARTS",
                                        1u,
                                        32u,
                                        1u,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_init_seeds_override(int enabled,
                                      unsigned int init_seeds)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_init_seeds_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_init_seeds_override_value = init_seeds;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_init_seeds(void)
{
    unsigned int fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_init_seeds_override_enabled) {
        if (sixel_kcenter_init_seeds_override_value < 1u) {
            return 1u;
        }
        if (sixel_kcenter_init_seeds_override_value > 8u) {
            return 8u;
        }
        return sixel_kcenter_init_seeds_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_init_seeds(profile);
    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_INIT_SEEDS",
                                        1u,
                                        8u,
                                        fallback,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_iter_override(int enabled,
                                unsigned int iter_count)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_iter_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_iter_override_value = iter_count;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_iter(void)
{
    if (sixel_kcenter_iter_override_enabled) {
        if (sixel_kcenter_iter_override_value < 1u) {
            return 1u;
        }
        if (sixel_kcenter_iter_override_value > 64u) {
            return 64u;
        }
        return sixel_kcenter_iter_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_ITER",
                                        1u,
                                        64u,
                                        16u,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_histbits_override(int enabled,
                                    unsigned int histbits)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_histbits_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_histbits_override_value = histbits;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_histbits(void)
{
    if (sixel_kcenter_histbits_override_enabled) {
        if (sixel_kcenter_histbits_override_value < 3u) {
            return 3u;
        }
        if (sixel_kcenter_histbits_override_value > 6u) {
            return 6u;
        }
        return sixel_kcenter_histbits_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_HISTBITS",
                                        3u,
                                        6u,
                                        5u,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_point_budget_override(int enabled,
                                        unsigned int point_budget)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_point_budget_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_point_budget_override_value = point_budget;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_point_budget(void)
{
    if (sixel_kcenter_point_budget_override_enabled) {
        if (sixel_kcenter_point_budget_override_value == 0u) {
            return 0u;
        }
        if (sixel_kcenter_point_budget_override_value < 64u) {
            return 64u;
        }
        if (sixel_kcenter_point_budget_override_value > 16384u) {
            return 16384u;
        }
        return sixel_kcenter_point_budget_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_POINT_BUDGET",
                                        64u,
                                        16384u,
                                        0u,
                                        1);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_prune_mass_override(int enabled,
                                      double prune_mass)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_prune_mass_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_prune_mass_override_value = prune_mass;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API double
sixel_get_kcenter_prune_mass(void)
{
    if (sixel_kcenter_prune_mass_override_enabled) {
        if (sixel_kcenter_prune_mass_override_value < 0.900) {
            return 0.900;
        }
        if (sixel_kcenter_prune_mass_override_value > 1.000) {
            return 1.000;
        }
        return sixel_kcenter_prune_mass_override_value;
    }

    return sixel_kcenter_parse_env_double("SIXEL_PALETTE_KCENTER_PRUNE_MASS",
                                          0.900,
                                          1.000,
                                          0.995);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_auto_policy_override(
    int enabled,
    sixel_kcenter_auto_policy_t policy)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_auto_policy_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_auto_policy_override_value
        = sixel_kcenter_resolve_auto_policy(policy);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_auto_policy_t
sixel_get_kcenter_auto_policy(void)
{
    char const *env_value;
    sixel_kcenter_profile_t profile;
    sixel_kcenter_auto_policy_t parsed;

    if (sixel_kcenter_auto_policy_override_enabled) {
        return sixel_kcenter_resolve_auto_policy(
            sixel_kcenter_auto_policy_override_value);
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_AUTO_POLICY");
    if (env_value != NULL) {
        if (strcmp(env_value, "adaptive") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE;
            return sixel_kcenter_resolve_auto_policy(parsed);
        }
        if (strcmp(env_value, "legacy") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
            return sixel_kcenter_resolve_auto_policy(parsed);
        }
    }

    profile = sixel_get_kcenter_profile();
    return sixel_kcenter_profile_default_auto_policy(profile);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_auto_fft_threshold_override(
    int enabled,
    unsigned int threshold)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_auto_fft_threshold_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_auto_fft_threshold_override_value = threshold;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_auto_fft_threshold(void)
{
    unsigned int fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_auto_fft_threshold_override_enabled) {
        if (sixel_kcenter_auto_fft_threshold_override_value < 256u) {
            return 256u;
        }
        if (sixel_kcenter_auto_fft_threshold_override_value > 65536u) {
            return 65536u;
        }
        return sixel_kcenter_auto_fft_threshold_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_auto_fft_threshold(profile);
    return sixel_kcenter_parse_env_uint(
        "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD",
        256u,
        65536u,
        fallback,
        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_space_policy_override(
    int enabled,
    sixel_kcenter_space_policy_t policy)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_space_policy_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_space_policy_override_value
        = sixel_kcenter_resolve_space_policy(policy);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_space_policy_t
sixel_get_kcenter_space_policy(void)
{
    char const *env_value;
    sixel_kcenter_profile_t profile;
    sixel_kcenter_space_policy_t parsed;

    if (sixel_kcenter_space_policy_override_enabled) {
        return sixel_kcenter_resolve_space_policy(
            sixel_kcenter_space_policy_override_value);
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_SPACE_POLICY");
    if (env_value != NULL) {
        if (strcmp(env_value, "perceptual") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL;
            return sixel_kcenter_resolve_space_policy(parsed);
        }
        if (strcmp(env_value, "legacy") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
            return sixel_kcenter_resolve_space_policy(parsed);
        }
    }

    profile = sixel_get_kcenter_profile();
    return sixel_kcenter_profile_default_space_policy(profile);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_candidate_policy_override(
    int enabled,
    sixel_kcenter_candidate_policy_t policy)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_candidate_policy_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_candidate_policy_override_value
        = sixel_kcenter_resolve_candidate_policy(policy);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_candidate_policy_t
sixel_get_kcenter_candidate_policy(void)
{
    char const *env_value;
    sixel_kcenter_profile_t profile;
    sixel_kcenter_candidate_policy_t parsed;

    if (sixel_kcenter_candidate_policy_override_enabled) {
        return sixel_kcenter_resolve_candidate_policy(
            sixel_kcenter_candidate_policy_override_value);
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY");
    if (env_value != NULL) {
        if (strcmp(env_value, "hybrid") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID;
            return sixel_kcenter_resolve_candidate_policy(parsed);
        }
        if (strcmp(env_value, "legacy") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
            return sixel_kcenter_resolve_candidate_policy(parsed);
        }
    }

    profile = sixel_get_kcenter_profile();
    return sixel_kcenter_profile_default_candidate_policy(profile);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_rare_keep_override(int enabled,
                                     unsigned int rare_keep)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_rare_keep_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_rare_keep_override_value = rare_keep;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_rare_keep(void)
{
    unsigned int fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_rare_keep_override_enabled) {
        if (sixel_kcenter_rare_keep_override_value == 0u) {
            return 0u;
        }
        if (sixel_kcenter_rare_keep_override_value > 2048u) {
            return 2048u;
        }
        return sixel_kcenter_rare_keep_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_rare_keep(profile);
    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_RARE_KEEP",
                                        1u,
                                        2048u,
                                        fallback,
                                        1);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_budget_policy_override(
    int enabled,
    sixel_kcenter_budget_policy_t policy)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_budget_policy_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_budget_policy_override_value
        = sixel_kcenter_resolve_budget_policy(policy);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_budget_policy_t
sixel_get_kcenter_budget_policy(void)
{
    char const *env_value;
    sixel_kcenter_profile_t profile;
    sixel_kcenter_budget_policy_t parsed;

    if (sixel_kcenter_budget_policy_override_enabled) {
        return sixel_kcenter_resolve_budget_policy(
            sixel_kcenter_budget_policy_override_value);
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_BUDGET_POLICY");
    if (env_value != NULL) {
        if (strcmp(env_value, "adaptive") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE;
            return sixel_kcenter_resolve_budget_policy(parsed);
        }
        if (strcmp(env_value, "legacy") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
            return sixel_kcenter_resolve_budget_policy(parsed);
        }
    }

    profile = sixel_get_kcenter_profile();
    return sixel_kcenter_profile_default_budget_policy(profile);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_budget_scale_override(int enabled,
                                        double budget_scale)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_budget_scale_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_budget_scale_override_value = budget_scale;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API double
sixel_get_kcenter_budget_scale(void)
{
    double fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_budget_scale_override_enabled) {
        if (sixel_kcenter_budget_scale_override_value < 0.25) {
            return 0.25;
        }
        if (sixel_kcenter_budget_scale_override_value > 4.00) {
            return 4.00;
        }
        return sixel_kcenter_budget_scale_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_budget_scale(profile);
    return sixel_kcenter_parse_env_double("SIXEL_PALETTE_KCENTER_BUDGET_SCALE",
                                          0.25,
                                          4.00,
                                          fallback);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_topk_override(int enabled,
                                     unsigned int swap_topk)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_swap_topk_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_swap_topk_override_value = swap_topk;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_swap_topk(void)
{
    unsigned int fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_swap_topk_override_enabled) {
        if (sixel_kcenter_swap_topk_override_value < 1u) {
            return 1u;
        }
        if (sixel_kcenter_swap_topk_override_value > 16u) {
            return 16u;
        }
        return sixel_kcenter_swap_topk_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_swap_topk(profile);
    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_SWAP_TOPK",
                                        1u,
                                        16u,
                                        fallback,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_update_override(
    int enabled,
    sixel_kcenter_swap_update_t swap_update)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_swap_update_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_swap_update_override_value
        = sixel_kcenter_resolve_swap_update(swap_update);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_swap_update_t
sixel_get_kcenter_swap_update(void)
{
    char const *env_value;
    sixel_kcenter_profile_t profile;
    sixel_kcenter_swap_update_t parsed;

    if (sixel_kcenter_swap_update_override_enabled) {
        return sixel_kcenter_resolve_swap_update(
            sixel_kcenter_swap_update_override_value);
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_SWAP_UPDATE");
    if (env_value != NULL) {
        if (strcmp(env_value, "incremental") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL;
            return sixel_kcenter_resolve_swap_update(parsed);
        }
        if (strcmp(env_value, "full") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
            return sixel_kcenter_resolve_swap_update(parsed);
        }
    }

    profile = sixel_get_kcenter_profile();
    return sixel_kcenter_profile_default_swap_update(profile);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_patience_override(
    int enabled,
    unsigned int swap_patience)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_swap_patience_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_swap_patience_override_value = swap_patience;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_swap_patience(void)
{
    unsigned int fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_swap_patience_override_enabled) {
        if (sixel_kcenter_swap_patience_override_value > 8u) {
            return 8u;
        }
        return sixel_kcenter_swap_patience_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_swap_patience(profile);
    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_SWAP_PATIENCE",
                                        1u,
                                        8u,
                                        fallback,
                                        1);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_swap_min_gain_override(int enabled,
                                         double swap_min_gain)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_swap_min_gain_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_swap_min_gain_override_value = swap_min_gain;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API double
sixel_get_kcenter_swap_min_gain(void)
{
    double fallback;
    sixel_kcenter_profile_t profile;

    if (sixel_kcenter_swap_min_gain_override_enabled) {
        if (sixel_kcenter_swap_min_gain_override_value < 0.0) {
            return 0.0;
        }
        if (sixel_kcenter_swap_min_gain_override_value > 8.0) {
            return 8.0;
        }
        return sixel_kcenter_swap_min_gain_override_value;
    }

    profile = sixel_get_kcenter_profile();
    fallback = sixel_kcenter_profile_default_swap_min_gain(profile);
    return sixel_kcenter_parse_env_double("SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN",
                                          0.0,
                                          8.0,
                                          fallback);
}

SIXEL_INTERNAL_API int
sixel_get_kcenter_last_polish_applied(void)
{
    return sixel_kcenter_last_polish_applied;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_last_polish_updates(void)
{
    return sixel_kcenter_last_polish_updates;
}

SIXEL_INTERNAL_API double
sixel_get_kcenter_last_polish_pre_radius2(void)
{
    return sixel_kcenter_last_polish_pre_radius2;
}

SIXEL_INTERNAL_API double
sixel_get_kcenter_last_polish_post_radius2(void)
{
    return sixel_kcenter_last_polish_post_radius2;
}

static int
sixel_kcenter_compare_bin_desc(void const *lhs,
                               void const *rhs)
{
    sixel_kcenter_bin_t const *a;
    sixel_kcenter_bin_t const *b;

    a = (sixel_kcenter_bin_t const *)lhs;
    b = (sixel_kcenter_bin_t const *)rhs;
    if (a->count > b->count) {
        return -1;
    }
    if (a->count < b->count) {
        return 1;
    }
    if (a->index < b->index) {
        return -1;
    }
    if (a->index > b->index) {
        return 1;
    }
    return 0;
}

static int
sixel_kcenter_compare_dispersion_asc(void const *lhs,
                                     void const *rhs)
{
    sixel_kcenter_dispersion_rank_t const *left;
    sixel_kcenter_dispersion_rank_t const *right;

    left = (sixel_kcenter_dispersion_rank_t const *)lhs;
    right = (sixel_kcenter_dispersion_rank_t const *)rhs;
    if (left->score < right->score) {
        return -1;
    }
    if (left->score > right->score) {
        return 1;
    }
    if (left->index < right->index) {
        return -1;
    }
    if (left->index > right->index) {
        return 1;
    }
    return 0;
}

static int
sixel_kcenter_compare_dispersion_desc(void const *lhs,
                                      void const *rhs)
{
    sixel_kcenter_dispersion_rank_t const *left;
    sixel_kcenter_dispersion_rank_t const *right;

    left = (sixel_kcenter_dispersion_rank_t const *)lhs;
    right = (sixel_kcenter_dispersion_rank_t const *)rhs;
    if (left->score > right->score) {
        return -1;
    }
    if (left->score < right->score) {
        return 1;
    }
    if (left->index < right->index) {
        return -1;
    }
    if (left->index > right->index) {
        return 1;
    }
    return 0;
}

static unsigned int
sixel_kcenter_apply_budget_scale(unsigned int budget,
                                 double budget_scale)
{
    double scaled;

    if (budget == 0u) {
        return 0u;
    }
    if (budget_scale < 0.25) {
        budget_scale = 0.25;
    }
    if (budget_scale > 4.00) {
        budget_scale = 4.00;
    }
    scaled = (double)budget * budget_scale;
    if (scaled < 1.0) {
        scaled = 1.0;
    }
    if (scaled > 16384.0) {
        scaled = 16384.0;
    }
    return (unsigned int)(scaled + 0.5);
}

static unsigned int
sixel_kcenter_auto_point_budget_legacy(unsigned int reqcolors,
                                       unsigned int visible_count,
                                       int quality_mode)
{
    unsigned int budget;

    budget = reqcolors;
    if (budget == 0u) {
        budget = 1u;
    }
    if (quality_mode == SIXEL_QUALITY_LOW) {
        budget *= 8u;
    } else if (quality_mode == SIXEL_QUALITY_FULL) {
        budget *= 24u;
    } else {
        budget *= 16u;
    }
    if (budget < 64u) {
        budget = 64u;
    }
    if (budget > 16384u) {
        budget = 16384u;
    }
    if (visible_count > 0u && budget > visible_count) {
        budget = visible_count;
    }
    if (budget == 0u) {
        budget = 1u;
    }
    return budget;
}

static unsigned int
sixel_kcenter_auto_point_budget_adaptive(unsigned int reqcolors,
                                         unsigned int visible_count,
                                         unsigned int active_count,
                                         int quality_mode,
                                         double entropy_norm,
                                         double effective_density)
{
    unsigned int budget;
    unsigned int soft_cap;
    double active_ratio;
    double scale;
    double scaled_budget;

    budget = sixel_kcenter_auto_point_budget_legacy(reqcolors,
                                                    visible_count,
                                                    quality_mode);
    if (active_count == 0u) {
        return budget;
    }

    if (entropy_norm < 0.0) {
        entropy_norm = 0.0;
    }
    if (entropy_norm > 1.0) {
        entropy_norm = 1.0;
    }
    if (effective_density < 0.0) {
        effective_density = 0.0;
    }
    if (effective_density > 1.0) {
        effective_density = 1.0;
    }

    active_ratio = 0.0;
    if (visible_count > 0u) {
        active_ratio = (double)active_count / (double)visible_count;
    }
    scale = 0.70 + entropy_norm * 0.60 + effective_density * 0.50;

    if (active_ratio < 0.02) {
        scale *= 0.80;
    } else if (active_ratio > 0.25) {
        scale *= 1.20;
    }

    if (quality_mode == SIXEL_QUALITY_LOW) {
        scale *= 0.90;
    } else if (quality_mode == SIXEL_QUALITY_FULL) {
        scale *= 1.10;
    }
    if (active_ratio < 0.05 && scale > 1.10) {
        scale = 1.10;
    }
    if (entropy_norm > 0.85
            && effective_density > 0.85
            && scale > 1.20) {
        scale = 1.20;
    }
    if (entropy_norm < 0.20
            && effective_density < 0.20
            && scale > 1.05) {
        scale = 1.05;
    }

    scaled_budget = (double)budget * scale;
    if (scaled_budget < 64.0) {
        scaled_budget = 64.0;
    }
    if (scaled_budget > 16384.0) {
        scaled_budget = 16384.0;
    }
    budget = (unsigned int)(scaled_budget + 0.5);
    soft_cap = reqcolors;
    if (soft_cap == 0u) {
        soft_cap = 1u;
    }
    if (quality_mode == SIXEL_QUALITY_LOW) {
        soft_cap *= 10u;
    } else if (quality_mode == SIXEL_QUALITY_FULL) {
        soft_cap *= 20u;
    } else {
        soft_cap *= 14u;
    }
    if (soft_cap < 64u) {
        soft_cap = 64u;
    }
    if (soft_cap > 16384u) {
        soft_cap = 16384u;
    }
    if (entropy_norm > 0.85
            && effective_density > 0.85
            && budget > soft_cap) {
        budget = soft_cap;
    }
    if (budget > active_count) {
        budget = active_count;
    }
    if (budget == 0u) {
        budget = 1u;
    }
    return budget;
}

typedef struct sixel_kcenter_collect_ctx {
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    int pixelformat;
    int treat_input_as_float32;
    unsigned int histbits;
    unsigned int point_budget;
    double prune_mass;
    unsigned int reqcolors;
    int quality_mode;
    sixel_kcenter_candidate_policy_t candidate_policy;
    sixel_kcenter_space_policy_t space_policy;
    unsigned int rare_keep;
    sixel_kcenter_budget_policy_t budget_policy;
    double budget_scale;
    double const *float32_channel_scale;
    double const *float32_channel_offset;
    sixel_allocator_t *allocator;
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int pixel_count;
    unsigned int bin_count;
    unsigned int shift_bits;
    unsigned int visible_count;
    unsigned int active_count;
    unsigned int keep_count;
    unsigned int budget;
    unsigned int selected_count;
    unsigned int retained_count;
    unsigned int rare_limit;
    unsigned int high_target;
    double total_weight;
    double retained_weight;
    double entropy_norm;
    double effective_density;
    int input_is_float32;
    int use_perceptual_strata;
    int oklab_perceptual_space;
    unsigned int *counts;
    double *sums;
    sixel_kcenter_bin_t *bins;
    unsigned char *bin_selected;
    sixel_kcenter_dispersion_rank_t *dispersion;
    sixel_kcenter_dispersion_rank_t *chroma_rank;
    double *chroma_cache;
    double *points;
    double *weights;
    unsigned int strata_picks[SIXEL_KCENTER_STRATA_BUCKETS];
    double strata_scores[SIXEL_KCENTER_STRATA_BUCKETS];
    unsigned int strata_order[SIXEL_KCENTER_STRATA_BUCKETS];
    double chroma_edges[SIXEL_KCENTER_CHROMA_BUCKETS - 1u];
} sixel_kcenter_collect_ctx_t;

static void
sixel_kcenter_collect_reset_strata(sixel_kcenter_collect_ctx_t *ctx)
{
    unsigned int strata_index;

    strata_index = 0u;
    if (ctx == NULL) {
        return;
    }
    for (strata_index = 0u;
            strata_index < SIXEL_KCENTER_STRATA_BUCKETS;
            ++strata_index) {
        ctx->strata_picks[strata_index] = UINT_MAX;
        ctx->strata_scores[strata_index] = -1.0;
        ctx->strata_order[strata_index] = strata_index;
    }
    for (strata_index = 0u;
            strata_index + 1u < SIXEL_KCENTER_CHROMA_BUCKETS;
            ++strata_index) {
        ctx->chroma_edges[strata_index] = 0.0;
    }
}

static void
sixel_kcenter_collect_ctx_clear(sixel_kcenter_collect_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    sixel_kcenter_collect_reset_strata(ctx);
}

static SIXELSTATUS
sixel_kcenter_collect_prepare(sixel_kcenter_collect_ctx_t *ctx)
{
    SIXELSTATUS status;
    int input_is_float32;

    status = SIXEL_BAD_ARGUMENT;
    input_is_float32 = 0;
    if (ctx == NULL) {
        return status;
    }

    ctx->channels = ctx->depth;
    ctx->pixel_stride = ctx->depth;
    ctx->budget = ctx->point_budget;
    ctx->visible_count = 0u;
    ctx->active_count = 0u;
    ctx->retained_count = 0u;
    ctx->selected_count = 0u;
    ctx->input_is_float32 = 0;
    ctx->use_perceptual_strata = 0;
    ctx->oklab_perceptual_space = 0;

    input_is_float32 = (ctx->treat_input_as_float32
                        && SIXEL_PIXELFORMAT_IS_FLOAT32(ctx->pixelformat));
    if (input_is_float32) {
        if (ctx->depth == 0u
                || ctx->depth % (unsigned int)sizeof(float) != 0u) {
            return status;
        }
        ctx->channels = ctx->depth / (unsigned int)sizeof(float);
        ctx->pixel_stride = ctx->depth;
    }
    if (ctx->channels != 3u && ctx->channels != 4u) {
        return status;
    }
    if (ctx->pixel_stride == 0u) {
        return status;
    }

    ctx->input_is_float32 = input_is_float32;
    ctx->pixel_count = ctx->length / ctx->pixel_stride;
    if (ctx->histbits > 8u) {
        return status;
    }
    ctx->shift_bits = 8u - ctx->histbits;
    ctx->bin_count = 1u << (ctx->histbits * 3u);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kcenter_collect_build_histogram(sixel_kcenter_collect_ctx_t *ctx)
{
    SIXELSTATUS status;
    unsigned int index;
    unsigned int ri;
    unsigned int gi;
    unsigned int bi;
    unsigned int bin_index;
    unsigned int offset;
    unsigned int alpha_byte;
    double red;
    double green;
    double blue;
    float const *pixel_float;

    status = SIXEL_BAD_ALLOCATION;
    index = 0u;
    ri = 0u;
    gi = 0u;
    bi = 0u;
    bin_index = 0u;
    offset = 0u;
    alpha_byte = 0u;
    red = 0.0;
    green = 0.0;
    blue = 0.0;
    pixel_float = NULL;

    ctx->counts = (unsigned int *)sixel_allocator_malloc(
        ctx->allocator,
        (size_t)ctx->bin_count * sizeof(unsigned int));
    ctx->sums = (double *)sixel_allocator_malloc(
        ctx->allocator,
        (size_t)ctx->bin_count * 3u * sizeof(double));
    if (ctx->counts == NULL || ctx->sums == NULL) {
        return status;
    }
    memset(ctx->counts, 0, (size_t)ctx->bin_count * sizeof(unsigned int));
    memset(ctx->sums, 0, (size_t)ctx->bin_count * 3u * sizeof(double));

    for (index = 0u; index < ctx->pixel_count; ++index) {
        offset = index * ctx->pixel_stride;
        if (ctx->channels == 4u) {
            if (ctx->input_is_float32) {
                pixel_float = (float const *)(void const *)(ctx->data + offset);
                if (sixel_pixelformat_float_channel_clamp(
                        ctx->pixelformat,
                        3,
                        pixel_float[3]) <= 0.0f) {
                    continue;
                }
            } else {
                alpha_byte = ctx->data[offset + 3u];
                if (alpha_byte == 0u) {
                    continue;
                }
            }
        }

        if (ctx->input_is_float32) {
            pixel_float = (float const *)(void const *)(ctx->data + offset);
            red = (double)sixel_pixelformat_float_channel_clamp(
                ctx->pixelformat,
                0,
                pixel_float[0]);
            green = (double)sixel_pixelformat_float_channel_clamp(
                ctx->pixelformat,
                1,
                pixel_float[1]);
            blue = (double)sixel_pixelformat_float_channel_clamp(
                ctx->pixelformat,
                2,
                pixel_float[2]);
            red = red * ctx->float32_channel_scale[0]
                + ctx->float32_channel_offset[0];
            green = green * ctx->float32_channel_scale[1]
                + ctx->float32_channel_offset[1];
            blue = blue * ctx->float32_channel_scale[2]
                + ctx->float32_channel_offset[2];
        } else {
            red = (double)ctx->data[offset + 0u];
            green = (double)ctx->data[offset + 1u];
            blue = (double)ctx->data[offset + 2u];
        }

        if (red < 0.0) {
            red = 0.0;
        } else if (red > 255.0) {
            red = 255.0;
        }
        if (green < 0.0) {
            green = 0.0;
        } else if (green > 255.0) {
            green = 255.0;
        }
        if (blue < 0.0) {
            blue = 0.0;
        } else if (blue > 255.0) {
            blue = 255.0;
        }

        ri = ((unsigned int)red) >> ctx->shift_bits;
        gi = ((unsigned int)green) >> ctx->shift_bits;
        bi = ((unsigned int)blue) >> ctx->shift_bits;
        bin_index = (ri << (ctx->histbits * 2u))
            | (gi << ctx->histbits) | bi;
        if (bin_index >= ctx->bin_count) {
            continue;
        }
        ctx->counts[bin_index] += 1u;
        ctx->sums[bin_index * 3u + 0u] += red;
        ctx->sums[bin_index * 3u + 1u] += green;
        ctx->sums[bin_index * 3u + 2u] += blue;
        ++ctx->visible_count;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kcenter_collect_build_bins_and_stats(sixel_kcenter_collect_ctx_t *ctx)
{
    SIXELSTATUS status;
    unsigned int index;
    unsigned int active_count;
    double keep_target;
    double accum_weight;
    double retained_weight;
    double probability;
    double entropy;
    double effective_bins;

    status = SIXEL_BAD_ALLOCATION;
    index = 0u;
    active_count = 0u;
    keep_target = 0.0;
    accum_weight = 0.0;
    retained_weight = 0.0;
    probability = 0.0;
    entropy = 0.0;
    effective_bins = 0.0;

    if (ctx->visible_count == 0u) {
        ctx->active_count = 0u;
        ctx->retained_count = 0u;
        return SIXEL_OK;
    }

    for (index = 0u; index < ctx->bin_count; ++index) {
        if (ctx->counts[index] > 0u) {
            ++active_count;
        }
    }
    if (active_count == 0u) {
        ctx->active_count = 0u;
        ctx->retained_count = 0u;
        return SIXEL_OK;
    }

    ctx->keep_count = active_count;
    ctx->bins = (sixel_kcenter_bin_t *)sixel_allocator_malloc(
        ctx->allocator,
        (size_t)ctx->keep_count * sizeof(sixel_kcenter_bin_t));
    if (ctx->bins == NULL) {
        return status;
    }

    active_count = 0u;
    ctx->total_weight = 0.0;
    for (index = 0u; index < ctx->bin_count; ++index) {
        if (ctx->counts[index] == 0u) {
            continue;
        }
        ctx->bins[active_count].index = index;
        ctx->bins[active_count].count = ctx->counts[index];
        ctx->bins[active_count].r = ctx->sums[index * 3u + 0u]
            / (double)ctx->counts[index];
        ctx->bins[active_count].g = ctx->sums[index * 3u + 1u]
            / (double)ctx->counts[index];
        ctx->bins[active_count].b = ctx->sums[index * 3u + 2u]
            / (double)ctx->counts[index];
        ctx->total_weight += (double)ctx->counts[index];
        ++active_count;
    }

    qsort(ctx->bins,
          active_count,
          sizeof(sixel_kcenter_bin_t),
          sixel_kcenter_compare_bin_desc);

    keep_target = ctx->prune_mass * ctx->total_weight;
    if (keep_target < 1.0) {
        keep_target = 1.0;
    }
    accum_weight = 0.0;
    ctx->active_count = 0u;
    for (index = 0u; index < active_count; ++index) {
        accum_weight += (double)ctx->bins[index].count;
        ++ctx->active_count;
        if (accum_weight >= keep_target
                && ctx->active_count >= ctx->reqcolors) {
            break;
        }
    }
    if (ctx->active_count == 0u) {
        ctx->active_count = 1u;
    }
    ctx->retained_count = ctx->active_count;
    ctx->retained_weight = 0.0;
    retained_weight = 0.0;
    for (index = 0u; index < ctx->retained_count; ++index) {
        retained_weight += (double)ctx->bins[index].count;
    }
    ctx->retained_weight = retained_weight;

    if (retained_weight > 0.0 && ctx->retained_count > 1u) {
        entropy = 0.0;
        for (index = 0u; index < ctx->retained_count; ++index) {
            probability = (double)ctx->bins[index].count / retained_weight;
            if (probability <= 0.0) {
                continue;
            }
            entropy -= probability * log(probability);
        }
        ctx->entropy_norm = entropy / log((double)ctx->retained_count);
        if (ctx->entropy_norm < 0.0) {
            ctx->entropy_norm = 0.0;
        } else if (ctx->entropy_norm > 1.0) {
            ctx->entropy_norm = 1.0;
        }
        effective_bins = exp(entropy);
        ctx->effective_density = effective_bins / (double)ctx->retained_count;
        if (ctx->effective_density < 0.0) {
            ctx->effective_density = 0.0;
        } else if (ctx->effective_density > 1.0) {
            ctx->effective_density = 1.0;
        }
    } else {
        ctx->entropy_norm = 0.0;
        ctx->effective_density = 0.0;
    }
    return SIXEL_OK;
}

static void
sixel_kcenter_collect_resolve_budget(sixel_kcenter_collect_ctx_t *ctx)
{
    if (ctx->budget == 0u) {
        if (sixel_kcenter_resolve_budget_policy(ctx->budget_policy)
                == SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE) {
            ctx->budget = sixel_kcenter_auto_point_budget_adaptive(
                ctx->reqcolors,
                ctx->visible_count,
                ctx->retained_count,
                ctx->quality_mode,
                ctx->entropy_norm,
                ctx->effective_density);
        } else {
            ctx->budget = sixel_kcenter_auto_point_budget_legacy(
                ctx->reqcolors,
                ctx->visible_count,
                ctx->quality_mode);
        }
        ctx->budget = sixel_kcenter_apply_budget_scale(ctx->budget,
                                                       ctx->budget_scale);
    }
    if (ctx->budget > ctx->retained_count) {
        ctx->budget = ctx->retained_count;
    }
    if (ctx->budget == 0u) {
        ctx->budget = 1u;
    }
}

static SIXELSTATUS
sixel_kcenter_collect_allocate_output_buffers(sixel_kcenter_collect_ctx_t *ctx)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ALLOCATION;
    ctx->points = (double *)sixel_allocator_malloc(
        ctx->allocator,
        (size_t)ctx->budget * 3u * sizeof(double));
    ctx->weights = (double *)sixel_allocator_malloc(
        ctx->allocator,
        (size_t)ctx->budget * sizeof(double));
    if (ctx->points == NULL || ctx->weights == NULL) {
        return status;
    }
    return SIXEL_OK;
}

static void
sixel_kcenter_collect_select_legacy(sixel_kcenter_collect_ctx_t *ctx)
{
    unsigned int index;

    index = 0u;
    for (index = 0u; index < ctx->budget; ++index) {
        ctx->points[index * 3u + 0u] = ctx->bins[index].r;
        ctx->points[index * 3u + 1u] = ctx->bins[index].g;
        ctx->points[index * 3u + 2u] = ctx->bins[index].b;
        ctx->weights[index] = (double)ctx->bins[index].count;
    }
}

static void
sixel_kcenter_collect_hybrid_pick_strata(sixel_kcenter_collect_ctx_t *ctx)
{
    unsigned int strata_rank;
    unsigned int strata_swap;
    unsigned int strata_index;
    unsigned int bin_index;

    strata_rank = 0u;
    strata_swap = 0u;
    strata_index = 0u;
    bin_index = 0u;
    for (strata_rank = 0u;
            strata_rank < SIXEL_KCENTER_STRATA_BUCKETS;
            ++strata_rank) {
        for (strata_swap = strata_rank + 1u;
                strata_swap < SIXEL_KCENTER_STRATA_BUCKETS;
                ++strata_swap) {
            if (ctx->strata_scores[ctx->strata_order[strata_swap]]
                    > ctx->strata_scores[ctx->strata_order[strata_rank]]
                    + 1.0e-12) {
                strata_index = ctx->strata_order[strata_rank];
                ctx->strata_order[strata_rank]
                    = ctx->strata_order[strata_swap];
                ctx->strata_order[strata_swap] = strata_index;
            }
        }
    }
    for (strata_rank = 0u;
            strata_rank < SIXEL_KCENTER_STRATA_BUCKETS
            && ctx->selected_count < ctx->budget;
            ++strata_rank) {
        strata_index = ctx->strata_order[strata_rank];
        bin_index = ctx->strata_picks[strata_index];
        if (bin_index == UINT_MAX) {
            continue;
        }
        if (bin_index >= ctx->retained_count
                || ctx->bin_selected[bin_index] != 0u) {
            continue;
        }
        ctx->bin_selected[bin_index] = 1u;
        ++ctx->selected_count;
    }
}

static void
sixel_kcenter_collect_hybrid_pick_remaining(sixel_kcenter_collect_ctx_t *ctx)
{
    unsigned int index;
    unsigned int bin_index;

    index = 0u;
    bin_index = 0u;
    ctx->high_target = ctx->budget;
    if (ctx->high_target > ctx->selected_count) {
        if (ctx->oklab_perceptual_space) {
            /*
             * Reserve more slots for dispersion picks in OKLab so the
             * candidate set does not over-focus on only high-mass bins.
             */
            ctx->high_target = ctx->selected_count
                + (ctx->budget - ctx->selected_count) / 2u;
        } else {
            ctx->high_target = ctx->selected_count
                + ((ctx->budget - ctx->selected_count) * 3u) / 4u;
        }
        if (ctx->high_target > ctx->budget) {
            ctx->high_target = ctx->budget;
        }
    }

    /*
     * Keep hybrid priority stable:
     * rare bins -> strata representatives -> mass order -> dispersion.
     */
    for (index = 0u;
            index < ctx->retained_count
            && ctx->selected_count < ctx->high_target;
            ++index) {
        if (ctx->bin_selected[index] != 0u) {
            continue;
        }
        ctx->bin_selected[index] = 1u;
        ++ctx->selected_count;
    }
    for (index = 0u;
            index < ctx->retained_count
            && ctx->selected_count < ctx->budget;
            ++index) {
        bin_index = ctx->dispersion[index].index;
        if (bin_index >= ctx->retained_count
                || ctx->bin_selected[bin_index] != 0u) {
            continue;
        }
        ctx->bin_selected[bin_index] = 1u;
        ++ctx->selected_count;
    }
    for (index = 0u;
            index < ctx->retained_count
            && ctx->selected_count < ctx->budget;
            ++index) {
        if (ctx->bin_selected[index] != 0u) {
            continue;
        }
        ctx->bin_selected[index] = 1u;
        ++ctx->selected_count;
    }
    if (ctx->selected_count == 0u) {
        ctx->bin_selected[0u] = 1u;
        ctx->selected_count = 1u;
    }
}

static void
sixel_kcenter_collect_hybrid_write_points(sixel_kcenter_collect_ctx_t *ctx)
{
    unsigned int index;
    unsigned int bin_index;

    index = 0u;
    bin_index = 0u;
    ctx->budget = ctx->selected_count;
    for (index = 0u;
            index < ctx->retained_count && bin_index < ctx->budget;
            ++index) {
        if (ctx->bin_selected[index] == 0u) {
            continue;
        }
        ctx->points[bin_index * 3u + 0u] = ctx->bins[index].r;
        ctx->points[bin_index * 3u + 1u] = ctx->bins[index].g;
        ctx->points[bin_index * 3u + 2u] = ctx->bins[index].b;
        ctx->weights[bin_index] = (double)ctx->bins[index].count;
        ++bin_index;
    }
}

typedef struct sixel_kcenter_collect_hybrid_state {
    unsigned int index;
    unsigned int tail_index;
    unsigned int strata_index;
    unsigned int strata_rank;
    unsigned int strata_swap;
    unsigned int bin_index;
    unsigned int hue_bucket;
    unsigned int luma_bucket;
    unsigned int chroma_bucket;
    int chroma_edge_found;
    double total_weight;
    double mean_r;
    double mean_g;
    double mean_b;
    double dr;
    double dg;
    double db;
    double dispersion_score;
    double max_component;
    double min_component;
    double delta_component;
    double hue_value;
    double hue_phase;
    double normalized_hue;
    double luma_value;
    double chroma_value;
    double chroma_target;
    double chroma_accum;
} sixel_kcenter_collect_hybrid_state_t;

static void
sixel_kcenter_collect_hybrid_state_clear(
    sixel_kcenter_collect_hybrid_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

static void
sixel_kcenter_collect_hybrid_resolve_space(sixel_kcenter_collect_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->use_perceptual_strata = 0;
    ctx->oklab_perceptual_space = 0;
    if (sixel_kcenter_resolve_space_policy(ctx->space_policy)
            == SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL
            && (ctx->pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32
                || ctx->pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32
                || ctx->pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32)) {
        ctx->use_perceptual_strata = 1;
        if (ctx->pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
            ctx->oklab_perceptual_space = 1;
        }
    }
}

static SIXELSTATUS
sixel_kcenter_collect_hybrid_allocate_scratch(sixel_kcenter_collect_ctx_t *ctx)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ALLOCATION;
    if (ctx == NULL) {
        return status;
    }

    ctx->bin_selected = (unsigned char *)sixel_allocator_malloc(
        ctx->allocator,
        (size_t)ctx->retained_count);
    ctx->dispersion = (sixel_kcenter_dispersion_rank_t *)
        sixel_allocator_malloc(
            ctx->allocator,
            (size_t)ctx->retained_count
            * sizeof(sixel_kcenter_dispersion_rank_t));
    if (ctx->use_perceptual_strata) {
        ctx->chroma_cache = (double *)sixel_allocator_malloc(
            ctx->allocator,
            (size_t)ctx->retained_count * sizeof(double));
        ctx->chroma_rank = (sixel_kcenter_dispersion_rank_t *)
            sixel_allocator_malloc(
                ctx->allocator,
                (size_t)ctx->retained_count
                * sizeof(sixel_kcenter_dispersion_rank_t));
    }
    if (ctx->bin_selected == NULL || ctx->dispersion == NULL) {
        return status;
    }
    if (ctx->use_perceptual_strata
            && (ctx->chroma_cache == NULL || ctx->chroma_rank == NULL)) {
        return status;
    }
    memset(ctx->bin_selected, 0, (size_t)ctx->retained_count);
    return SIXEL_OK;
}

static void
sixel_kcenter_collect_hybrid_pick_rare(
    sixel_kcenter_collect_ctx_t *ctx,
    sixel_kcenter_collect_hybrid_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    ctx->selected_count = 0u;
    ctx->rare_limit = ctx->rare_keep;
    if (ctx->rare_limit > ctx->budget) {
        ctx->rare_limit = ctx->budget;
    }
    if (ctx->rare_limit > ctx->retained_count) {
        ctx->rare_limit = ctx->retained_count;
    }
    for (state->index = 0u;
            state->index < ctx->rare_limit;
            ++state->index) {
        state->tail_index = ctx->retained_count - 1u - state->index;
        if (ctx->bin_selected[state->tail_index] != 0u) {
            continue;
        }
        ctx->bin_selected[state->tail_index] = 1u;
        ++ctx->selected_count;
    }
}

static void
sixel_kcenter_collect_hybrid_build_dispersion(
    sixel_kcenter_collect_ctx_t *ctx,
    sixel_kcenter_collect_hybrid_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    state->total_weight = 0.0;
    state->mean_r = 0.0;
    state->mean_g = 0.0;
    state->mean_b = 0.0;
    for (state->index = 0u;
            state->index < ctx->retained_count;
            ++state->index) {
        state->total_weight += (double)ctx->bins[state->index].count;
        state->mean_r += ctx->bins[state->index].r
            * (double)ctx->bins[state->index].count;
        state->mean_g += ctx->bins[state->index].g
            * (double)ctx->bins[state->index].count;
        state->mean_b += ctx->bins[state->index].b
            * (double)ctx->bins[state->index].count;
    }
    if (state->total_weight > 0.0) {
        state->mean_r /= state->total_weight;
        state->mean_g /= state->total_weight;
        state->mean_b /= state->total_weight;
    }

    for (state->index = 0u;
            state->index < ctx->retained_count;
            ++state->index) {
        state->dr = ctx->bins[state->index].r - state->mean_r;
        state->dg = ctx->bins[state->index].g - state->mean_g;
        state->db = ctx->bins[state->index].b - state->mean_b;
        state->dispersion_score = (double)ctx->bins[state->index].count
            * (state->dr * state->dr
               + state->dg * state->dg
               + state->db * state->db);
        ctx->dispersion[state->index].index = state->index;
        ctx->dispersion[state->index].score = state->dispersion_score;
        if (ctx->use_perceptual_strata) {
            /*
             * Perceptual spaces already encode opponent channels on a/b.
             * Use raw channels so strata bins stay stable even if the
             * frame-wise mean drifts.
             */
            state->chroma_value = sqrt(
                ctx->bins[state->index].g * ctx->bins[state->index].g
                + ctx->bins[state->index].b * ctx->bins[state->index].b);
            ctx->chroma_cache[state->index] = state->chroma_value;
            ctx->chroma_rank[state->index].index = state->index;
            ctx->chroma_rank[state->index].score = state->chroma_value;
        }
    }
    qsort(ctx->dispersion,
          ctx->retained_count,
          sizeof(sixel_kcenter_dispersion_rank_t),
          sixel_kcenter_compare_dispersion_desc);
}

static void
sixel_kcenter_collect_hybrid_build_chroma_edges(
    sixel_kcenter_collect_ctx_t *ctx,
    sixel_kcenter_collect_hybrid_state_t *state)
{
    if (ctx == NULL || state == NULL
            || !ctx->use_perceptual_strata
            || ctx->retained_count == 0u) {
        return;
    }

    /*
     * Keep runtime guards minimal for MSVC /analyze.  The bucket count is
     * currently a fixed compile-time constant.
     */
    qsort(ctx->chroma_rank,
          ctx->retained_count,
          sizeof(sixel_kcenter_dispersion_rank_t),
          sixel_kcenter_compare_dispersion_asc);
    for (state->strata_rank = 0u;
            state->strata_rank + 1u < SIXEL_KCENTER_CHROMA_BUCKETS;
            ++state->strata_rank) {
        state->chroma_target = ctx->retained_weight
            * (double)(state->strata_rank + 1u)
            / (double)SIXEL_KCENTER_CHROMA_BUCKETS;
        state->chroma_accum = 0.0;
        ctx->chroma_edges[state->strata_rank]
            = ctx->chroma_rank[ctx->retained_count - 1u].score;
        state->chroma_edge_found = 0;
        for (state->strata_swap = 0u;
                state->strata_swap < ctx->retained_count;
                ++state->strata_swap) {
            state->bin_index = ctx->chroma_rank[state->strata_swap].index;
            state->chroma_accum += (double)ctx->bins[state->bin_index].count;
            if (state->chroma_accum + 1.0e-12 < state->chroma_target) {
                continue;
            }
            ctx->chroma_edges[state->strata_rank]
                = ctx->chroma_rank[state->strata_swap].score;
            state->chroma_edge_found = 1;
            break;
        }
        if (!state->chroma_edge_found) {
            ctx->chroma_edges[state->strata_rank]
                = ctx->chroma_rank[ctx->retained_count - 1u].score;
        }
    }
}

static void
sixel_kcenter_collect_hybrid_score_strata(
    sixel_kcenter_collect_ctx_t *ctx,
    sixel_kcenter_collect_hybrid_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    sixel_kcenter_collect_reset_strata(ctx);
    for (state->index = 0u;
            state->index < ctx->retained_count;
            ++state->index) {
        if (ctx->use_perceptual_strata) {
            state->luma_value = ctx->bins[state->index].r;
        } else {
            state->luma_value = ctx->bins[state->index].r * 0.299
                + ctx->bins[state->index].g * 0.587
                + ctx->bins[state->index].b * 0.114;
        }
        state->luma_bucket = (unsigned int)(
            (state->luma_value * (double)SIXEL_KCENTER_LUMA_BUCKETS) / 256.0);
        if (state->luma_bucket >= SIXEL_KCENTER_LUMA_BUCKETS) {
            state->luma_bucket = SIXEL_KCENTER_LUMA_BUCKETS - 1u;
        }

        if (ctx->use_perceptual_strata) {
            state->chroma_value = ctx->chroma_cache[state->index];
            if (state->chroma_value <= 1.0e-9) {
                /*
                 * Near-neutral bins have unstable hue.  Pin the hue bucket
                 * instead of amplifying angle noise.
                 */
                state->normalized_hue = 0.0;
            } else {
                state->hue_phase = atan2(ctx->bins[state->index].b,
                                         ctx->bins[state->index].g);
                state->normalized_hue = (state->hue_phase + SIXEL_KCENTER_PI)
                    / (2.0 * SIXEL_KCENTER_PI);
                if (state->normalized_hue < 0.0) {
                    state->normalized_hue = 0.0;
                }
                if (state->normalized_hue >= 1.0) {
                    state->normalized_hue = 0.999999;
                }
            }
            state->dr = ctx->bins[state->index].r - state->mean_r;
            state->chroma_bucket = 0u;
            while (state->chroma_bucket + 1u < SIXEL_KCENTER_CHROMA_BUCKETS) {
                if (state->chroma_value
                        <= ctx->chroma_edges[state->chroma_bucket] + 1.0e-12) {
                    break;
                }
                ++state->chroma_bucket;
            }
            state->dispersion_score = (double)ctx->bins[state->index].count
                * (1.10 * state->dr * state->dr
                   + 1.50 * state->chroma_value * state->chroma_value);
        } else {
            state->max_component = ctx->bins[state->index].r;
            if (ctx->bins[state->index].g > state->max_component) {
                state->max_component = ctx->bins[state->index].g;
            }
            if (ctx->bins[state->index].b > state->max_component) {
                state->max_component = ctx->bins[state->index].b;
            }
            state->min_component = ctx->bins[state->index].r;
            if (ctx->bins[state->index].g < state->min_component) {
                state->min_component = ctx->bins[state->index].g;
            }
            if (ctx->bins[state->index].b < state->min_component) {
                state->min_component = ctx->bins[state->index].b;
            }
            state->delta_component
                = state->max_component - state->min_component;
            state->hue_value = 0.0;
            if (state->delta_component > 1.0e-9) {
                if (state->max_component == ctx->bins[state->index].r) {
                    state->hue_value = (ctx->bins[state->index].g
                        - ctx->bins[state->index].b)
                        / state->delta_component;
                    if (state->hue_value < 0.0) {
                        state->hue_value += 6.0;
                    }
                } else if (state->max_component == ctx->bins[state->index].g) {
                    state->hue_value = 2.0 + (ctx->bins[state->index].b
                        - ctx->bins[state->index].r)
                        / state->delta_component;
                } else {
                    state->hue_value = 4.0 + (ctx->bins[state->index].r
                        - ctx->bins[state->index].g)
                        / state->delta_component;
                }
            }
            state->normalized_hue = state->hue_value / 6.0;
            if (state->normalized_hue < 0.0) {
                state->normalized_hue = 0.0;
            }
            if (state->normalized_hue >= 1.0) {
                state->normalized_hue = 0.999999;
            }
            state->dr = ctx->bins[state->index].r - state->mean_r;
            state->dg = ctx->bins[state->index].g - state->mean_g;
            state->db = ctx->bins[state->index].b - state->mean_b;
            state->dispersion_score = (double)ctx->bins[state->index].count
                * (state->dr * state->dr
                   + state->dg * state->dg
                   + state->db * state->db);
            state->chroma_bucket = 0u;
        }

        state->hue_bucket = (unsigned int)(
            state->normalized_hue * (double)SIXEL_KCENTER_HUE_BUCKETS);
        if (state->hue_bucket >= SIXEL_KCENTER_HUE_BUCKETS) {
            state->hue_bucket = SIXEL_KCENTER_HUE_BUCKETS - 1u;
        }
        state->strata_index = (state->luma_bucket * SIXEL_KCENTER_CHROMA_BUCKETS
            + state->chroma_bucket) * SIXEL_KCENTER_HUE_BUCKETS
            + state->hue_bucket;

        if (state->dispersion_score
                > ctx->strata_scores[state->strata_index] + 1.0e-12) {
            ctx->strata_scores[state->strata_index] = state->dispersion_score;
            ctx->strata_picks[state->strata_index] = state->index;
        } else if (state->dispersion_score
                >= ctx->strata_scores[state->strata_index] - 1.0e-12
                && ctx->strata_picks[state->strata_index] != UINT_MAX
                && state->index < ctx->strata_picks[state->strata_index]) {
            ctx->strata_picks[state->strata_index] = state->index;
        }
    }
}

static SIXELSTATUS
sixel_kcenter_collect_select_hybrid(sixel_kcenter_collect_ctx_t *ctx)
{
    SIXELSTATUS status;
    sixel_kcenter_collect_hybrid_state_t state;

    status = SIXEL_BAD_ALLOCATION;
    sixel_kcenter_collect_hybrid_state_clear(&state);
    if (ctx == NULL) {
        return status;
    }

    /*
     * Keep hybrid priority and tie-break stable:
     * rare -> strata -> dispersion -> fallback.
     */
    sixel_kcenter_collect_hybrid_resolve_space(ctx);
    status = sixel_kcenter_collect_hybrid_allocate_scratch(ctx);
    if (status != SIXEL_OK) {
        return status;
    }
    sixel_kcenter_collect_hybrid_pick_rare(ctx, &state);
    sixel_kcenter_collect_hybrid_build_dispersion(ctx, &state);
    sixel_kcenter_collect_hybrid_build_chroma_edges(ctx, &state);
    sixel_kcenter_collect_hybrid_score_strata(ctx, &state);
    sixel_kcenter_collect_hybrid_pick_strata(ctx);
    sixel_kcenter_collect_hybrid_pick_remaining(ctx);
    sixel_kcenter_collect_hybrid_write_points(ctx);
    return SIXEL_OK;
}

static void
sixel_kcenter_collect_cleanup(sixel_kcenter_collect_ctx_t *ctx)
{
    if (ctx == NULL || ctx->allocator == NULL) {
        return;
    }
    if (ctx->points != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->points);
    }
    if (ctx->weights != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->weights);
    }
    if (ctx->dispersion != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->dispersion);
    }
    if (ctx->chroma_rank != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->chroma_rank);
    }
    if (ctx->chroma_cache != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->chroma_cache);
    }
    if (ctx->bin_selected != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->bin_selected);
    }
    if (ctx->bins != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->bins);
    }
    if (ctx->sums != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->sums);
    }
    if (ctx->counts != NULL) {
        sixel_allocator_free(ctx->allocator, ctx->counts);
    }
}

static SIXELSTATUS
sixel_kcenter_collect_points(double **points_out,
                             double **weights_out,
                             unsigned int *point_count_out,
                             unsigned int *visible_count_out,
                             unsigned int *active_count_out,
                             unsigned char const *data,
                             unsigned int length,
                             unsigned int depth,
                             int pixelformat,
                             int treat_input_as_float32,
                             unsigned int histbits,
                             unsigned int point_budget,
                             double prune_mass,
                             unsigned int reqcolors,
                             int quality_mode,
                             sixel_kcenter_candidate_policy_t
                                 candidate_policy,
                             sixel_kcenter_space_policy_t space_policy,
                             unsigned int rare_keep,
                             sixel_kcenter_budget_policy_t budget_policy,
                             double budget_scale,
                             double const *float32_channel_scale,
                             double const *float32_channel_offset,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_kcenter_collect_ctx_t collect;

    status = SIXEL_BAD_ARGUMENT;
    if (points_out == NULL || weights_out == NULL || point_count_out == NULL
            || visible_count_out == NULL
            || active_count_out == NULL
            || data == NULL
            || allocator == NULL) {
        return status;
    }

    *points_out = NULL;
    *weights_out = NULL;
    *point_count_out = 0u;
    *visible_count_out = 0u;
    *active_count_out = 0u;

    sixel_kcenter_collect_ctx_clear(&collect);
    collect.data = data;
    collect.length = length;
    collect.depth = depth;
    collect.pixelformat = pixelformat;
    collect.treat_input_as_float32 = treat_input_as_float32;
    collect.histbits = histbits;
    collect.point_budget = point_budget;
    collect.prune_mass = prune_mass;
    collect.reqcolors = reqcolors;
    collect.quality_mode = quality_mode;
    collect.candidate_policy = candidate_policy;
    collect.space_policy = space_policy;
    collect.rare_keep = rare_keep;
    collect.budget_policy = budget_policy;
    collect.budget_scale = budget_scale;
    collect.float32_channel_scale = float32_channel_scale;
    collect.float32_channel_offset = float32_channel_offset;
    collect.allocator = allocator;

    /*
     * Keep the stage order stable to preserve tie-break behavior:
     * histogram -> prune/stats -> budget -> candidate selection.
     */
    status = sixel_kcenter_collect_prepare(&collect);
    if (status != SIXEL_OK) {
        goto end;
    }
    if (collect.pixel_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    status = sixel_kcenter_collect_build_histogram(&collect);
    if (status != SIXEL_OK) {
        goto end;
    }
    *visible_count_out = collect.visible_count;
    if (collect.visible_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    status = sixel_kcenter_collect_build_bins_and_stats(&collect);
    if (status != SIXEL_OK) {
        goto end;
    }
    *active_count_out = collect.retained_count;
    if (collect.retained_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    sixel_kcenter_collect_resolve_budget(&collect);
    status = sixel_kcenter_collect_allocate_output_buffers(&collect);
    if (status != SIXEL_OK) {
        goto end;
    }

    if (sixel_kcenter_resolve_candidate_policy(collect.candidate_policy)
            == SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID) {
        status = sixel_kcenter_collect_select_hybrid(&collect);
        if (status != SIXEL_OK) {
            goto end;
        }
    } else {
        sixel_kcenter_collect_select_legacy(&collect);
    }

    *points_out = collect.points;
    *weights_out = collect.weights;
    *point_count_out = collect.budget;
    collect.points = NULL;
    collect.weights = NULL;
    status = SIXEL_OK;

end:
    sixel_kcenter_collect_cleanup(&collect);
    return status;
}

static double
sixel_kcenter_distance_sq(double const *points,
                          unsigned int left,
                          unsigned int right)
{
    double dr;
    double dg;
    double db;

    dr = points[left * 3u + 0u] - points[right * 3u + 0u];
    dg = points[left * 3u + 1u] - points[right * 3u + 1u];
    db = points[left * 3u + 2u] - points[right * 3u + 2u];
    return dr * dr + dg * dg + db * db;
}

static void
sixel_kcenter_assign_points(double const *points,
                            double const *weights,
                            unsigned int point_count,
                            unsigned int const *centers,
                            unsigned int k,
                            unsigned int *nearest_slot,
                            double *nearest_dist,
                            double *radius2_out,
                            double *sse_out,
                            double *cluster_weights,
                            double *cluster_sums)
{
    unsigned int index;
    unsigned int slot;
    unsigned int best_slot;
    unsigned int center_index;
    double distance;
    double best_distance;
    double radius2;
    double sse;
    double weight_value;

    index = 0u;
    slot = 0u;
    best_slot = 0u;
    center_index = 0u;
    distance = 0.0;
    best_distance = 0.0;
    radius2 = 0.0;
    sse = 0.0;
    weight_value = 0.0;

    if (cluster_weights != NULL) {
        memset(cluster_weights, 0, (size_t)k * sizeof(double));
    }
    if (cluster_sums != NULL) {
        memset(cluster_sums, 0, (size_t)k * 3u * sizeof(double));
    }

    for (index = 0u; index < point_count; ++index) {
        best_slot = 0u;
        best_distance = sixel_kcenter_distance_sq(points, index, centers[0u]);
        for (slot = 1u; slot < k; ++slot) {
            center_index = centers[slot];
            distance = sixel_kcenter_distance_sq(points, index, center_index);
            if (distance < best_distance) {
                best_distance = distance;
                best_slot = slot;
            }
        }

        if (nearest_slot != NULL) {
            nearest_slot[index] = best_slot;
        }
        if (nearest_dist != NULL) {
            nearest_dist[index] = best_distance;
        }
        if (best_distance > radius2) {
            radius2 = best_distance;
        }

        weight_value = 1.0;
        if (weights != NULL) {
            weight_value = weights[index];
            if (weight_value <= 0.0) {
                weight_value = 1.0;
            }
        }
        sse += best_distance * weight_value;

        if (cluster_weights != NULL) {
            cluster_weights[best_slot] += weight_value;
        }
        if (cluster_sums != NULL) {
            cluster_sums[best_slot * 3u + 0u]
                += points[index * 3u + 0u] * weight_value;
            cluster_sums[best_slot * 3u + 1u]
                += points[index * 3u + 1u] * weight_value;
            cluster_sums[best_slot * 3u + 2u]
                += points[index * 3u + 2u] * weight_value;
        }
    }

    if (radius2_out != NULL) {
        *radius2_out = radius2;
    }
    if (sse_out != NULL) {
        *sse_out = sse;
    }
}

/*
 * Evaluate one center-set candidate with a hard radius cutoff.
 * Return 1 when the candidate stayed within the cutoff, 0 otherwise.
 */
static int
sixel_kcenter_assign_points_with_cutoff(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *centers,
    unsigned int k,
    double allowed_radius2,
    double *radius2_out,
    double *sse_out)
{
    unsigned int index;
    unsigned int slot;
    unsigned int center_index;
    double distance;
    double best_distance;
    double radius2;
    double sse;
    double weight_value;

    index = 0u;
    slot = 0u;
    center_index = 0u;
    distance = 0.0;
    best_distance = 0.0;
    radius2 = 0.0;
    sse = 0.0;
    weight_value = 0.0;

    if (points == NULL || centers == NULL || point_count == 0u || k == 0u
            || radius2_out == NULL || sse_out == NULL) {
        return 0;
    }

    for (index = 0u; index < point_count; ++index) {
        best_distance = sixel_kcenter_distance_sq(points, index, centers[0u]);
        for (slot = 1u; slot < k; ++slot) {
            center_index = centers[slot];
            distance = sixel_kcenter_distance_sq(points, index, center_index);
            if (distance < best_distance) {
                best_distance = distance;
            }
        }

        if (best_distance > radius2) {
            radius2 = best_distance;
            if (radius2 > allowed_radius2 + 1.0e-12) {
                return 0;
            }
        }
        weight_value = 1.0;
        if (weights != NULL) {
            weight_value = weights[index];
            if (weight_value <= 0.0) {
                weight_value = 1.0;
            }
        }
        sse += best_distance * weight_value;
    }

    *radius2_out = radius2;
    *sse_out = sse;
    return 1;
}

static void
sixel_kcenter_assign_points_with_second(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *centers,
    unsigned int k,
    unsigned int *nearest_slot,
    double *nearest_dist,
    unsigned int *second_slot,
    double *second_dist,
    double *radius2_out,
    double *sse_out,
    double *cluster_weights,
    double *cluster_sums)
{
    unsigned int index;
    unsigned int slot;
    unsigned int best_slot;
    unsigned int second_best_slot;
    unsigned int center_index;
    double distance;
    double best_distance;
    double second_best_distance;
    double radius2;
    double sse;
    double weight_value;

    index = 0u;
    slot = 0u;
    best_slot = 0u;
    second_best_slot = 0u;
    center_index = 0u;
    distance = 0.0;
    best_distance = 0.0;
    second_best_distance = 0.0;
    radius2 = 0.0;
    sse = 0.0;
    weight_value = 0.0;

    if (cluster_weights != NULL) {
        memset(cluster_weights, 0, (size_t)k * sizeof(double));
    }
    if (cluster_sums != NULL) {
        memset(cluster_sums, 0, (size_t)k * 3u * sizeof(double));
    }

    for (index = 0u; index < point_count; ++index) {
        best_slot = 0u;
        best_distance = sixel_kcenter_distance_sq(points, index, centers[0u]);
        second_best_slot = 0u;
        second_best_distance = DBL_MAX;
        for (slot = 1u; slot < k; ++slot) {
            center_index = centers[slot];
            distance = sixel_kcenter_distance_sq(points, index, center_index);
            if (distance < best_distance) {
                second_best_distance = best_distance;
                second_best_slot = best_slot;
                best_distance = distance;
                best_slot = slot;
            } else if (distance < second_best_distance) {
                second_best_distance = distance;
                second_best_slot = slot;
            }
        }
        if (k == 1u) {
            second_best_distance = best_distance;
            second_best_slot = best_slot;
        }

        if (nearest_slot != NULL) {
            nearest_slot[index] = best_slot;
        }
        if (nearest_dist != NULL) {
            nearest_dist[index] = best_distance;
        }
        if (second_slot != NULL) {
            second_slot[index] = second_best_slot;
        }
        if (second_dist != NULL) {
            second_dist[index] = second_best_distance;
        }
        if (best_distance > radius2) {
            radius2 = best_distance;
        }

        weight_value = 1.0;
        if (weights != NULL) {
            weight_value = weights[index];
            if (weight_value <= 0.0) {
                weight_value = 1.0;
            }
        }
        sse += best_distance * weight_value;

        if (cluster_weights != NULL) {
            cluster_weights[best_slot] += weight_value;
        }
        if (cluster_sums != NULL) {
            cluster_sums[best_slot * 3u + 0u]
                += points[index * 3u + 0u] * weight_value;
            cluster_sums[best_slot * 3u + 1u]
                += points[index * 3u + 1u] * weight_value;
            cluster_sums[best_slot * 3u + 2u]
                += points[index * 3u + 2u] * weight_value;
        }
    }

    if (radius2_out != NULL) {
        *radius2_out = radius2;
    }
    if (sse_out != NULL) {
        *sse_out = sse;
    }
}

/*
 * Dispatch assignment through the second-nearest path when second buffers are
 * available, otherwise use nearest-only assignment.
 */
static void
sixel_kcenter_assign_points_dispatch(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int const *centers,
    unsigned int k,
    unsigned int *nearest_slot,
    double *nearest_dist,
    unsigned int *second_slot,
    double *second_dist,
    double *radius2_out,
    double *sse_out,
    double *cluster_weights,
    double *cluster_sums)
{
    if (second_slot != NULL && second_dist != NULL) {
        sixel_kcenter_assign_points_with_second(points,
                                                weights,
                                                point_count,
                                                centers,
                                                k,
                                                nearest_slot,
                                                nearest_dist,
                                                second_slot,
                                                second_dist,
                                                radius2_out,
                                                sse_out,
                                                cluster_weights,
                                                cluster_sums);
        return;
    }

    sixel_kcenter_assign_points(points,
                                weights,
                                point_count,
                                centers,
                                k,
                                nearest_slot,
                                nearest_dist,
                                radius2_out,
                                sse_out,
                                cluster_weights,
                                cluster_sums);
}

/*
 * One-pass SSE polish under a strict radius cap.  A candidate is accepted only
 * when the radius does not worsen and SSE strictly improves.
 */
static void
sixel_kcenter_polish_sse_with_radius_guard(
    sixel_kcenter_polish_ctx_t *ctx,
    unsigned int *updates_io)
{
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int *centers;
    unsigned int k;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    unsigned char *center_mask;
    double *cluster_weights;
    double *cluster_sums;
    double *radius2_io;
    double *sse_io;
    unsigned int slot;
    unsigned int index;
    unsigned int old_center;
    unsigned int candidate;
    double target_r;
    double target_g;
    double target_b;
    double distance;
    double candidate_dist;
    double weight_value;
    double radius_limit2;
    double best_sse;
    double trial_radius2;
    double trial_sse;
    unsigned int updates;
    int accepted;

    points = NULL;
    weights = NULL;
    point_count = 0u;
    centers = NULL;
    k = 0u;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_slot = NULL;
    second_dist = NULL;
    scratch_slot = NULL;
    scratch_dist = NULL;
    scratch_second_slot = NULL;
    scratch_second_dist = NULL;
    center_mask = NULL;
    cluster_weights = NULL;
    cluster_sums = NULL;
    radius2_io = NULL;
    sse_io = NULL;
    slot = 0u;
    index = 0u;
    old_center = 0u;
    candidate = 0u;
    target_r = 0.0;
    target_g = 0.0;
    target_b = 0.0;
    distance = 0.0;
    candidate_dist = 0.0;
    weight_value = 0.0;
    radius_limit2 = 0.0;
    best_sse = 0.0;
    trial_radius2 = 0.0;
    trial_sse = 0.0;
    updates = 0u;
    accepted = 0;

    if (ctx == NULL) {
        if (updates_io != NULL) {
            *updates_io = 0u;
        }
        return;
    }

    points = ctx->points;
    weights = ctx->weights;
    point_count = ctx->point_count;
    centers = ctx->centers;
    k = ctx->k;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    second_slot = ctx->second_slot;
    second_dist = ctx->second_dist;
    scratch_slot = ctx->scratch_slot;
    scratch_dist = ctx->scratch_dist;
    scratch_second_slot = ctx->scratch_second_slot;
    scratch_second_dist = ctx->scratch_second_dist;
    center_mask = ctx->center_mask;
    cluster_weights = ctx->cluster_weights;
    cluster_sums = ctx->cluster_sums;
    radius2_io = ctx->radius2_io;
    sse_io = ctx->sse_io;

    if (points == NULL || centers == NULL || nearest_slot == NULL
            || nearest_dist == NULL || second_slot == NULL
            || second_dist == NULL || scratch_slot == NULL
            || scratch_dist == NULL || scratch_second_slot == NULL
            || scratch_second_dist == NULL || center_mask == NULL
            || cluster_weights == NULL || cluster_sums == NULL
            || radius2_io == NULL || sse_io == NULL
            || point_count == 0u || k == 0u) {
        if (updates_io != NULL) {
            *updates_io = 0u;
        }
        return;
    }

    radius_limit2 = *radius2_io;
    best_sse = *sse_io;
    for (slot = 0u; slot < k; ++slot) {
        weight_value = cluster_weights[slot];
        if (weight_value <= 0.0) {
            continue;
        }
        target_r = cluster_sums[slot * 3u + 0u] / weight_value;
        target_g = cluster_sums[slot * 3u + 1u] / weight_value;
        target_b = cluster_sums[slot * 3u + 2u] / weight_value;

        candidate = centers[slot];
        candidate_dist = DBL_MAX;
        for (index = 0u; index < point_count; ++index) {
            if (nearest_slot[index] != slot) {
                continue;
            }
            distance = points[index * 3u + 0u] - target_r;
            distance = distance * distance;
            weight_value = points[index * 3u + 1u] - target_g;
            distance += weight_value * weight_value;
            weight_value = points[index * 3u + 2u] - target_b;
            distance += weight_value * weight_value;
            if (distance < candidate_dist - 1.0e-12) {
                candidate = index;
                candidate_dist = distance;
                continue;
            }
            if (distance <= candidate_dist + 1.0e-12 && index < candidate) {
                candidate = index;
                candidate_dist = distance;
            }
        }
        if (candidate == centers[slot]) {
            continue;
        }
        if (center_mask[candidate] != 0u) {
            continue;
        }

        old_center = centers[slot];
        centers[slot] = candidate;
        sixel_kcenter_assign_points_with_second(points,
                                                weights,
                                                point_count,
                                                centers,
                                                k,
                                                scratch_slot,
                                                scratch_dist,
                                                scratch_second_slot,
                                                scratch_second_dist,
                                                &trial_radius2,
                                                &trial_sse,
                                                NULL,
                                                NULL);
        if (trial_radius2 <= radius_limit2 + 1.0e-12
                && trial_sse < best_sse - 1.0e-9) {
            center_mask[old_center] = 0u;
            center_mask[candidate] = 1u;
            memcpy(nearest_slot,
                   scratch_slot,
                   (size_t)point_count * sizeof(unsigned int));
            memcpy(nearest_dist,
                   scratch_dist,
                   (size_t)point_count * sizeof(double));
            memcpy(second_slot,
                   scratch_second_slot,
                   (size_t)point_count * sizeof(unsigned int));
            memcpy(second_dist,
                   scratch_second_dist,
                   (size_t)point_count * sizeof(double));
            *radius2_io = trial_radius2;
            *sse_io = trial_sse;
            best_sse = trial_sse;
            ++updates;
            accepted = 1;
            sixel_kcenter_assign_points_with_second(points,
                                                    weights,
                                                    point_count,
                                                    centers,
                                                    k,
                                                    nearest_slot,
                                                    nearest_dist,
                                                    second_slot,
                                                    second_dist,
                                                    &trial_radius2,
                                                    &trial_sse,
                                                    cluster_weights,
                                                    cluster_sums);
            continue;
        }
        centers[slot] = old_center;
    }

    if (updates_io != NULL) {
        *updates_io = updates;
    }
    if (!accepted) {
        return;
    }
}

static unsigned int
sixel_kcenter_pick_weighted_seed(double const *weights,
                                 unsigned int point_count,
                                 uint32_t *rng_state)
{
    unsigned int index;
    double total;
    double threshold;
    double scale;

    index = 0u;
    total = 0.0;
    threshold = 0.0;
    scale = 0.0;

    if (weights == NULL || point_count == 0u) {
        return 0u;
    }

    for (index = 0u; index < point_count; ++index) {
        if (weights[index] > 0.0) {
            total += weights[index];
        }
    }
    if (total <= 0.0) {
        return sixel_kcenter_rng_bounded(rng_state, point_count);
    }

    scale = (double)sixel_kcenter_rng_next(rng_state)
        / (double)UINT_MAX;
    threshold = scale * total;
    total = 0.0;
    for (index = 0u; index < point_count; ++index) {
        if (weights[index] <= 0.0) {
            continue;
        }
        total += weights[index];
        if (total >= threshold) {
            return index;
        }
    }

    return point_count - 1u;
}

static void
sixel_kcenter_choose_fft(double const *points,
                         double const *weights,
                         unsigned int point_count,
                         unsigned int k,
                         uint32_t *rng_state,
                         unsigned int *centers,
                         double *dist2_cache,
                         unsigned char *center_mask)
{
    unsigned int slot;
    unsigned int index;
    unsigned int selected;
    unsigned int tie_index;
    double distance;
    double best_distance;
    double tie_weight;
    double weight_value;

    slot = 0u;
    index = 0u;
    selected = 0u;
    tie_index = 0u;
    distance = 0.0;
    best_distance = 0.0;
    tie_weight = 0.0;
    weight_value = 0.0;

    if (k == 0u || point_count == 0u) {
        return;
    }

    memset(center_mask, 0, (size_t)point_count);
    centers[0u] = sixel_kcenter_pick_weighted_seed(weights,
                                                    point_count,
                                                    rng_state);
    center_mask[centers[0u]] = 1u;

    for (index = 0u; index < point_count; ++index) {
        dist2_cache[index] = sixel_kcenter_distance_sq(points,
                                                       index,
                                                       centers[0u]);
    }

    for (slot = 1u; slot < k; ++slot) {
        selected = 0u;
        best_distance = -1.0;
        tie_weight = -1.0;
        tie_index = 0u;
        for (index = 0u; index < point_count; ++index) {
            if (center_mask[index]) {
                continue;
            }
            distance = dist2_cache[index];
            if (distance > best_distance + 1.0e-12) {
                best_distance = distance;
                tie_weight = (weights != NULL) ? weights[index] : 1.0;
                tie_index = index;
                selected = 1u;
            } else if (distance >= best_distance - 1.0e-12) {
                weight_value = (weights != NULL) ? weights[index] : 1.0;
                if (weight_value > tie_weight + 1.0e-12) {
                    tie_weight = weight_value;
                    tie_index = index;
                    selected = 1u;
                } else if (weight_value >= tie_weight - 1.0e-12
                        && index < tie_index) {
                    tie_index = index;
                    selected = 1u;
                }
            }
        }
        if (!selected) {
            tie_index = sixel_kcenter_rng_bounded(rng_state, point_count);
        }
        centers[slot] = tie_index;
        center_mask[tie_index] = 1u;

        for (index = 0u; index < point_count; ++index) {
            distance = sixel_kcenter_distance_sq(points, index, tie_index);
            if (distance < dist2_cache[index]) {
                dist2_cache[index] = distance;
            }
        }
    }
}

static void
sixel_kcenter_choose_random(unsigned int point_count,
                            unsigned int k,
                            uint32_t *rng_state,
                            unsigned int *centers,
                            unsigned int *scratch_indices)
{
    unsigned int index;
    unsigned int pick;
    unsigned int remaining;
    unsigned int temp;

    index = 0u;
    pick = 0u;
    remaining = 0u;
    temp = 0u;

    for (index = 0u; index < point_count; ++index) {
        scratch_indices[index] = index;
    }

    for (index = 0u; index < k; ++index) {
        remaining = point_count - index;
        pick = index + sixel_kcenter_rng_bounded(rng_state, remaining);
        temp = scratch_indices[index];
        scratch_indices[index] = scratch_indices[pick];
        scratch_indices[pick] = temp;
        centers[index] = scratch_indices[index];
    }
}

static void
sixel_kcenter_refresh_center_mask(unsigned char *center_mask,
                                  unsigned int point_count,
                                  unsigned int const *centers,
                                  unsigned int k)
{
    unsigned int slot;

    slot = 0u;
    if (center_mask == NULL || centers == NULL || point_count == 0u) {
        return;
    }

    memset(center_mask, 0, (size_t)point_count);
    for (slot = 0u; slot < k; ++slot) {
        if (centers[slot] >= point_count) {
            continue;
        }
        center_mask[centers[slot]] = 1u;
    }
}

static int
sixel_kcenter_swap_candidate_present(unsigned int const *candidate_list,
                                     unsigned int candidate_count,
                                     unsigned int candidate)
{
    unsigned int index;

    if (candidate_list == NULL) {
        return 0;
    }
    for (index = 0u; index < candidate_count; ++index) {
        if (candidate_list[index] == candidate) {
            return 1;
        }
    }
    return 0;
}

static void
sixel_kcenter_swap_insert_candidate(unsigned int *candidate_list,
                                    double *candidate_dist,
                                    double *candidate_weight,
                                    unsigned int *candidate_count_io,
                                    unsigned int max_count,
                                    unsigned int candidate,
                                    double distance,
                                    double weight)
{
    unsigned int insert_pos;
    unsigned int shift;
    double rhs_weight;
    unsigned int candidate_count;

    insert_pos = 0u;
    shift = 0u;
    rhs_weight = 0.0;
    candidate_count = 0u;
    if (candidate_list == NULL || candidate_dist == NULL
            || candidate_weight == NULL || candidate_count_io == NULL
            || max_count == 0u) {
        return;
    }
    if (weight <= 0.0) {
        weight = 1.0;
    }

    candidate_count = *candidate_count_io;
    insert_pos = candidate_count;
    while (insert_pos > 0u) {
        rhs_weight = candidate_weight[insert_pos - 1u];
        if (distance > candidate_dist[insert_pos - 1u] + 1.0e-12) {
            --insert_pos;
            continue;
        }
        if (distance >= candidate_dist[insert_pos - 1u] - 1.0e-12
                && (weight > rhs_weight + 1.0e-12
                    || (weight >= rhs_weight - 1.0e-12
                        && candidate < candidate_list[insert_pos - 1u]))) {
            --insert_pos;
            continue;
        }
        break;
    }
    if (insert_pos >= max_count) {
        return;
    }
    if (candidate_count < max_count) {
        ++candidate_count;
    }
    shift = candidate_count;
    while (shift > insert_pos + 1u) {
        candidate_list[shift - 1u] = candidate_list[shift - 2u];
        candidate_dist[shift - 1u] = candidate_dist[shift - 2u];
        candidate_weight[shift - 1u] = candidate_weight[shift - 2u];
        --shift;
    }
    candidate_list[insert_pos] = candidate;
    candidate_dist[insert_pos] = distance;
    candidate_weight[insert_pos] = weight;
    *candidate_count_io = candidate_count;
}

static double
sixel_kcenter_swap_allowed_radius_sq(double best_radius2,
                                     double swap_min_gain)
{
    double allowed_radius;

    allowed_radius = 0.0;
    if (best_radius2 < 0.0) {
        return DBL_MAX;
    }
    if (swap_min_gain <= 0.0) {
        return best_radius2 + 1.0e-12;
    }
    allowed_radius = sqrt(best_radius2) - swap_min_gain + 1.0e-12;
    if (allowed_radius <= 0.0) {
        return 0.0;
    }
    return allowed_radius * allowed_radius;
}

static int
sixel_kcenter_swap_candidate_is_better(double candidate_radius2,
                                       double candidate_sse,
                                       double best_radius2,
                                       double best_sse,
                                       double allowed_radius2)
{
    if (candidate_radius2 > allowed_radius2 + 1.0e-12) {
        return 0;
    }
    if (candidate_radius2 < best_radius2 - 1.0e-12) {
        return 1;
    }
    if (candidate_radius2 <= best_radius2 + 1.0e-12
            && candidate_sse < best_sse - 1.0e-9) {
        return 1;
    }
    return 0;
}

/*
 * Evaluate one swap candidate in O(point_count) using cached nearest and
 * second-nearest distances.  This stays exact because only one center slot is
 * replaced: points in that slot fall back to second-nearest, and all other
 * points keep their old nearest unless the new center is closer.
 */
static int
sixel_kcenter_swap_eval_with_cutoff(
    double const *points,
    double const *weights,
    unsigned int point_count,
    unsigned int slot,
    unsigned int candidate,
    unsigned int const *nearest_slot,
    double const *nearest_dist,
    double const *second_dist,
    double allowed_radius2,
    double best_radius2,
    double best_sse,
    double *distance_to_new_out,
    double *radius2_out,
    double *sse_out)
{
    unsigned int index;
    double radius2;
    double sse;
    double old_distance;
    double old_second;
    double new_distance;
    double distance_to_new;
    double lhs_weight;

    index = 0u;
    radius2 = 0.0;
    sse = 0.0;
    old_distance = 0.0;
    old_second = 0.0;
    new_distance = 0.0;
    distance_to_new = 0.0;
    lhs_weight = 0.0;

    if (points == NULL
            || nearest_slot == NULL
            || nearest_dist == NULL
            || second_dist == NULL
            || radius2_out == NULL
            || sse_out == NULL
            || point_count == 0u) {
        return 0;
    }

    for (index = 0u; index < point_count; ++index) {
        old_distance = nearest_dist[index];
        old_second = second_dist[index];
        distance_to_new = sixel_kcenter_distance_sq(points,
                                                    index,
                                                    candidate);
        if (distance_to_new_out != NULL) {
            distance_to_new_out[index] = distance_to_new;
        }
        if (nearest_slot[index] == slot) {
            if (distance_to_new < old_second) {
                new_distance = distance_to_new;
            } else {
                new_distance = old_second;
            }
        } else if (distance_to_new < old_distance) {
            new_distance = distance_to_new;
        } else {
            new_distance = old_distance;
        }

        if (new_distance > radius2) {
            radius2 = new_distance;
            if (radius2 > allowed_radius2 + 1.0e-12) {
                return 0;
            }
        }
        lhs_weight = (weights != NULL) ? weights[index] : 1.0;
        if (lhs_weight <= 0.0) {
            lhs_weight = 1.0;
        }
        sse += new_distance * lhs_weight;
        if (radius2 >= best_radius2 - 1.0e-12
                && sse >= best_sse - 1.0e-9) {
            return 0;
        }
    }

    *radius2_out = radius2;
    *sse_out = sse;
    return 1;
}

/*
 * Apply an accepted swap and refresh nearest/second assignments using cached
 * nearest-neighbor state.  This keeps the update exact while avoiding a full
 * O(point_count * k) reassignment pass.
 */
static int
sixel_kcenter_swap_apply_with_second(sixel_kcenter_swap_apply_ctx_t *ctx)
{
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int *centers;
    unsigned int k;
    unsigned int swapped_slot;
    unsigned int swapped_center;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    double const *new_center_dist2;
    double *radius2_out;
    double *sse_out;
    unsigned int old_slot;
    unsigned int scan_slot;
    unsigned int old_nearest_slot;
    unsigned int old_second_slot;
    unsigned int scan_best_slot;
    unsigned int scan_second_slot;
    double old_distance;
    double old_second;
    double scan_best_dist;
    double scan_second_dist;
    double scan_distance;
    double distance_to_new;
    double radius2;
    double sse;
    double lhs_weight;

    old_slot = 0u;
    scan_slot = 0u;
    old_nearest_slot = 0u;
    old_second_slot = 0u;
    scan_best_slot = 0u;
    scan_second_slot = 0u;
    old_distance = 0.0;
    old_second = 0.0;
    scan_best_dist = 0.0;
    scan_second_dist = 0.0;
    scan_distance = 0.0;
    distance_to_new = 0.0;
    radius2 = 0.0;
    sse = 0.0;
    lhs_weight = 0.0;

    if (ctx == NULL) {
        return 0;
    }
    points = ctx->points;
    weights = ctx->weights;
    point_count = ctx->point_count;
    centers = ctx->centers;
    k = ctx->k;
    swapped_slot = ctx->swapped_slot;
    swapped_center = ctx->swapped_center;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    second_slot = ctx->second_slot;
    second_dist = ctx->second_dist;
    scratch_slot = ctx->scratch_slot;
    scratch_dist = ctx->scratch_dist;
    scratch_second_slot = ctx->scratch_second_slot;
    scratch_second_dist = ctx->scratch_second_dist;
    new_center_dist2 = ctx->new_center_dist2;
    radius2_out = ctx->radius2_out;
    sse_out = ctx->sse_out;

    if (points == NULL
            || centers == NULL
            || nearest_slot == NULL
            || nearest_dist == NULL
            || second_slot == NULL
            || second_dist == NULL
            || scratch_slot == NULL
            || scratch_dist == NULL
            || scratch_second_slot == NULL
            || scratch_second_dist == NULL
            || radius2_out == NULL
            || sse_out == NULL
            || point_count == 0u
            || k == 0u) {
        return 0;
    }

    for (old_slot = 0u; old_slot < point_count; ++old_slot) {
        old_nearest_slot = nearest_slot[old_slot];
        if (old_nearest_slot == swapped_slot) {
            scan_best_slot = 0u;
            scan_second_slot = 0u;
            scan_best_dist = sixel_kcenter_distance_sq(points,
                                                       old_slot,
                                                       centers[0u]);
            scan_second_dist = DBL_MAX;
            for (scan_slot = 1u; scan_slot < k; ++scan_slot) {
                scan_distance = sixel_kcenter_distance_sq(points,
                                                          old_slot,
                                                          centers[scan_slot]);
                if (scan_distance < scan_best_dist) {
                    scan_second_dist = scan_best_dist;
                    scan_second_slot = scan_best_slot;
                    scan_best_dist = scan_distance;
                    scan_best_slot = scan_slot;
                } else if (scan_distance < scan_second_dist) {
                    scan_second_dist = scan_distance;
                    scan_second_slot = scan_slot;
                }
            }
            if (k == 1u) {
                scan_second_slot = scan_best_slot;
                scan_second_dist = scan_best_dist;
            }
            scratch_slot[old_slot] = scan_best_slot;
            scratch_dist[old_slot] = scan_best_dist;
            scratch_second_slot[old_slot] = scan_second_slot;
            scratch_second_dist[old_slot] = scan_second_dist;
        } else if (second_slot[old_slot] == swapped_slot) {
            old_distance = nearest_dist[old_slot];
            if (new_center_dist2 != NULL) {
                distance_to_new = new_center_dist2[old_slot];
            } else {
                distance_to_new = sixel_kcenter_distance_sq(points,
                                                            old_slot,
                                                            swapped_center);
            }
            if (distance_to_new < old_distance) {
                scratch_slot[old_slot] = swapped_slot;
                scratch_dist[old_slot] = distance_to_new;
                scratch_second_slot[old_slot] = old_nearest_slot;
                scratch_second_dist[old_slot] = old_distance;
            } else {
                scratch_slot[old_slot] = old_nearest_slot;
                scratch_dist[old_slot] = old_distance;
                if (k == 1u) {
                    scratch_second_slot[old_slot] = scratch_slot[old_slot];
                    scratch_second_dist[old_slot] = scratch_dist[old_slot];
                } else {
                    scan_second_slot = scratch_slot[old_slot];
                    scan_second_dist = DBL_MAX;
                    for (scan_slot = 0u; scan_slot < k; ++scan_slot) {
                        if (scan_slot == scratch_slot[old_slot]) {
                            continue;
                        }
                        scan_distance = sixel_kcenter_distance_sq(
                            points,
                            old_slot,
                            centers[scan_slot]);
                        if (scan_distance < scan_second_dist) {
                            scan_second_dist = scan_distance;
                            scan_second_slot = scan_slot;
                        }
                    }
                    if (scan_second_dist == DBL_MAX) {
                        scan_second_slot = scratch_slot[old_slot];
                        scan_second_dist = scratch_dist[old_slot];
                    }
                    scratch_second_slot[old_slot] = scan_second_slot;
                    scratch_second_dist[old_slot] = scan_second_dist;
                }
            }
        } else {
            old_second_slot = second_slot[old_slot];
            old_distance = nearest_dist[old_slot];
            old_second = second_dist[old_slot];
            if (new_center_dist2 != NULL) {
                distance_to_new = new_center_dist2[old_slot];
            } else {
                distance_to_new = sixel_kcenter_distance_sq(points,
                                                            old_slot,
                                                            swapped_center);
            }
            if (distance_to_new < old_distance) {
                scratch_slot[old_slot] = swapped_slot;
                scratch_dist[old_slot] = distance_to_new;
                scratch_second_slot[old_slot] = old_nearest_slot;
                scratch_second_dist[old_slot] = old_distance;
            } else {
                scratch_slot[old_slot] = old_nearest_slot;
                scratch_dist[old_slot] = old_distance;
                if (distance_to_new < old_second) {
                    scratch_second_slot[old_slot] = swapped_slot;
                    scratch_second_dist[old_slot] = distance_to_new;
                } else {
                    scratch_second_slot[old_slot] = old_second_slot;
                    scratch_second_dist[old_slot] = old_second;
                }
            }
        }

        nearest_slot[old_slot] = scratch_slot[old_slot];
        nearest_dist[old_slot] = scratch_dist[old_slot];
        second_slot[old_slot] = scratch_second_slot[old_slot];
        second_dist[old_slot] = scratch_second_dist[old_slot];
        if (nearest_dist[old_slot] > radius2) {
            radius2 = nearest_dist[old_slot];
        }
        lhs_weight = (weights != NULL) ? weights[old_slot] : 1.0;
        if (lhs_weight <= 0.0) {
            lhs_weight = 1.0;
        }
        sse += nearest_dist[old_slot] * lhs_weight;
    }

    *radius2_out = radius2;
    *sse_out = sse;
    return 1;
}

static void
sixel_kcenter_swap_prepare_apply_ctx(
    sixel_kcenter_swap_apply_ctx_t *apply_ctx,
    sixel_kcenter_swap_ctx_t const *swap_ctx,
    unsigned int swapped_slot,
    unsigned int swapped_center,
    double const *new_center_dist2,
    double *radius2_out,
    double *sse_out)
{
    if (apply_ctx == NULL || swap_ctx == NULL) {
        return;
    }
    apply_ctx->points = swap_ctx->points;
    apply_ctx->weights = swap_ctx->weights;
    apply_ctx->point_count = swap_ctx->point_count;
    apply_ctx->centers = swap_ctx->centers;
    apply_ctx->k = swap_ctx->k;
    apply_ctx->swapped_slot = swapped_slot;
    apply_ctx->swapped_center = swapped_center;
    apply_ctx->nearest_slot = swap_ctx->nearest_slot;
    apply_ctx->nearest_dist = swap_ctx->nearest_dist;
    apply_ctx->second_slot = swap_ctx->second_slot;
    apply_ctx->second_dist = swap_ctx->second_dist;
    apply_ctx->scratch_slot = swap_ctx->scratch_slot;
    apply_ctx->scratch_dist = swap_ctx->scratch_dist;
    apply_ctx->scratch_second_slot = swap_ctx->scratch_second_slot;
    apply_ctx->scratch_second_dist = swap_ctx->scratch_second_dist;
    apply_ctx->new_center_dist2 = new_center_dist2;
    apply_ctx->radius2_out = radius2_out;
    apply_ctx->sse_out = sse_out;
}

typedef struct sixel_kcenter_swap_candidate_state {
    unsigned int point_count;
    unsigned int k;
    unsigned int topk;
    double const *weights;
    unsigned char const *center_mask;
    unsigned int const *nearest_slot;
    double const *nearest_dist;
    unsigned int global_list[16];
    double global_dist[16];
    double global_weight[16];
    unsigned int cluster_list[16];
    double cluster_dist[16];
    double cluster_weight[16];
    unsigned int cluster_slot_list[16];
    unsigned int cluster_best_list[16];
    double cluster_best_dist_list[16];
    double cluster_best_weight_list[16];
    unsigned int candidate_count;
    unsigned int global_count;
    unsigned int cluster_count;
    unsigned int cluster_slot_count;
    unsigned int global_index;
    unsigned int cluster_index;
    unsigned int selected_source;
} sixel_kcenter_swap_candidate_state_t;

static void
sixel_kcenter_swap_candidate_state_clear(
    sixel_kcenter_swap_candidate_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

static int
sixel_kcenter_swap_collect_resolve_inputs(
    sixel_kcenter_swap_ctx_t const *ctx,
    unsigned int topk,
    unsigned int const *candidate_list,
    sixel_kcenter_swap_candidate_state_t *state)
{
    if (ctx == NULL || candidate_list == NULL || state == NULL || topk == 0u) {
        return 0;
    }

    state->point_count = ctx->point_count;
    state->k = ctx->k;
    state->weights = ctx->weights;
    state->center_mask = ctx->center_mask;
    state->nearest_slot = ctx->nearest_slot;
    state->nearest_dist = ctx->nearest_dist;
    if (state->center_mask == NULL
            || state->nearest_slot == NULL
            || state->nearest_dist == NULL) {
        return 0;
    }

    state->topk = topk;
    if (state->topk > 16u) {
        state->topk = 16u;
    }
    return 1;
}

static void
sixel_kcenter_swap_collect_global_candidates(
    sixel_kcenter_swap_candidate_state_t *state)
{
    unsigned int index;
    double lhs_weight;

    index = 0u;
    lhs_weight = 0.0;
    if (state == NULL) {
        return;
    }

    for (index = 0u; index < state->point_count; ++index) {
        if (state->center_mask[index] != 0u) {
            continue;
        }
        lhs_weight = (state->weights != NULL) ? state->weights[index] : 1.0;
        sixel_kcenter_swap_insert_candidate(state->global_list,
                                            state->global_dist,
                                            state->global_weight,
                                            &state->global_count,
                                            state->topk,
                                            index,
                                            state->nearest_dist[index],
                                            lhs_weight);
    }
}

static void
sixel_kcenter_swap_collect_cluster_slots(
    sixel_kcenter_swap_candidate_state_t *state)
{
    unsigned int index;
    unsigned int cluster_slot;

    index = 0u;
    cluster_slot = 0u;
    if (state == NULL) {
        return;
    }

    for (index = 0u; index < state->global_count; ++index) {
        cluster_slot = state->nearest_slot[state->global_list[index]];
        if (cluster_slot >= state->k) {
            continue;
        }
        if (sixel_kcenter_swap_candidate_present(state->cluster_slot_list,
                                                 state->cluster_slot_count,
                                                 cluster_slot)) {
            continue;
        }
        if (state->cluster_slot_count >= state->topk) {
            break;
        }
        state->cluster_slot_list[state->cluster_slot_count] = cluster_slot;
        ++state->cluster_slot_count;
    }
}

static void
sixel_kcenter_swap_collect_cluster_representatives(
    sixel_kcenter_swap_candidate_state_t *state)
{
    unsigned int index;
    unsigned int pick;
    unsigned int cluster_slot;
    unsigned int cluster_slot_pos;
    unsigned int cluster_best;
    double lhs_weight;
    double best_cluster_dist;
    double best_cluster_weight;

    index = 0u;
    pick = 0u;
    cluster_slot = 0u;
    cluster_slot_pos = 0u;
    cluster_best = UINT_MAX;
    lhs_weight = 0.0;
    best_cluster_dist = 0.0;
    best_cluster_weight = 0.0;
    if (state == NULL) {
        return;
    }

    for (index = 0u; index < state->cluster_slot_count; ++index) {
        state->cluster_best_list[index] = UINT_MAX;
        state->cluster_best_dist_list[index] = -1.0;
        state->cluster_best_weight_list[index] = -1.0;
    }
    for (pick = 0u; pick < state->point_count; ++pick) {
        if (state->center_mask[pick] != 0u) {
            continue;
        }
        cluster_slot = state->nearest_slot[pick];
        if (cluster_slot >= state->k) {
            continue;
        }
        cluster_slot_pos = 0u;
        while (cluster_slot_pos < state->cluster_slot_count
                && state->cluster_slot_list[cluster_slot_pos] != cluster_slot) {
            ++cluster_slot_pos;
        }
        if (cluster_slot_pos >= state->cluster_slot_count) {
            continue;
        }
        lhs_weight = (state->weights != NULL) ? state->weights[pick] : 1.0;
        if (lhs_weight <= 0.0) {
            lhs_weight = 1.0;
        }
        if (state->nearest_dist[pick]
                > state->cluster_best_dist_list[cluster_slot_pos] + 1.0e-12) {
            state->cluster_best_list[cluster_slot_pos] = pick;
            state->cluster_best_dist_list[cluster_slot_pos]
                = state->nearest_dist[pick];
            state->cluster_best_weight_list[cluster_slot_pos] = lhs_weight;
            continue;
        }
        if (state->nearest_dist[pick]
                >= state->cluster_best_dist_list[cluster_slot_pos] - 1.0e-12
                && (lhs_weight
                    > state->cluster_best_weight_list[cluster_slot_pos]
                        + 1.0e-12
                    || (lhs_weight
                        >= state->cluster_best_weight_list[cluster_slot_pos]
                            - 1.0e-12
                        && pick
                            < state->cluster_best_list[cluster_slot_pos]))) {
            state->cluster_best_list[cluster_slot_pos] = pick;
            state->cluster_best_dist_list[cluster_slot_pos]
                = state->nearest_dist[pick];
            state->cluster_best_weight_list[cluster_slot_pos] = lhs_weight;
        }
    }
    for (index = 0u; index < state->cluster_slot_count; ++index) {
        cluster_best = state->cluster_best_list[index];
        if (cluster_best == UINT_MAX) {
            continue;
        }
        best_cluster_dist = state->cluster_best_dist_list[index];
        best_cluster_weight = state->cluster_best_weight_list[index];
        sixel_kcenter_swap_insert_candidate(state->cluster_list,
                                            state->cluster_dist,
                                            state->cluster_weight,
                                            &state->cluster_count,
                                            state->topk,
                                            cluster_best,
                                            best_cluster_dist,
                                            best_cluster_weight);
    }
}

static void
sixel_kcenter_swap_merge_interleaved_candidates(
    sixel_kcenter_swap_candidate_state_t *state,
    unsigned int *candidate_list)
{
    unsigned int candidate;

    candidate = 0u;
    if (state == NULL || candidate_list == NULL) {
        return;
    }

    state->global_index = 0u;
    state->cluster_index = 0u;
    state->selected_source = 0u;
    while (state->candidate_count < state->topk
            && (state->global_index < state->global_count
                || state->cluster_index < state->cluster_count)) {
        if (state->selected_source == 0u
                && state->global_index < state->global_count) {
            candidate = state->global_list[state->global_index];
            ++state->global_index;
            state->selected_source = 1u;
        } else if (state->selected_source == 1u
                && state->cluster_index < state->cluster_count) {
            candidate = state->cluster_list[state->cluster_index];
            ++state->cluster_index;
            state->selected_source = 0u;
        } else if (state->global_index < state->global_count) {
            candidate = state->global_list[state->global_index];
            ++state->global_index;
        } else {
            candidate = state->cluster_list[state->cluster_index];
            ++state->cluster_index;
        }
        if (sixel_kcenter_swap_candidate_present(candidate_list,
                                                 state->candidate_count,
                                                 candidate)) {
            continue;
        }
        candidate_list[state->candidate_count] = candidate;
        ++state->candidate_count;
    }
    while (state->candidate_count < state->topk
            && state->global_index < state->global_count) {
        candidate = state->global_list[state->global_index];
        if (!sixel_kcenter_swap_candidate_present(candidate_list,
                                                  state->candidate_count,
                                                  candidate)) {
            candidate_list[state->candidate_count] = candidate;
            ++state->candidate_count;
        }
        ++state->global_index;
    }
    while (state->candidate_count < state->topk
            && state->cluster_index < state->cluster_count) {
        candidate = state->cluster_list[state->cluster_index];
        if (!sixel_kcenter_swap_candidate_present(candidate_list,
                                                  state->candidate_count,
                                                  candidate)) {
            candidate_list[state->candidate_count] = candidate;
            ++state->candidate_count;
        }
        ++state->cluster_index;
    }
}

static void
sixel_kcenter_swap_collect_global_only(
    sixel_kcenter_swap_candidate_state_t *state,
    unsigned int *candidate_list)
{
    unsigned int index;

    index = 0u;
    if (state == NULL || candidate_list == NULL) {
        return;
    }

    for (index = 0u; index < state->global_count; ++index) {
        candidate_list[state->candidate_count] = state->global_list[index];
        ++state->candidate_count;
    }
}

static void
sixel_kcenter_swap_fill_random_candidates(
    sixel_kcenter_swap_candidate_state_t *state,
    uint32_t *rng_state,
    unsigned int *candidate_list)
{
    unsigned int tries;
    unsigned int pick;
    int added;

    tries = 0u;
    pick = 0u;
    added = 0;
    if (state == NULL || candidate_list == NULL) {
        return;
    }

    /*
     * Keep random supplementation deterministic: preserve the existing
     * bounded-retry cap and RNG call order.
     */
    while (state->candidate_count < state->topk
            && rng_state != NULL
            && state->point_count > 0u) {
        tries = 0u;
        pick = 0u;
        added = 0;
        while (tries < state->point_count * 2u + 8u) {
            pick = sixel_kcenter_rng_bounded(rng_state, state->point_count);
            if (state->center_mask[pick] != 0u) {
                ++tries;
                continue;
            }
            if (!sixel_kcenter_swap_candidate_present(candidate_list,
                                                      state->candidate_count,
                                                      pick)) {
                candidate_list[state->candidate_count] = pick;
                ++state->candidate_count;
                added = 1;
                break;
            }
            ++tries;
        }
        if (!added) {
            break;
        }
    }
}

static unsigned int
sixel_kcenter_swap_collect_candidates(
    sixel_kcenter_swap_ctx_t const *ctx,
    unsigned int topk,
    int use_cluster_candidates,
    uint32_t *rng_state,
    unsigned int *candidate_list)
{
    sixel_kcenter_swap_candidate_state_t state;

    sixel_kcenter_swap_candidate_state_clear(&state);
    if (!sixel_kcenter_swap_collect_resolve_inputs(ctx,
                                                   topk,
                                                   candidate_list,
                                                   &state)) {
        return 0u;
    }

    sixel_kcenter_swap_collect_global_candidates(&state);

    /*
     * Keep candidate ordering stable:
     * global -> cluster representative extraction -> interleaved merge.
     */
    if (use_cluster_candidates) {
        sixel_kcenter_swap_collect_cluster_slots(&state);
        sixel_kcenter_swap_collect_cluster_representatives(&state);
        sixel_kcenter_swap_merge_interleaved_candidates(&state, candidate_list);
    } else {
        sixel_kcenter_swap_collect_global_only(&state, candidate_list);
    }

    sixel_kcenter_swap_fill_random_candidates(&state,
                                              rng_state,
                                              candidate_list);
    return state.candidate_count;
}

static int
sixel_kcenter_swap_step_init(sixel_kcenter_swap_step_t *step,
                             sixel_kcenter_swap_ctx_t const *ctx)
{
    if (step == NULL || ctx == NULL
            || ctx->radius2_io == NULL || ctx->sse_io == NULL) {
        return 0;
    }

    sixel_kcenter_swap_step_clear(step);
    step->topk = ctx->swap_topk;
    if (step->topk < 1u) {
        step->topk = 1u;
    }
    if (step->topk > 16u) {
        step->topk = 16u;
    }
    step->swap_min_gain = ctx->swap_min_gain;
    if (step->swap_min_gain < 0.0) {
        step->swap_min_gain = 0.0;
    }
    step->best_radius2 = *ctx->radius2_io;
    step->best_sse = *ctx->sse_io;
    step->allowed_radius2 = sixel_kcenter_swap_allowed_radius_sq(
        step->best_radius2,
        step->swap_min_gain);
    return 1;
}

static int
sixel_kcenter_swap_prepare_candidates_stage(sixel_kcenter_swap_ctx_t const *ctx,
                                            sixel_kcenter_swap_step_t *step)
{
    if (ctx == NULL || step == NULL) {
        return 0;
    }
    if (ctx->point_count == 0u
            || ctx->k == 0u
            || ctx->point_count <= ctx->k
            || ctx->center_mask == NULL
            || ctx->nearest_slot == NULL
            || ctx->nearest_dist == NULL) {
        return 0;
    }

    step->candidate_count = sixel_kcenter_swap_collect_candidates(
        ctx,
        step->topk,
        ctx->use_cluster_candidates,
        ctx->rng_state,
        step->candidate_list);
    return step->candidate_count > 0u;
}

static int
sixel_kcenter_swap_candidate_usable(sixel_kcenter_swap_ctx_t const *ctx,
                                    unsigned int candidate,
                                    unsigned int *slot_out)
{
    unsigned int slot;

    slot = 0u;
    if (ctx == NULL
            || ctx->centers == NULL
            || ctx->center_mask == NULL
            || ctx->nearest_slot == NULL
            || candidate >= ctx->point_count) {
        return 0;
    }

    slot = ctx->nearest_slot[candidate];
    if (slot >= ctx->k || ctx->centers[slot] == candidate) {
        return 0;
    }
    if (ctx->center_mask[candidate] != 0u) {
        return 0;
    }
    if (slot_out != NULL) {
        *slot_out = slot;
    }
    return 1;
}

static void
sixel_kcenter_swap_accept_candidate(sixel_kcenter_swap_step_t *step,
                                    unsigned int slot,
                                    unsigned int candidate,
                                    double radius2,
                                    double sse)
{
    if (step == NULL) {
        return;
    }
    step->best_radius2 = radius2;
    step->best_sse = sse;
    step->best_slot = slot;
    step->best_candidate = candidate;
    step->allowed_radius2 = sixel_kcenter_swap_allowed_radius_sq(
        step->best_radius2,
        step->swap_min_gain);
    step->found = 1;
}

typedef struct sixel_kcenter_swap_incremental_eval_state {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int k;
    unsigned int const *centers;
    unsigned int const *nearest_slot;
    double const *nearest_dist;
    unsigned int const *second_slot;
    double const *second_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    unsigned int candidate;
    unsigned int slot;
    unsigned int affected_count;
    double radius2;
    double sse;
    int rejected;
} sixel_kcenter_swap_incremental_eval_state_t;

static void
sixel_kcenter_swap_incremental_eval_state_clear(
    sixel_kcenter_swap_incremental_eval_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

static int
sixel_kcenter_swap_incremental_prepare(
    sixel_kcenter_swap_ctx_t const *ctx,
    sixel_kcenter_swap_step_t const *step,
    sixel_kcenter_swap_incremental_eval_state_t *state)
{
    if (ctx == NULL || step == NULL || state == NULL) {
        return 0;
    }
    if (ctx->swap_update != SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL
            || ctx->second_slot == NULL
            || ctx->second_dist == NULL
            || ctx->scratch_slot == NULL
            || ctx->scratch_dist == NULL
            || ctx->scratch_second_slot == NULL
            || ctx->scratch_second_dist == NULL) {
        return 0;
    }
    if (ctx->points == NULL
            || ctx->centers == NULL
            || ctx->nearest_slot == NULL
            || ctx->nearest_dist == NULL) {
        return 0;
    }

    state->points = ctx->points;
    state->weights = ctx->weights;
    state->point_count = ctx->point_count;
    state->k = ctx->k;
    state->centers = ctx->centers;
    state->nearest_slot = ctx->nearest_slot;
    state->nearest_dist = ctx->nearest_dist;
    state->second_slot = ctx->second_slot;
    state->second_dist = ctx->second_dist;
    state->scratch_slot = ctx->scratch_slot;
    state->scratch_dist = ctx->scratch_dist;
    state->scratch_second_slot = ctx->scratch_second_slot;
    state->scratch_second_dist = ctx->scratch_second_dist;
    return 1;
}

static double
sixel_kcenter_swap_incremental_weight(
    sixel_kcenter_swap_incremental_eval_state_t const *state,
    unsigned int point_index)
{
    double lhs_weight;

    lhs_weight = 1.0;
    if (state == NULL) {
        return lhs_weight;
    }
    lhs_weight = (state->weights != NULL) ? state->weights[point_index] : 1.0;
    if (lhs_weight <= 0.0) {
        lhs_weight = 1.0;
    }
    return lhs_weight;
}

static int
sixel_kcenter_swap_incremental_reject_by_radius(
    sixel_kcenter_swap_step_t const *step,
    double radius2)
{
    if (step == NULL) {
        return 1;
    }
    return radius2 > step->allowed_radius2 + 1.0e-12;
}

static int
sixel_kcenter_swap_incremental_reject_by_objective(
    sixel_kcenter_swap_step_t const *step,
    double radius2,
    double sse)
{
    if (step == NULL) {
        return 1;
    }
    return radius2 >= step->best_radius2 - 1.0e-12
        && sse >= step->best_sse - 1.0e-9;
}

static void
sixel_kcenter_swap_incremental_reset_candidate(
    sixel_kcenter_swap_incremental_eval_state_t *state,
    unsigned int candidate,
    unsigned int slot)
{
    if (state == NULL) {
        return;
    }
    state->candidate = candidate;
    state->slot = slot;
    state->affected_count = 0u;
    state->radius2 = 0.0;
    state->sse = 0.0;
    state->rejected = 0;
}

static void
sixel_kcenter_swap_incremental_eval_unaffected(
    sixel_kcenter_swap_incremental_eval_state_t *state,
    sixel_kcenter_swap_step_t const *step)
{
    unsigned int old_slot;
    unsigned int old_nearest_slot;
    unsigned int old_second_slot;
    double old_distance;
    double old_second;
    double distance_to_new;
    double new_distance;
    double lhs_weight;

    old_slot = 0u;
    old_nearest_slot = 0u;
    old_second_slot = 0u;
    old_distance = 0.0;
    old_second = 0.0;
    distance_to_new = 0.0;
    new_distance = 0.0;
    lhs_weight = 0.0;
    if (state == NULL || step == NULL) {
        return;
    }

    for (old_slot = 0u; old_slot < state->point_count; ++old_slot) {
        old_nearest_slot = state->nearest_slot[old_slot];
        if (old_nearest_slot == state->slot) {
            state->scratch_slot[old_slot] = UINT_MAX;
            ++state->affected_count;
            continue;
        }
        old_second_slot = state->second_slot[old_slot];
        old_distance = state->nearest_dist[old_slot];
        old_second = state->second_dist[old_slot];
        distance_to_new = sixel_kcenter_distance_sq(state->points,
                                                    old_slot,
                                                    state->candidate);
        if (distance_to_new < old_distance) {
            new_distance = distance_to_new;
            state->scratch_slot[old_slot] = state->slot;
            state->scratch_dist[old_slot] = distance_to_new;
            state->scratch_second_slot[old_slot] = old_nearest_slot;
            state->scratch_second_dist[old_slot] = old_distance;
        } else {
            new_distance = old_distance;
            state->scratch_slot[old_slot] = old_nearest_slot;
            state->scratch_dist[old_slot] = old_distance;
            if (distance_to_new < old_second) {
                state->scratch_second_slot[old_slot] = state->slot;
                state->scratch_second_dist[old_slot] = distance_to_new;
            } else {
                state->scratch_second_slot[old_slot] = old_second_slot;
                state->scratch_second_dist[old_slot] = old_second;
            }
        }
        if (new_distance > state->radius2) {
            state->radius2 = new_distance;
            if (sixel_kcenter_swap_incremental_reject_by_radius(
                    step, state->radius2)) {
                state->rejected = 1;
                break;
            }
        }
        lhs_weight = sixel_kcenter_swap_incremental_weight(state, old_slot);
        state->sse += new_distance * lhs_weight;
        if (sixel_kcenter_swap_incremental_reject_by_objective(step,
                                                               state->radius2,
                                                               state->sse)) {
            state->rejected = 1;
            break;
        }
    }
}

static void
sixel_kcenter_swap_incremental_eval_affected(
    sixel_kcenter_swap_incremental_eval_state_t *state,
    sixel_kcenter_swap_step_t const *step)
{
    unsigned int old_slot;
    unsigned int scan_slot;
    unsigned int scan_best_slot;
    unsigned int scan_second_slot;
    double scan_best_dist;
    double scan_second_dist;
    double scan_distance;
    double lhs_weight;

    old_slot = 0u;
    scan_slot = 0u;
    scan_best_slot = 0u;
    scan_second_slot = 0u;
    scan_best_dist = 0.0;
    scan_second_dist = 0.0;
    scan_distance = 0.0;
    lhs_weight = 0.0;
    if (state == NULL || step == NULL || state->affected_count == 0u) {
        return;
    }

    for (old_slot = 0u; old_slot < state->point_count; ++old_slot) {
        if (state->scratch_slot[old_slot] != UINT_MAX) {
            continue;
        }
        scan_best_slot = 0u;
        if (state->slot == 0u) {
            scan_best_dist = sixel_kcenter_distance_sq(state->points,
                                                       old_slot,
                                                       state->candidate);
        } else {
            scan_best_dist = sixel_kcenter_distance_sq(state->points,
                                                       old_slot,
                                                       state->centers[0u]);
        }
        scan_second_slot = 0u;
        scan_second_dist = DBL_MAX;
        for (scan_slot = 1u; scan_slot < state->k; ++scan_slot) {
            if (scan_slot == state->slot) {
                scan_distance = sixel_kcenter_distance_sq(state->points,
                                                          old_slot,
                                                          state->candidate);
            } else {
                scan_distance = sixel_kcenter_distance_sq(state->points,
                                                          old_slot,
                                                          state->centers[
                                                              scan_slot]);
            }
            if (scan_distance < scan_best_dist) {
                scan_second_dist = scan_best_dist;
                scan_second_slot = scan_best_slot;
                scan_best_dist = scan_distance;
                scan_best_slot = scan_slot;
            } else if (scan_distance < scan_second_dist) {
                scan_second_dist = scan_distance;
                scan_second_slot = scan_slot;
            }
        }
        if (state->k == 1u) {
            scan_second_slot = scan_best_slot;
            scan_second_dist = scan_best_dist;
        }
        state->scratch_slot[old_slot] = scan_best_slot;
        state->scratch_dist[old_slot] = scan_best_dist;
        state->scratch_second_slot[old_slot] = scan_second_slot;
        state->scratch_second_dist[old_slot] = scan_second_dist;
        if (scan_best_dist > state->radius2) {
            state->radius2 = scan_best_dist;
            if (sixel_kcenter_swap_incremental_reject_by_radius(
                    step, state->radius2)) {
                state->rejected = 1;
                break;
            }
        }
        lhs_weight = sixel_kcenter_swap_incremental_weight(state, old_slot);
        state->sse += scan_best_dist * lhs_weight;
        if (sixel_kcenter_swap_incremental_reject_by_objective(step,
                                                               state->radius2,
                                                               state->sse)) {
            state->rejected = 1;
            break;
        }
    }
}

static void
sixel_kcenter_swap_eval_incremental_stage(sixel_kcenter_swap_ctx_t const *ctx,
                                          sixel_kcenter_swap_step_t *step)
{
    sixel_kcenter_swap_incremental_eval_state_t state;
    unsigned int index;
    unsigned int candidate;
    unsigned int slot;

    sixel_kcenter_swap_incremental_eval_state_clear(&state);
    index = 0u;
    candidate = 0u;
    slot = 0u;
    if (!sixel_kcenter_swap_incremental_prepare(ctx, step, &state)) {
        return;
    }

    /*
     * Keep objective ordering strict and unchanged:
     * radius minimization first, weighted SSE as the tie-break.
     */
    step->used_incremental_eval = 1;
    for (index = 0u; index < step->candidate_count; ++index) {
        candidate = step->candidate_list[index];
        if (!sixel_kcenter_swap_candidate_usable(ctx, candidate, &slot)) {
            continue;
        }
        sixel_kcenter_swap_incremental_reset_candidate(&state, candidate, slot);
        sixel_kcenter_swap_incremental_eval_unaffected(&state, step);
        if (state.rejected) {
            continue;
        }
        sixel_kcenter_swap_incremental_eval_affected(&state, step);
        if (state.rejected) {
            continue;
        }

        if (sixel_kcenter_swap_candidate_is_better(state.radius2,
                                                   state.sse,
                                                   step->best_radius2,
                                                   step->best_sse,
                                                   step->allowed_radius2)) {
            sixel_kcenter_swap_accept_candidate(step,
                                                slot,
                                                candidate,
                                                state.radius2,
                                                state.sse);
        }
    }
}

static void
sixel_kcenter_swap_eval_full_stage(sixel_kcenter_swap_ctx_t *ctx,
                                   sixel_kcenter_swap_step_t *step)
{
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int k;
    unsigned int *centers;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    double *scratch_dist;
    unsigned int index;
    unsigned int candidate;
    unsigned int slot;
    unsigned int old_center;
    double radius2;
    double sse;
    double *trial_new_dist;
    double *best_new_dist;

    points = NULL;
    weights = NULL;
    point_count = 0u;
    k = 0u;
    centers = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    scratch_dist = NULL;
    index = 0u;
    candidate = 0u;
    slot = 0u;
    old_center = 0u;
    radius2 = 0.0;
    sse = 0.0;
    trial_new_dist = NULL;
    best_new_dist = NULL;

    if (ctx == NULL || step == NULL || step->used_incremental_eval) {
        return;
    }

    points = ctx->points;
    weights = ctx->weights;
    point_count = ctx->point_count;
    k = ctx->k;
    centers = ctx->centers;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    second_dist = ctx->second_dist;
    scratch_dist = ctx->scratch_dist;
    trial_new_dist = ctx->scratch_second_dist;
    if (points == NULL || centers == NULL || nearest_slot == NULL
            || nearest_dist == NULL) {
        return;
    }

    for (index = 0u; index < step->candidate_count; ++index) {
        candidate = step->candidate_list[index];
        if (!sixel_kcenter_swap_candidate_usable(ctx, candidate, &slot)) {
            continue;
        }

        if (second_dist != NULL) {
            if (!sixel_kcenter_swap_eval_with_cutoff(points,
                                                     weights,
                                                     point_count,
                                                     slot,
                                                     candidate,
                                                     nearest_slot,
                                                     nearest_dist,
                                                     second_dist,
                                                     step->allowed_radius2,
                                                     step->best_radius2,
                                                     step->best_sse,
                                                     trial_new_dist,
                                                     &radius2,
                                                     &sse)) {
                continue;
            }
        } else {
            old_center = centers[slot];
            centers[slot] = candidate;
            if (!sixel_kcenter_assign_points_with_cutoff(points,
                                                         weights,
                                                         point_count,
                                                         centers,
                                                         k,
                                                         step->allowed_radius2,
                                                         &radius2,
                                                         &sse)) {
                centers[slot] = old_center;
                continue;
            }
            centers[slot] = old_center;
        }

        if (sixel_kcenter_swap_candidate_is_better(radius2,
                                                   sse,
                                                   step->best_radius2,
                                                   step->best_sse,
                                                   step->allowed_radius2)) {
            sixel_kcenter_swap_accept_candidate(step,
                                                slot,
                                                candidate,
                                                radius2,
                                                sse);
            if (trial_new_dist != NULL
                    && scratch_dist != NULL
                    && point_count > 0u) {
                memcpy(scratch_dist,
                       trial_new_dist,
                       (size_t)point_count * sizeof(double));
                best_new_dist = scratch_dist;
            } else {
                best_new_dist = NULL;
            }
        }
    }

    step->best_new_dist = best_new_dist;
}

static int
sixel_kcenter_swap_apply_stage(sixel_kcenter_swap_ctx_t *ctx,
                               sixel_kcenter_swap_step_t const *step)
{
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int k;
    unsigned int *centers;
    unsigned char *center_mask;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *second_slot;
    double *second_dist;
    double *radius2_io;
    double *sse_io;
    unsigned int *scratch_second_slot;
    double *scratch_second_dist;
    unsigned int old_center;
    double radius2;
    double sse;
    int used_cached_apply;
    sixel_kcenter_swap_apply_ctx_t apply_ctx;

    points = NULL;
    weights = NULL;
    point_count = 0u;
    k = 0u;
    centers = NULL;
    center_mask = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_slot = NULL;
    second_dist = NULL;
    radius2_io = NULL;
    sse_io = NULL;
    scratch_second_slot = NULL;
    scratch_second_dist = NULL;
    old_center = 0u;
    radius2 = 0.0;
    sse = 0.0;
    used_cached_apply = 0;
    sixel_kcenter_swap_apply_ctx_clear(&apply_ctx);

    if (ctx == NULL || step == NULL || !step->found) {
        return 0;
    }

    points = ctx->points;
    weights = ctx->weights;
    point_count = ctx->point_count;
    k = ctx->k;
    centers = ctx->centers;
    center_mask = ctx->center_mask;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    second_slot = ctx->second_slot;
    second_dist = ctx->second_dist;
    radius2_io = ctx->radius2_io;
    sse_io = ctx->sse_io;
    scratch_second_slot = ctx->scratch_second_slot;
    scratch_second_dist = ctx->scratch_second_dist;
    if (points == NULL || centers == NULL
            || center_mask == NULL
            || radius2_io == NULL
            || sse_io == NULL) {
        return 0;
    }

    old_center = centers[step->best_slot];
    centers[step->best_slot] = step->best_candidate;
    center_mask[old_center] = 0u;
    center_mask[step->best_candidate] = 1u;
    sixel_kcenter_swap_prepare_apply_ctx(&apply_ctx,
                                         ctx,
                                         step->best_slot,
                                         step->best_candidate,
                                         step->best_new_dist,
                                         &radius2,
                                         &sse);
    used_cached_apply = sixel_kcenter_swap_apply_with_second(&apply_ctx);
    if (!used_cached_apply) {
        /*
         * Fallback keeps nearest/second assignment coherent by rebuilding
         * from the same center set with the shared dispatcher.
         */
        sixel_kcenter_assign_points_dispatch(points,
                                             weights,
                                             point_count,
                                             centers,
                                             k,
                                             nearest_slot,
                                             nearest_dist,
                                             second_slot,
                                             second_dist,
                                             &radius2,
                                             &sse,
                                             NULL,
                                             NULL);
    }
    *radius2_io = radius2;
    *sse_io = sse;
    if (!used_cached_apply
            && scratch_second_slot != NULL
            && scratch_second_dist != NULL
            && nearest_slot != NULL
            && nearest_dist != NULL
            && point_count > 0u) {
        /*
         * Keep fallback buffers initialized for debugability and to ensure the
         * next incremental pass starts from coherent nearest/second state.
         */
        memcpy(scratch_second_slot,
               nearest_slot,
               (size_t)point_count * sizeof(unsigned int));
        memcpy(scratch_second_dist,
               nearest_dist,
               (size_t)point_count * sizeof(double));
    }
    return 1;
}

/*
 * Try one swap step and keep the objective ordering strict:
 *   1) minimize radius first
 *   2) break ties by weighted SSE
 */
static int
sixel_kcenter_try_worst_swap(sixel_kcenter_swap_ctx_t *ctx)
{
    sixel_kcenter_swap_step_t step;

    sixel_kcenter_swap_step_clear(&step);
    if (!sixel_kcenter_swap_step_init(&step, ctx)) {
        return 0;
    }
    if (!sixel_kcenter_swap_prepare_candidates_stage(ctx, &step)) {
        return 0;
    }

    sixel_kcenter_swap_eval_incremental_stage(ctx, &step);
    if (!step.used_incremental_eval) {
        sixel_kcenter_swap_eval_full_stage(ctx, &step);
    }
    return sixel_kcenter_swap_apply_stage(ctx, &step);
}

static void
sixel_kcenter_solver_seed_centers(sixel_kcenter_algo_t resolved_algo,
                                  double const *points,
                                  double const *weights,
                                  unsigned int point_count,
                                  unsigned int k,
                                  uint32_t *trial_state,
                                  unsigned int *centers,
                                  unsigned int *scratch_slot,
                                  double *fft_dist_cache,
                                  unsigned char *center_mask)
{
    if (resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_SWAP) {
        sixel_kcenter_choose_random(point_count,
                                    k,
                                    trial_state,
                                    centers,
                                    scratch_slot);
    } else {
        sixel_kcenter_choose_fft(points,
                                 weights,
                                 point_count,
                                 k,
                                 trial_state,
                                 centers,
                                 fft_dist_cache,
                                 center_mask);
    }
    sixel_kcenter_refresh_center_mask(center_mask,
                                      point_count,
                                      centers,
                                      k);
}

static void
sixel_kcenter_solver_assign_current(double const *points,
                                    double const *weights,
                                    unsigned int point_count,
                                    unsigned int const *centers,
                                    unsigned int k,
                                    unsigned int *nearest_slot,
                                    double *nearest_dist,
                                    unsigned int *second_slot,
                                    double *second_dist,
                                    double *radius2_out,
                                    double *sse_out)
{
    sixel_kcenter_assign_points_dispatch(points,
                                         weights,
                                         point_count,
                                         centers,
                                         k,
                                         nearest_slot,
                                         nearest_dist,
                                         second_slot,
                                         second_dist,
                                         radius2_out,
                                         sse_out,
                                         NULL,
                                         NULL);
}

static void
sixel_kcenter_solver_run_trial_swaps(sixel_kcenter_swap_ctx_t *swap_ctx,
                                     sixel_kcenter_algo_t resolved_algo,
                                     sixel_kcenter_profile_t profile,
                                     unsigned int iter_limit,
                                     unsigned int swap_topk,
                                     unsigned int swap_patience,
                                     int adaptive_swap_controls,
                                     double tiny_gain2,
                                     double *radius2_io,
                                     unsigned int *trial_iterations_io)
{
    unsigned int trial_iterations;
    unsigned int no_improve;
    unsigned int effective_topk;
    unsigned int patience_limit;
    double gain2;

    trial_iterations = 0u;
    no_improve = 0u;
    effective_topk = swap_topk;
    patience_limit = swap_patience;
    gain2 = 0.0;

    if (trial_iterations_io != NULL) {
        *trial_iterations_io = 0u;
    }
    if (swap_ctx == NULL || radius2_io == NULL) {
        return;
    }
    if (profile == SIXEL_PALETTE_KCENTER_PROFILE_SPEED
            && effective_topk > 2u) {
        effective_topk = 2u;
    }
    if (adaptive_swap_controls && patience_limit == 0u) {
        patience_limit = sixel_kcenter_profile_default_swap_patience(profile);
    }
    if (resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_FFT) {
        return;
    }

    while (trial_iterations < iter_limit) {
        swap_ctx->swap_topk = effective_topk;
        gain2 = *radius2_io;
        if (sixel_kcenter_try_worst_swap(swap_ctx)) {
            gain2 -= *radius2_io;
            ++trial_iterations;
            if (adaptive_swap_controls && gain2 <= tiny_gain2 + 1.0e-12) {
                ++no_improve;
            } else {
                no_improve = 0u;
            }
            if (adaptive_swap_controls
                    && gain2 > tiny_gain2 * 4.0 + 1.0e-12) {
                effective_topk = swap_topk;
                if (profile == SIXEL_PALETTE_KCENTER_PROFILE_SPEED
                        && effective_topk > 2u) {
                    effective_topk = 2u;
                }
            } else if (adaptive_swap_controls && effective_topk > 1u) {
                --effective_topk;
            }
            if (patience_limit != 0u
                    && no_improve >= patience_limit
                    && trial_iterations >= 2u) {
                break;
            }
            continue;
        }
        ++no_improve;
        if (adaptive_swap_controls
                && no_improve >= 2u
                && effective_topk > 1u) {
            --effective_topk;
        }
        if (patience_limit == 0u || no_improve >= patience_limit) {
            break;
        }
    }

    if (trial_iterations_io != NULL) {
        *trial_iterations_io = trial_iterations;
    }
}

static int
sixel_kcenter_solver_trial_is_better(double radius2,
                                     double sse,
                                     double best_radius2,
                                     double best_sse)
{
    if (best_radius2 < 0.0) {
        return 1;
    }
    if (radius2 < best_radius2 - 1.0e-12) {
        return 1;
    }
    if (radius2 <= best_radius2 + 1.0e-12
            && sse < best_sse - 1.0e-9) {
        return 1;
    }
    return 0;
}

static int
sixel_kcenter_solver_validate_ctx(sixel_kcenter_solver_ctx_t const *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    if (ctx->points == NULL || ctx->centers == NULL
            || ctx->nearest_slot == NULL || ctx->nearest_dist == NULL
            || ctx->scratch_slot == NULL || ctx->scratch_dist == NULL
            || ctx->scratch_indices == NULL
            || ctx->fft_dist_cache == NULL || ctx->center_mask == NULL) {
        return 0;
    }
    return 1;
}

static void
sixel_kcenter_solver_init_runtime(
    sixel_kcenter_solver_ctx_t const *ctx,
    sixel_kcenter_solver_run_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    state->status = SIXEL_OK;
    state->init_seeds = ctx->init_seeds;
    state->swap_topk = ctx->swap_topk;
    state->swap_patience = ctx->swap_patience;
    state->swap_min_gain = ctx->swap_min_gain;
    state->best_trial_radius2 = -1.0;
    state->best_trial_sse = 0.0;
    state->best_trial_iterations = 0u;
    state->base_state = 1u;
    state->trial_state = 1u;

    if (state->init_seeds < 1u) {
        state->init_seeds = 1u;
    }
    if (state->init_seeds > 8u) {
        state->init_seeds = 8u;
    }
    if (state->swap_topk < 1u) {
        state->swap_topk = 1u;
    }
    if (state->swap_topk > 16u) {
        state->swap_topk = 16u;
    }
    if (state->swap_patience > 8u) {
        state->swap_patience = 8u;
    }
    if (state->swap_min_gain < 0.0) {
        state->swap_min_gain = 0.0;
    }
    if (state->swap_min_gain > 0.0) {
        state->tiny_gain2 = state->swap_min_gain * state->swap_min_gain * 0.25;
    } else {
        state->tiny_gain2 = 1.0e-6;
    }
    state->adaptive_swap_controls
        = (ctx->profile != SIXEL_PALETTE_KCENTER_PROFILE_LEGACY);
    if (ctx->rng_state != NULL && *ctx->rng_state != 0u) {
        state->base_state = *ctx->rng_state;
    }
}

static void
sixel_kcenter_solver_init_swap_context(
    sixel_kcenter_solver_ctx_t const *ctx,
    sixel_kcenter_solver_run_state_t *state)
{
    sixel_kcenter_swap_ctx_t *swap_ctx;

    swap_ctx = NULL;
    if (ctx == NULL || state == NULL) {
        return;
    }

    swap_ctx = &state->swap_ctx;
    memset(swap_ctx, 0, sizeof(*swap_ctx));
    swap_ctx->points = ctx->points;
    swap_ctx->weights = ctx->weights;
    swap_ctx->point_count = ctx->point_count;
    swap_ctx->centers = ctx->centers;
    swap_ctx->k = ctx->k;
    swap_ctx->center_mask = ctx->center_mask;
    swap_ctx->nearest_slot = ctx->nearest_slot;
    swap_ctx->nearest_dist = ctx->nearest_dist;
    swap_ctx->second_slot = ctx->second_slot;
    swap_ctx->second_dist = ctx->second_dist;
    swap_ctx->radius2_io = &state->radius2;
    swap_ctx->sse_io = &state->sse;
    swap_ctx->scratch_slot = ctx->scratch_slot;
    swap_ctx->scratch_dist = ctx->scratch_dist;
    swap_ctx->scratch_second_slot = ctx->scratch_second_slot;
    swap_ctx->scratch_second_dist = ctx->scratch_second_dist;
    swap_ctx->swap_topk = state->swap_topk;
    swap_ctx->swap_update = ctx->swap_update;
    swap_ctx->swap_min_gain = state->swap_min_gain;
    swap_ctx->use_cluster_candidates = ctx->use_cluster_candidates;
    swap_ctx->rng_state = &state->trial_state;
}

static void
sixel_kcenter_solver_run_trial_once(
    sixel_kcenter_solver_ctx_t const *ctx,
    sixel_kcenter_solver_run_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    state->trial_state = (uint32_t)(
        (uint64_t)state->base_state
        + (uint64_t)0x9e3779b9u * ((uint64_t)state->init_trial + 1u));
    if (state->trial_state == 0u) {
        state->trial_state = 1u;
    }

    sixel_kcenter_solver_seed_centers(ctx->resolved_algo,
                                      ctx->points,
                                      ctx->weights,
                                      ctx->point_count,
                                      ctx->k,
                                      &state->trial_state,
                                      ctx->centers,
                                      ctx->scratch_slot,
                                      ctx->fft_dist_cache,
                                      ctx->center_mask);

    sixel_kcenter_solver_assign_current(ctx->points,
                                        ctx->weights,
                                        ctx->point_count,
                                        ctx->centers,
                                        ctx->k,
                                        ctx->nearest_slot,
                                        ctx->nearest_dist,
                                        ctx->second_slot,
                                        ctx->second_dist,
                                        &state->radius2,
                                        &state->sse);

    sixel_kcenter_solver_run_trial_swaps(&state->swap_ctx,
                                         ctx->resolved_algo,
                                         ctx->profile,
                                         ctx->iter_limit,
                                         state->swap_topk,
                                         state->swap_patience,
                                         state->adaptive_swap_controls,
                                         state->tiny_gain2,
                                         &state->radius2,
                                         &state->trial_iterations);
}

static void
sixel_kcenter_solver_maybe_store_best(
    sixel_kcenter_solver_ctx_t const *ctx,
    sixel_kcenter_solver_run_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }
    if (!sixel_kcenter_solver_trial_is_better(state->radius2,
                                              state->sse,
                                              state->best_trial_radius2,
                                              state->best_trial_sse)) {
        return;
    }

    memcpy(ctx->scratch_indices,
           ctx->centers,
           (size_t)ctx->k * sizeof(unsigned int));
    state->best_trial_radius2 = state->radius2;
    state->best_trial_sse = state->sse;
    state->best_trial_iterations = state->trial_iterations;
}

static void
sixel_kcenter_solver_finalize_best(
    sixel_kcenter_solver_ctx_t const *ctx,
    sixel_kcenter_solver_run_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    memcpy(ctx->centers,
           ctx->scratch_indices,
           (size_t)ctx->k * sizeof(unsigned int));
    sixel_kcenter_refresh_center_mask(ctx->center_mask,
                                      ctx->point_count,
                                      ctx->centers,
                                      ctx->k);
    sixel_kcenter_solver_assign_current(ctx->points,
                                        ctx->weights,
                                        ctx->point_count,
                                        ctx->centers,
                                        ctx->k,
                                        ctx->nearest_slot,
                                        ctx->nearest_dist,
                                        ctx->second_slot,
                                        ctx->second_dist,
                                        &state->radius2,
                                        &state->sse);
    state->iterations = state->best_trial_iterations;

    if (ctx->radius2_out != NULL) {
        *ctx->radius2_out = state->radius2;
    }
    if (ctx->sse_out != NULL) {
        *ctx->sse_out = state->sse;
    }
    if (ctx->iterations_out != NULL) {
        *ctx->iterations_out = state->iterations;
    }
    if (ctx->rng_state != NULL) {
        *ctx->rng_state = state->trial_state;
    }
}

static SIXELSTATUS
sixel_kcenter_run_solver(sixel_kcenter_solver_ctx_t *ctx)
{
    sixel_kcenter_solver_run_state_t state;

    if (!sixel_kcenter_solver_validate_ctx(ctx)) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (ctx->point_count == 0u || ctx->k == 0u) {
        if (ctx->radius2_out != NULL) {
            *ctx->radius2_out = 0.0;
        }
        if (ctx->sse_out != NULL) {
            *ctx->sse_out = 0.0;
        }
        if (ctx->iterations_out != NULL) {
            *ctx->iterations_out = 0u;
        }
        return SIXEL_OK;
    }

    sixel_kcenter_solver_run_state_clear(&state);
    sixel_kcenter_solver_init_runtime(ctx, &state);
    sixel_kcenter_solver_init_swap_context(ctx, &state);

    /*
     * Trial invariants:
     *   - seed progression keeps the fixed golden-ratio offset order
     *   - objective ordering stays radius first, then weighted SSE
     *   - final assignment always runs once after restoring best centers
     */
    for (state.init_trial = 0u;
            state.init_trial < state.init_seeds;
            ++state.init_trial) {
        sixel_kcenter_solver_run_trial_once(ctx, &state);
        sixel_kcenter_solver_maybe_store_best(ctx, &state);
    }

    sixel_kcenter_solver_finalize_best(ctx, &state);
    return state.status;
}

static sixel_kcenter_algo_t
sixel_kcenter_choose_auto_algo(int quality_mode,
                               unsigned int point_count,
                               sixel_kcenter_auto_policy_t auto_policy,
                               unsigned int auto_fft_threshold,
                               int pixelformat,
                               sixel_kcenter_space_policy_t space_policy)
{
    unsigned int threshold;
    int use_perceptual_bias;

    threshold = auto_fft_threshold;
    use_perceptual_bias = 0;
    if (threshold < 256u) {
        threshold = 256u;
    }
    if (threshold > 65536u) {
        threshold = 65536u;
    }

    if (sixel_kcenter_resolve_auto_policy(auto_policy)
            == SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY) {
        if (quality_mode == SIXEL_QUALITY_LOW) {
            return SIXEL_PALETTE_KCENTER_ALGO_FFT;
        }
        /*
         * Keep legacy default stable but avoid an overly early FFT switch in
         * OKLab because hybrid usually preserves perceptual detail better.
         */
        if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32
                && point_count > 4096u) {
            return SIXEL_PALETTE_KCENTER_ALGO_FFT;
        }
        if (point_count > 2048u) {
            return SIXEL_PALETTE_KCENTER_ALGO_FFT;
        }
        return SIXEL_PALETTE_KCENTER_ALGO_HYBRID;
    }

    if (sixel_kcenter_resolve_space_policy(space_policy)
            == SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL
            && (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32
                || pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32
                || pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32)) {
        use_perceptual_bias = 1;
    }

    if (quality_mode == SIXEL_QUALITY_LOW) {
        if (threshold > 256u) {
            threshold /= 2u;
        }
    } else if (quality_mode == SIXEL_QUALITY_FULL) {
        if (threshold < 32768u) {
            threshold *= 2u;
        }
    }
    if (use_perceptual_bias) {
        if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
            if (quality_mode == SIXEL_QUALITY_LOW) {
                if (threshold < 32768u) {
                    threshold *= 2u;
                } else {
                    threshold = 65536u;
                }
            } else if (threshold < 16384u) {
                threshold *= 4u;
            } else {
                threshold = 65536u;
            }
        } else if (threshold < 32768u) {
            threshold *= 2u;
        } else {
            threshold = 65536u;
        }
    }
    if (point_count > threshold) {
        return SIXEL_PALETTE_KCENTER_ALGO_FFT;
    }
    return SIXEL_PALETTE_KCENTER_ALGO_HYBRID;
}

/*
 * Keep explicit user knobs stable while nudging default profile settings
 * toward better perceptual-space quality.  The bump is applied only when the
 * current values still match the profile defaults.
 */
static void
sixel_kcenter_apply_perceptual_solver_bias(
    sixel_kcenter_profile_t profile,
    sixel_kcenter_space_policy_t space_policy,
    unsigned int *init_seeds_io,
    unsigned int *swap_topk_io)
{
    unsigned int profile_init_seeds;
    unsigned int profile_swap_topk;

    profile_init_seeds = 0u;
    profile_swap_topk = 0u;
    if (init_seeds_io == NULL || swap_topk_io == NULL) {
        return;
    }
    if (profile == SIXEL_PALETTE_KCENTER_PROFILE_LEGACY) {
        return;
    }
    if (sixel_kcenter_resolve_space_policy(space_policy)
            != SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL) {
        return;
    }

    profile_init_seeds = sixel_kcenter_profile_default_init_seeds(profile);
    profile_swap_topk = sixel_kcenter_profile_default_swap_topk(profile);
    if (*init_seeds_io == profile_init_seeds && *init_seeds_io < 8u) {
        *init_seeds_io += 1u;
    }
    if (*swap_topk_io == profile_swap_topk) {
        if (profile == SIXEL_PALETTE_KCENTER_PROFILE_QUALITY
                && *swap_topk_io <= 14u) {
            *swap_topk_io += 2u;
        } else if (*swap_topk_io < 16u) {
            *swap_topk_io += 1u;
        }
    }
}


typedef struct sixel_kcenter_build_runtime {
    unsigned char **result;
    float **result_float32;
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    unsigned int reqcolors;
    unsigned int *ncolors;
    unsigned int *origcolors;
    int quality_mode;
    int force_palette;
    int use_reversible;
    int final_merge_mode;
    sixel_allocator_t *allocator;
    int pixelformat;
    int treat_input_as_float32;
    sixel_timeline_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    sixel_palette_telemetry_t *telemetry;
    SIXELSTATUS status;
    double *points;
    double *weights;
    unsigned int point_count;
    unsigned int visible_count;
    unsigned int active_count;
    unsigned int k;
    unsigned int overshoot;
    unsigned int polish_point_limit;
    unsigned int polish_k_limit;
    unsigned int seed;
    unsigned int restarts;
    unsigned int init_seeds;
    unsigned int iter_limit;
    unsigned int histbits;
    unsigned int point_budget;
    unsigned int auto_fft_threshold;
    unsigned int rare_keep;
    unsigned int swap_topk;
    unsigned int swap_patience;
    double swap_min_gain;
    int use_cluster_candidates;
    int use_perceptual_polish;
    int use_legacy_oklab_polish;
    unsigned int total_iterations;
    unsigned int best_iterations;
    unsigned int run_iterations;
    unsigned int polish_updates;
    unsigned int resolved_merge;
    int apply_merge;
    sixel_kcenter_algo_t algo;
    sixel_kcenter_algo_t resolved_algo;
    sixel_kcenter_profile_t profile;
    sixel_kcenter_auto_policy_t auto_policy;
    sixel_kcenter_space_policy_t space_policy;
    sixel_kcenter_candidate_policy_t candidate_policy;
    sixel_kcenter_budget_policy_t budget_policy;
    sixel_kcenter_swap_update_t swap_update;
    uint32_t rng_state;
    unsigned int *centers;
    unsigned int *best_centers;
    unsigned int *work_centers;
    unsigned int *nearest_slot;
    unsigned int *second_slot;
    unsigned int *scratch_slot;
    unsigned int *scratch_second_slot;
    double *nearest_dist;
    double *second_dist;
    double *scratch_dist;
    double *scratch_second_dist;
    double *cluster_weights;
    double *cluster_sums;
    double *final_centers;
    double *sort_weights;
    unsigned int *order;
    unsigned int *scratch_indices;
    double *fft_dist_cache;
    unsigned char *center_mask;
    double radius2;
    double sse;
    double best_radius2;
    double best_sse;
    double polish_pre_radius2;
    double prune_mass;
    double budget_scale;
    unsigned long *merge_weights;
    double *merge_sums;
    int cluster_total;
    unsigned int final_count;
    unsigned char *palette;
    unsigned char *grown_palette;
    float *float_palette;
    float *grown_float;
    int input_is_float32;
    double float32_channel_scale[3];
    double float32_channel_offset[3];
    int job_init;
    int job_iter;
    int job_merge;
    int job_export;
    char log_detail[320];
    double wall_start;
    double init_stop;
    double iterate_start;
    double iterate_stop;
    double merge_start;
    double merge_stop;
    double export_start;
    double export_stop;
    int override_lock_acquired;
    sixel_kcenter_solver_ctx_t solver_ctx;
    sixel_kcenter_polish_ctx_t polish_ctx;
} sixel_kcenter_build_runtime_t;

static void
sixel_kcenter_build_runtime_init(sixel_kcenter_build_runtime_t *rt,
                                 sixel_kcenter_build_ctx_t const *ctx)
{
    if (rt == NULL || ctx == NULL) {
        return;
    }
    memset(rt, 0, sizeof(*rt));

    rt->result = ctx->result;
    rt->result_float32 = ctx->result_float32;
    rt->data = ctx->data;
    rt->length = ctx->length;
    rt->depth = ctx->depth;
    rt->reqcolors = ctx->reqcolors;
    rt->ncolors = ctx->ncolors;
    rt->origcolors = ctx->origcolors;
    rt->quality_mode = ctx->quality_mode;
    rt->force_palette = ctx->force_palette;
    rt->use_reversible = ctx->use_reversible;
    rt->final_merge_mode = ctx->final_merge_mode;
    rt->allocator = ctx->allocator;
    rt->pixelformat = ctx->pixelformat;
    rt->treat_input_as_float32 = ctx->treat_input_as_float32;
    rt->logger = ctx->logger;
    rt->job_seq = ctx->job_seq;
    rt->engine_name = ctx->engine_name;
    rt->telemetry = ctx->telemetry;

    rt->status = SIXEL_BAD_ARGUMENT;
    rt->seed = 1u;
    rt->restarts = 1u;
    rt->init_seeds = SIXEL_KCENTER_INIT_SEEDS_DEFAULT;
    rt->iter_limit = 16u;
    rt->histbits = 5u;
    rt->point_budget = 0u;
    rt->auto_fft_threshold = SIXEL_KCENTER_AUTO_FFT_THRESHOLD_DEFAULT;
    rt->rare_keep = SIXEL_KCENTER_RARE_KEEP_DEFAULT;
    rt->swap_topk = SIXEL_KCENTER_SWAP_TOPK_DEFAULT;
    rt->swap_patience = SIXEL_KCENTER_SWAP_PATIENCE_DEFAULT;
    rt->swap_min_gain = SIXEL_KCENTER_SWAP_MIN_GAIN_DEFAULT;
    rt->resolved_merge = SIXEL_FINAL_MERGE_NONE;
    rt->algo = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    rt->resolved_algo = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    rt->profile = SIXEL_PALETTE_KCENTER_PROFILE_LEGACY;
    rt->auto_policy = SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
    rt->space_policy = SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
    rt->candidate_policy = SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
    rt->budget_policy = SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
    rt->swap_update = SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
    rt->rng_state = 1u;
    rt->prune_mass = 0.995;
    rt->budget_scale = SIXEL_KCENTER_BUDGET_SCALE_DEFAULT;
    rt->job_init = -1;
    rt->job_iter = -1;
    rt->job_merge = -1;
    rt->job_export = -1;
    rt->log_detail[0] = '\0';

    if (rt->result != NULL) {
        *rt->result = NULL;
    }
    if (rt->result_float32 != NULL) {
        *rt->result_float32 = NULL;
    }
    if (rt->ncolors != NULL) {
        *rt->ncolors = 0u;
    }
    if (rt->origcolors != NULL) {
        *rt->origcolors = 0u;
    }
}

static SIXELSTATUS
sixel_kcenter_build_prepare_and_collect(sixel_kcenter_build_runtime_t *rt)
{
    unsigned int channel;
    float float_minimum;
    float float_maximum;
    double range;

    channel = 0u;
    float_minimum = 0.0f;
    float_maximum = 0.0f;
    range = 0.0;

    if (rt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (rt->allocator == NULL || rt->data == NULL || rt->result == NULL) {
        return rt->status;
    }

    rt->wall_start = sixel_timer_now();
    rt->init_stop = rt->wall_start;
    rt->iterate_start = rt->wall_start;
    rt->iterate_stop = rt->wall_start;
    rt->merge_start = rt->wall_start;
    rt->merge_stop = rt->wall_start;
    rt->export_start = rt->wall_start;
    rt->export_stop = rt->wall_start;

    rt->input_is_float32 = (rt->treat_input_as_float32
                            && SIXEL_PIXELFORMAT_IS_FLOAT32(rt->pixelformat));
    if (rt->input_is_float32) {
        for (channel = 0u; channel < 3u; ++channel) {
#if HAVE_FLOAT_H
# define SIXEL_KCENTER_FLOAT_BOUND FLT_MAX
#else
# define SIXEL_KCENTER_FLOAT_BOUND 1.0e9f
#endif
            float_minimum = sixel_pixelformat_float_channel_clamp(
                rt->pixelformat,
                (int)channel,
                -SIXEL_KCENTER_FLOAT_BOUND);
            float_maximum = sixel_pixelformat_float_channel_clamp(
                rt->pixelformat,
                (int)channel,
                SIXEL_KCENTER_FLOAT_BOUND);
#undef SIXEL_KCENTER_FLOAT_BOUND
            range = (double)float_maximum - (double)float_minimum;
            if (range <= 0.0) {
                rt->float32_channel_scale[channel] = 0.0;
                rt->float32_channel_offset[channel] = 0.0;
            } else {
                rt->float32_channel_scale[channel] = 255.0 / range;
                rt->float32_channel_offset[channel]
                    = -((double)float_minimum)
                    * rt->float32_channel_scale[channel];
            }
        }
    }

    if (rt->reqcolors == 0u) {
        rt->reqcolors = 1u;
    }

    sixel_kcenter_last_polish_applied = 0;
    sixel_kcenter_last_polish_updates = 0u;
    sixel_kcenter_last_polish_pre_radius2 = 0.0;
    sixel_kcenter_last_polish_post_radius2 = 0.0;

    rt->override_lock_acquired = sixel_kcenter_override_lock_acquire();

    rt->job_init = sixel_palette_kcenter_log_start(rt->logger,
                                                   rt->job_seq,
                                                   rt->engine_name,
                                                   "palette/init",
                                                   "init");

    rt->algo = sixel_get_kcenter_algo();
    rt->profile = sixel_get_kcenter_profile();
    rt->seed = (unsigned int)sixel_get_kcenter_seed();
    rt->restarts = sixel_get_kcenter_restarts();
    rt->init_seeds = sixel_get_kcenter_init_seeds();
    rt->iter_limit = sixel_get_kcenter_iter();
    rt->histbits = sixel_get_kcenter_histbits();
    rt->point_budget = sixel_get_kcenter_point_budget();
    rt->prune_mass = sixel_get_kcenter_prune_mass();
    rt->auto_policy = sixel_get_kcenter_auto_policy();
    rt->auto_fft_threshold = sixel_get_kcenter_auto_fft_threshold();
    rt->space_policy = sixel_get_kcenter_space_policy();
    rt->candidate_policy = sixel_get_kcenter_candidate_policy();
    rt->rare_keep = sixel_get_kcenter_rare_keep();
    rt->budget_policy = sixel_get_kcenter_budget_policy();
    rt->budget_scale = sixel_get_kcenter_budget_scale();
    rt->swap_topk = sixel_get_kcenter_swap_topk();
    rt->swap_update = sixel_get_kcenter_swap_update();
    rt->swap_patience = sixel_get_kcenter_swap_patience();
    rt->swap_min_gain = sixel_get_kcenter_swap_min_gain();

    sixel_kcenter_apply_perceptual_solver_bias(rt->profile,
                                               rt->space_policy,
                                               &rt->init_seeds,
                                               &rt->swap_topk);
    rt->use_cluster_candidates =
        (rt->profile != SIXEL_PALETTE_KCENTER_PROFILE_LEGACY
         && rt->swap_topk > 1u);
    rt->use_perceptual_polish =
        (rt->profile != SIXEL_PALETTE_KCENTER_PROFILE_LEGACY
         && sixel_kcenter_resolve_space_policy(rt->space_policy)
         == SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL
         && (rt->pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32
             || rt->pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32
             || rt->pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32));
    rt->use_legacy_oklab_polish =
        (rt->profile == SIXEL_PALETTE_KCENTER_PROFILE_LEGACY
         && rt->quality_mode != SIXEL_QUALITY_LOW
         && rt->pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32);

    rt->status = sixel_kcenter_collect_points(
        &rt->points,
        &rt->weights,
        &rt->point_count,
        &rt->visible_count,
        &rt->active_count,
        rt->data,
        rt->length,
        rt->depth,
        rt->pixelformat,
        rt->treat_input_as_float32,
        rt->histbits,
        rt->point_budget,
        rt->prune_mass,
        rt->reqcolors,
        rt->quality_mode,
        rt->candidate_policy,
        rt->space_policy,
        rt->rare_keep,
        rt->budget_policy,
        rt->budget_scale,
        rt->float32_channel_scale,
        rt->float32_channel_offset,
        rt->allocator);
    if (SIXEL_FAILED(rt->status)) {
        return rt->status;
    }

    if (rt->origcolors != NULL) {
        *rt->origcolors = rt->visible_count;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kcenter_build_allocate_solver_buffers(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    rt->centers = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * sizeof(unsigned int));
    rt->best_centers = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * sizeof(unsigned int));
    rt->work_centers = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * sizeof(unsigned int));
    rt->nearest_slot = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(unsigned int));
    rt->second_slot = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(unsigned int));
    rt->scratch_slot = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(unsigned int));
    rt->scratch_second_slot = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(unsigned int));
    rt->nearest_dist = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(double));
    rt->second_dist = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(double));
    rt->scratch_dist = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(double));
    rt->scratch_second_dist = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(double));
    rt->cluster_weights = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * sizeof(double));
    rt->cluster_sums = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * 3u * sizeof(double));
    rt->final_centers = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * 3u * sizeof(double));
    rt->scratch_indices = (unsigned int *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(unsigned int));
    rt->fft_dist_cache = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count * sizeof(double));
    rt->center_mask = (unsigned char *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->point_count);

    if (rt->centers == NULL || rt->best_centers == NULL
            || rt->work_centers == NULL || rt->nearest_slot == NULL
            || rt->second_slot == NULL || rt->scratch_slot == NULL
            || rt->scratch_second_slot == NULL || rt->nearest_dist == NULL
            || rt->second_dist == NULL || rt->scratch_dist == NULL
            || rt->scratch_second_dist == NULL || rt->cluster_weights == NULL
            || rt->cluster_sums == NULL || rt->final_centers == NULL
            || rt->scratch_indices == NULL || rt->fft_dist_cache == NULL
            || rt->center_mask == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    return SIXEL_OK;
}

typedef struct sixel_kcenter_build_solve_state {
    unsigned int slot;
} sixel_kcenter_build_solve_state_t;

static void
sixel_kcenter_build_solve_state_clear(sixel_kcenter_build_solve_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

static void
sixel_kcenter_build_solve_resolve_target(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    rt->resolved_merge = (unsigned int)sixel_resolve_final_merge_mode(
        rt->final_merge_mode);
    rt->apply_merge = (rt->resolved_merge == SIXEL_FINAL_MERGE_WARD);
    rt->overshoot = rt->reqcolors;
    if (rt->apply_merge) {
        sixel_final_merge_load_env();
        rt->overshoot = sixel_final_merge_target(
            rt->reqcolors,
            (int)rt->resolved_merge);
    }
    if (rt->overshoot > rt->point_count) {
        rt->overshoot = rt->point_count;
    }
    if (rt->overshoot == 0u) {
        rt->overshoot = 1u;
    }
    rt->k = rt->overshoot;
}

static SIXELSTATUS
sixel_kcenter_build_solve_prepare_solver(sixel_kcenter_build_runtime_t *rt)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ARGUMENT;
    if (rt == NULL) {
        return status;
    }

    status = sixel_kcenter_build_allocate_solver_buffers(rt);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    rt->resolved_algo = rt->algo;
    if (rt->resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_AUTO) {
        rt->resolved_algo = sixel_kcenter_choose_auto_algo(
            rt->quality_mode,
            rt->point_count,
            rt->auto_policy,
            rt->auto_fft_threshold,
            rt->pixelformat,
            rt->space_policy);
    }
    return SIXEL_OK;
}

static void
sixel_kcenter_build_solve_start_iterate_log(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    rt->init_stop = sixel_timer_now();
    rt->iterate_start = rt->init_stop;
    (void)sixel_compat_snprintf(
        rt->log_detail,
        sizeof(rt->log_detail),
        "samples=%u active=%u k=%u profile=%s algo=%s/%s auto=%s "
        "fft_threshold=%u space=%s cand=%s rare_keep=%u budget=%s "
        "scale=%.2f req_budget=%u prune_mass=%.3f swap_topk=%u "
        "swap_update=%s swap_patience=%u swap_min_gain=%.3f seed=%u "
        "init_seeds=%u histbits=%u",
        rt->point_count,
        rt->active_count,
        rt->k,
        sixel_kcenter_profile_to_string(rt->profile),
        sixel_kcenter_algo_to_string(rt->algo),
        sixel_kcenter_algo_to_string(rt->resolved_algo),
        sixel_kcenter_auto_policy_to_string(rt->auto_policy),
        rt->auto_fft_threshold,
        sixel_kcenter_space_policy_to_string(rt->space_policy),
        sixel_kcenter_candidate_policy_to_string(rt->candidate_policy),
        rt->rare_keep,
        sixel_kcenter_budget_policy_to_string(rt->budget_policy),
        rt->budget_scale,
        rt->point_budget,
        rt->prune_mass,
        rt->swap_topk,
        sixel_kcenter_swap_update_to_string(rt->swap_update),
        rt->swap_patience,
        rt->swap_min_gain,
        rt->seed,
        rt->init_seeds,
        rt->histbits);
    sixel_palette_kcenter_log_finish(rt->logger,
                                     rt->job_init,
                                     rt->engine_name,
                                     "palette/init",
                                     "init",
                                     rt->log_detail);

    rt->job_iter = sixel_palette_kcenter_log_start(rt->logger,
                                                   rt->job_seq,
                                                   rt->engine_name,
                                                   "palette/iterate",
                                                   "iterate");
}

static void
sixel_kcenter_build_solve_setup_solver_ctx(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    rt->solver_ctx.points = rt->points;
    rt->solver_ctx.weights = rt->weights;
    rt->solver_ctx.point_count = rt->point_count;
    rt->solver_ctx.k = rt->k;
    rt->solver_ctx.resolved_algo = rt->resolved_algo;
    rt->solver_ctx.profile = rt->profile;
    rt->solver_ctx.init_seeds = rt->init_seeds;
    rt->solver_ctx.iter_limit = rt->iter_limit;
    rt->solver_ctx.rng_state = &rt->rng_state;
    rt->solver_ctx.centers = rt->work_centers;
    rt->solver_ctx.nearest_slot = rt->nearest_slot;
    rt->solver_ctx.nearest_dist = rt->nearest_dist;
    rt->solver_ctx.second_slot = rt->second_slot;
    rt->solver_ctx.second_dist = rt->second_dist;
    rt->solver_ctx.scratch_slot = rt->scratch_slot;
    rt->solver_ctx.scratch_dist = rt->scratch_dist;
    rt->solver_ctx.scratch_second_slot = rt->scratch_second_slot;
    rt->solver_ctx.scratch_second_dist = rt->scratch_second_dist;
    rt->solver_ctx.swap_topk = rt->swap_topk;
    rt->solver_ctx.swap_update = rt->swap_update;
    rt->solver_ctx.swap_patience = rt->swap_patience;
    rt->solver_ctx.swap_min_gain = rt->swap_min_gain;
    rt->solver_ctx.use_cluster_candidates = rt->use_cluster_candidates;
    rt->solver_ctx.radius2_out = &rt->radius2;
    rt->solver_ctx.sse_out = &rt->sse;
    rt->solver_ctx.iterations_out = &rt->run_iterations;
    rt->solver_ctx.scratch_indices = rt->scratch_indices;
    rt->solver_ctx.fft_dist_cache = rt->fft_dist_cache;
    rt->solver_ctx.center_mask = rt->center_mask;
}

static SIXELSTATUS
sixel_kcenter_build_solve_run_restarts(sixel_kcenter_build_runtime_t *rt,
                                       sixel_kcenter_build_solve_state_t *state)
{
    SIXELSTATUS status;

    status = SIXEL_BAD_ARGUMENT;
    if (rt == NULL || state == NULL) {
        return status;
    }

    rt->best_radius2 = -1.0;
    rt->best_sse = 0.0;
    rt->best_iterations = 0u;
    for (state->slot = 0u; state->slot < rt->restarts; ++state->slot) {
        rt->rng_state = (uint32_t)(
            (uint64_t)rt->seed
            + (uint64_t)0x9e3779b9u * ((uint64_t)state->slot + 1u));
        status = sixel_kcenter_run_solver(&rt->solver_ctx);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        if (sixel_kcenter_solver_trial_is_better(rt->radius2,
                                                 rt->sse,
                                                 rt->best_radius2,
                                                 rt->best_sse)) {
            memcpy(rt->best_centers,
                   rt->work_centers,
                   (size_t)rt->k * sizeof(unsigned int));
            rt->best_radius2 = rt->radius2;
            rt->best_sse = rt->sse;
            rt->best_iterations = rt->run_iterations;
        }
    }
    return SIXEL_OK;
}

static void
sixel_kcenter_build_solve_assign_best(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    memcpy(rt->centers,
           rt->best_centers,
           (size_t)rt->k * sizeof(unsigned int));
    rt->total_iterations = rt->best_iterations;

    sixel_kcenter_assign_points_with_second(rt->points,
                                            rt->weights,
                                            rt->point_count,
                                            rt->centers,
                                            rt->k,
                                            rt->nearest_slot,
                                            rt->nearest_dist,
                                            rt->second_slot,
                                            rt->second_dist,
                                            &rt->radius2,
                                            &rt->sse,
                                            rt->cluster_weights,
                                            rt->cluster_sums);
}

static void
sixel_kcenter_build_solve_setup_polish_ctx(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    rt->polish_ctx.points = rt->points;
    rt->polish_ctx.weights = rt->weights;
    rt->polish_ctx.point_count = rt->point_count;
    rt->polish_ctx.centers = rt->centers;
    rt->polish_ctx.k = rt->k;
    rt->polish_ctx.nearest_slot = rt->nearest_slot;
    rt->polish_ctx.nearest_dist = rt->nearest_dist;
    rt->polish_ctx.second_slot = rt->second_slot;
    rt->polish_ctx.second_dist = rt->second_dist;
    rt->polish_ctx.scratch_slot = rt->scratch_slot;
    rt->polish_ctx.scratch_dist = rt->scratch_dist;
    rt->polish_ctx.scratch_second_slot = rt->scratch_second_slot;
    rt->polish_ctx.scratch_second_dist = rt->scratch_second_dist;
    rt->polish_ctx.center_mask = rt->center_mask;
    rt->polish_ctx.cluster_weights = rt->cluster_weights;
    rt->polish_ctx.cluster_sums = rt->cluster_sums;
    rt->polish_ctx.radius2_io = &rt->radius2;
    rt->polish_ctx.sse_io = &rt->sse;
}

static void
sixel_kcenter_build_solve_apply_polish(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    if (rt->use_legacy_oklab_polish) {
        rt->polish_point_limit = 2048u;
        rt->polish_k_limit = 96u;
    } else {
        rt->polish_point_limit = 8192u;
        rt->polish_k_limit = 128u;
    }
    if ((rt->use_perceptual_polish || rt->use_legacy_oklab_polish)
            && rt->resolved_algo != SIXEL_PALETTE_KCENTER_ALGO_FFT
            && rt->point_count <= rt->polish_point_limit
            && rt->k <= rt->polish_k_limit) {
        rt->polish_pre_radius2 = rt->radius2;
        sixel_kcenter_polish_sse_with_radius_guard(&rt->polish_ctx,
                                                   &rt->polish_updates);
        sixel_kcenter_last_polish_applied = 1;
        sixel_kcenter_last_polish_updates = rt->polish_updates;
        sixel_kcenter_last_polish_pre_radius2 = rt->polish_pre_radius2;
        sixel_kcenter_last_polish_post_radius2 = rt->radius2;
    }
}

static void
sixel_kcenter_build_solve_update_best_objective(
    sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    if (sixel_kcenter_solver_trial_is_better(rt->radius2,
                                             rt->sse,
                                             rt->best_radius2,
                                             rt->best_sse)) {
        rt->best_radius2 = rt->radius2;
        rt->best_sse = rt->sse;
    }
}

static void
sixel_kcenter_build_solve_finalize(sixel_kcenter_build_runtime_t *rt,
                                   sixel_kcenter_build_solve_state_t *state)
{
    if (rt == NULL || state == NULL) {
        return;
    }

    for (state->slot = 0u; state->slot < rt->k; ++state->slot) {
        rt->final_centers[state->slot * 3u + 0u]
            = rt->points[rt->centers[state->slot] * 3u];
        rt->final_centers[state->slot * 3u + 1u]
            = rt->points[rt->centers[state->slot] * 3u + 1u];
        rt->final_centers[state->slot * 3u + 2u]
            = rt->points[rt->centers[state->slot] * 3u + 2u];
    }
    rt->final_count = rt->k;

    rt->iterate_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(rt->log_detail,
                                sizeof(rt->log_detail),
                                "algo=%s iter=%u radius=%.4f sse=%.4f "
                                "polish_updates=%u",
                                sixel_kcenter_algo_to_string(rt->resolved_algo),
                                rt->total_iterations,
                                sqrt(rt->best_radius2),
                                rt->best_sse,
                                rt->polish_updates);
    sixel_palette_kcenter_log_finish(rt->logger,
                                     rt->job_iter,
                                     rt->engine_name,
                                     "palette/iterate",
                                     "iterate",
                                     rt->log_detail);

    rt->merge_start = rt->iterate_stop;
    rt->merge_stop = rt->iterate_stop;
}

static SIXELSTATUS
sixel_kcenter_build_solve_phase(sixel_kcenter_build_runtime_t *rt)
{
    sixel_kcenter_build_solve_state_t state;

    sixel_kcenter_build_solve_state_clear(&state);
    if (rt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Keep solve execution order fixed:
     * solve -> assign -> polish -> finalize.
     */
    sixel_kcenter_build_solve_resolve_target(rt);
    rt->status = sixel_kcenter_build_solve_prepare_solver(rt);
    if (SIXEL_FAILED(rt->status)) {
        return rt->status;
    }

    sixel_kcenter_build_solve_start_iterate_log(rt);
    sixel_kcenter_build_solve_setup_solver_ctx(rt);
    rt->status = sixel_kcenter_build_solve_run_restarts(rt, &state);
    if (SIXEL_FAILED(rt->status)) {
        return rt->status;
    }
    sixel_kcenter_build_solve_assign_best(rt);
    sixel_kcenter_build_solve_setup_polish_ctx(rt);
    sixel_kcenter_build_solve_apply_polish(rt);
    sixel_kcenter_build_solve_update_best_objective(rt);
    sixel_kcenter_build_solve_finalize(rt, &state);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kcenter_build_merge_phase(sixel_kcenter_build_runtime_t *rt)
{
    unsigned int slot;
    unsigned int channel;
    double weight_value;
    double component;

    slot = 0u;
    channel = 0u;
    weight_value = 0.0;
    component = 0.0;

    if (rt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!rt->apply_merge || rt->k <= rt->reqcolors) {
        return SIXEL_OK;
    }

    rt->merge_start = sixel_timer_now();
    rt->job_merge = sixel_palette_kcenter_log_start(rt->logger,
                                                    rt->job_seq,
                                                    rt->engine_name,
                                                    "palette/merge",
                                                    "merge");

    rt->merge_weights = (unsigned long *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * sizeof(unsigned long));
    rt->merge_sums = (double *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->k * 3u * sizeof(double));
    if (rt->merge_weights == NULL || rt->merge_sums == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    for (slot = 0u; slot < rt->k; ++slot) {
        weight_value = rt->cluster_weights[slot];
        if (weight_value <= 0.0) {
            rt->merge_weights[slot] = 1ul;
            rt->merge_sums[slot * 3u + 0u] = 0.0;
            rt->merge_sums[slot * 3u + 1u] = 0.0;
            rt->merge_sums[slot * 3u + 2u] = 0.0;
            continue;
        }
        if (weight_value > (double)ULONG_MAX) {
            rt->merge_weights[slot] = ULONG_MAX;
        } else {
            rt->merge_weights[slot] = (unsigned long)(weight_value + 0.5);
            if (rt->merge_weights[slot] == 0ul) {
                rt->merge_weights[slot] = 1ul;
            }
        }
        for (channel = 0u; channel < 3u; ++channel) {
            component = rt->cluster_sums[slot * 3u + channel]
                / rt->cluster_weights[slot];
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            rt->merge_sums[slot * 3u + channel]
                = component * (double)rt->merge_weights[slot];
        }
    }

    rt->cluster_total = sixel_palette_apply_merge(rt->merge_weights,
                                                  rt->merge_sums,
                                                  3u,
                                                  (int)rt->k,
                                                  (int)rt->reqcolors,
                                                  (int)rt->resolved_merge,
                                                  rt->use_reversible,
                                                  rt->pixelformat,
                                                  rt->allocator);
    if (rt->cluster_total < 1) {
        rt->cluster_total = 1;
    }
    if ((unsigned int)rt->cluster_total > rt->reqcolors) {
        rt->cluster_total = (int)rt->reqcolors;
    }
    rt->final_count = (unsigned int)rt->cluster_total;
    if (rt->final_count == 0u) {
        rt->final_count = 1u;
    }

    for (slot = 0u; slot < rt->final_count; ++slot) {
        weight_value = (double)rt->merge_weights[slot];
        if (weight_value <= 0.0) {
            weight_value = 1.0;
        }
        for (channel = 0u; channel < 3u; ++channel) {
            rt->final_centers[slot * 3u + channel]
                = rt->merge_sums[slot * 3u + channel] / weight_value;
        }
        rt->cluster_weights[slot] = weight_value;
    }

    rt->merge_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(rt->log_detail,
                                sizeof(rt->log_detail),
                                "clusters=%u merge=%u",
                                rt->final_count,
                                rt->resolved_merge);
    sixel_palette_kcenter_log_finish(rt->logger,
                                     rt->job_merge,
                                     rt->engine_name,
                                     "palette/merge",
                                     "merge",
                                     rt->log_detail);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kcenter_build_export_phase(sixel_kcenter_build_runtime_t *rt)
{
    unsigned int slot;
    unsigned int channel;
    unsigned int index;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    double component;
    double restored_component;
    int have_float_palette;

    slot = 0u;
    channel = 0u;
    index = 0u;
    fill = 0u;
    source = 0u;
    swap_temp = 0u;
    component = 0.0;
    restored_component = 0.0;
    have_float_palette = 0;

    if (rt == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    rt->export_start = sixel_timer_now();
    rt->job_export = sixel_palette_kcenter_log_start(rt->logger,
                                                     rt->job_seq,
                                                     rt->engine_name,
                                                     "palette/export",
                                                     "export");

    rt->palette = (unsigned char *)sixel_allocator_malloc(
        rt->allocator,
        (size_t)rt->final_count * 3u);
    if (rt->palette == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (rt->result_float32 != NULL
            && rt->input_is_float32
            && rt->final_count > 0u) {
        rt->float_palette = (float *)sixel_allocator_malloc(
            rt->allocator,
            (size_t)rt->final_count * 3u * sizeof(float));
        if (rt->float_palette == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
    }

    for (slot = 0u; slot < rt->final_count; ++slot) {
        for (channel = 0u; channel < 3u; ++channel) {
            component = rt->final_centers[slot * 3u + channel];
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            if (rt->float_palette != NULL) {
                if (rt->float32_channel_scale[channel] > 0.0) {
                    restored_component = component;
                    restored_component -= rt->float32_channel_offset[channel];
                    restored_component /= rt->float32_channel_scale[channel];
                } else {
                    restored_component = 0.0;
                }
                rt->float_palette[slot * 3u + channel]
                    = sixel_pixelformat_float_channel_clamp(
                        rt->pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            rt->palette[slot * 3u + channel] = (unsigned char)(component + 0.5);
        }
    }

    if (rt->force_palette
            && rt->final_count < rt->reqcolors
            && rt->final_count > 0u) {
        rt->grown_palette = (unsigned char *)sixel_allocator_malloc(
            rt->allocator,
            (size_t)rt->reqcolors * 3u);
        rt->sort_weights = (double *)sixel_allocator_malloc(
            rt->allocator,
            (size_t)rt->final_count * sizeof(double));
        rt->order = (unsigned int *)sixel_allocator_malloc(
            rt->allocator,
            (size_t)rt->final_count * sizeof(unsigned int));
        if (rt->grown_palette == NULL
                || rt->sort_weights == NULL
                || rt->order == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        if (rt->float_palette != NULL) {
            rt->grown_float = (float *)sixel_allocator_malloc(
                rt->allocator,
                (size_t)rt->reqcolors * 3u * sizeof(float));
            if (rt->grown_float == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
        }
        if (rt->grown_float != NULL && rt->float_palette == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        have_float_palette = (rt->grown_float != NULL
                              && rt->float_palette != NULL);

        memcpy(rt->grown_palette,
               rt->palette,
               (size_t)rt->final_count * 3u * sizeof(unsigned char));
        if (have_float_palette) {
            memcpy(rt->grown_float,
                   rt->float_palette,
                   (size_t)rt->final_count * 3u * sizeof(float));
        }

        for (index = 0u; index < rt->final_count; ++index) {
            rt->order[index] = index;
            rt->sort_weights[index] = rt->cluster_weights[index];
        }
        for (index = 0u; index + 1u < rt->final_count; ++index) {
            for (slot = index + 1u; slot < rt->final_count; ++slot) {
                if (rt->sort_weights[rt->order[slot]]
                        > rt->sort_weights[rt->order[index]]) {
                    swap_temp = rt->order[index];
                    rt->order[index] = rt->order[slot];
                    rt->order[slot] = swap_temp;
                }
            }
        }

        fill = rt->final_count;
        source = 0u;
        while (fill < rt->reqcolors) {
            slot = rt->order[source];
            for (channel = 0u; channel < 3u; ++channel) {
                rt->grown_palette[fill * 3u + channel]
                    = rt->palette[slot * 3u + channel];
                if (have_float_palette) {
                    rt->grown_float[fill * 3u + channel]
                        = rt->float_palette[slot * 3u + channel];
                }
            }
            ++fill;
            ++source;
            if (source >= rt->final_count) {
                source = 0u;
            }
        }

        sixel_allocator_free(rt->allocator, rt->palette);
        rt->palette = rt->grown_palette;
        rt->grown_palette = NULL;
        if (rt->float_palette != NULL) {
            sixel_allocator_free(rt->allocator, rt->float_palette);
            rt->float_palette = rt->grown_float;
            rt->grown_float = NULL;
        }
        rt->final_count = rt->reqcolors;
    }

    *rt->result = rt->palette;
    rt->palette = NULL;
    if (rt->result_float32 != NULL) {
        *rt->result_float32 = rt->float_palette;
        rt->float_palette = NULL;
    }
    if (rt->ncolors != NULL) {
        *rt->ncolors = rt->final_count;
    }

    rt->export_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(rt->log_detail,
                                sizeof(rt->log_detail),
                                "colors=%u",
                                rt->final_count);
    sixel_palette_kcenter_log_finish(rt->logger,
                                     rt->job_export,
                                     rt->engine_name,
                                     "palette/export",
                                     "export",
                                     rt->log_detail);
    return SIXEL_OK;
}

static void
sixel_kcenter_build_finalize_telemetry(sixel_kcenter_build_runtime_t *rt)
{
    double now;
    double init_span;
    double iterate_span;
    double merge_span;
    double export_span;

    now = 0.0;
    init_span = 0.0;
    iterate_span = 0.0;
    merge_span = 0.0;
    export_span = 0.0;

    if (rt == NULL || rt->telemetry == NULL) {
        return;
    }

    now = sixel_timer_now();
    if (rt->init_stop < rt->wall_start) {
        rt->init_stop = now;
    }
    if (rt->iterate_stop < rt->iterate_start) {
        rt->iterate_stop = rt->init_stop;
    }
    if (rt->merge_stop < rt->merge_start) {
        rt->merge_stop = rt->iterate_stop;
    }
    if (rt->export_stop < rt->export_start) {
        rt->export_stop = now;
    }

    init_span = rt->init_stop - rt->wall_start;
    if (init_span < 0.0) {
        init_span = 0.0;
    }
    iterate_span = rt->iterate_stop - rt->iterate_start;
    if (iterate_span < 0.0) {
        iterate_span = 0.0;
    }
    merge_span = rt->merge_stop - rt->merge_start;
    if (merge_span < 0.0) {
        merge_span = 0.0;
    }
    export_span = rt->export_stop - rt->export_start;
    if (export_span < 0.0) {
        export_span = 0.0;
    }

    rt->telemetry->init_ms = init_span * 1000.0;
    rt->telemetry->iterate_ms = iterate_span * 1000.0;
    rt->telemetry->merge_ms = merge_span * 1000.0;
    rt->telemetry->export_ms = export_span * 1000.0;
    rt->telemetry->iterate_count = rt->total_iterations;
    rt->telemetry->merge_iterate_count
        = (rt->apply_merge && rt->k > rt->reqcolors) ? 1u : 0u;
    rt->telemetry->merge_mode
        = (rt->apply_merge && rt->k > rt->reqcolors)
        ? (int)rt->resolved_merge
        : SIXEL_FINAL_MERGE_NONE;
}

static void
sixel_kcenter_build_cleanup(sixel_kcenter_build_runtime_t *rt)
{
    if (rt == NULL || rt->allocator == NULL) {
        return;
    }
    /*
     * Release temporary storage in reverse ownership order so partially
     * completed stages remain easy to audit during failure debugging.
     */
    if (rt->grown_float != NULL) {
        sixel_allocator_free(rt->allocator, rt->grown_float);
    }
    if (rt->grown_palette != NULL) {
        sixel_allocator_free(rt->allocator, rt->grown_palette);
    }
    if (rt->float_palette != NULL) {
        sixel_allocator_free(rt->allocator, rt->float_palette);
    }
    if (rt->palette != NULL) {
        sixel_allocator_free(rt->allocator, rt->palette);
    }
    if (rt->merge_sums != NULL) {
        sixel_allocator_free(rt->allocator, rt->merge_sums);
    }
    if (rt->merge_weights != NULL) {
        sixel_allocator_free(rt->allocator, rt->merge_weights);
    }
    if (rt->center_mask != NULL) {
        sixel_allocator_free(rt->allocator, rt->center_mask);
    }
    if (rt->fft_dist_cache != NULL) {
        sixel_allocator_free(rt->allocator, rt->fft_dist_cache);
    }
    if (rt->scratch_indices != NULL) {
        sixel_allocator_free(rt->allocator, rt->scratch_indices);
    }
    if (rt->order != NULL) {
        sixel_allocator_free(rt->allocator, rt->order);
    }
    if (rt->sort_weights != NULL) {
        sixel_allocator_free(rt->allocator, rt->sort_weights);
    }
    if (rt->final_centers != NULL) {
        sixel_allocator_free(rt->allocator, rt->final_centers);
    }
    if (rt->cluster_sums != NULL) {
        sixel_allocator_free(rt->allocator, rt->cluster_sums);
    }
    if (rt->cluster_weights != NULL) {
        sixel_allocator_free(rt->allocator, rt->cluster_weights);
    }
    if (rt->scratch_dist != NULL) {
        sixel_allocator_free(rt->allocator, rt->scratch_dist);
    }
    if (rt->scratch_second_dist != NULL) {
        sixel_allocator_free(rt->allocator, rt->scratch_second_dist);
    }
    if (rt->nearest_dist != NULL) {
        sixel_allocator_free(rt->allocator, rt->nearest_dist);
    }
    if (rt->second_dist != NULL) {
        sixel_allocator_free(rt->allocator, rt->second_dist);
    }
    if (rt->scratch_slot != NULL) {
        sixel_allocator_free(rt->allocator, rt->scratch_slot);
    }
    if (rt->scratch_second_slot != NULL) {
        sixel_allocator_free(rt->allocator, rt->scratch_second_slot);
    }
    if (rt->nearest_slot != NULL) {
        sixel_allocator_free(rt->allocator, rt->nearest_slot);
    }
    if (rt->second_slot != NULL) {
        sixel_allocator_free(rt->allocator, rt->second_slot);
    }
    if (rt->work_centers != NULL) {
        sixel_allocator_free(rt->allocator, rt->work_centers);
    }
    if (rt->best_centers != NULL) {
        sixel_allocator_free(rt->allocator, rt->best_centers);
    }
    if (rt->centers != NULL) {
        sixel_allocator_free(rt->allocator, rt->centers);
    }
    if (rt->weights != NULL) {
        sixel_allocator_free(rt->allocator, rt->weights);
    }
    if (rt->points != NULL) {
        sixel_allocator_free(rt->allocator, rt->points);
    }
    sixel_kcenter_override_lock_release(rt->override_lock_acquired);
}

static SIXELSTATUS
build_palette_kcenter(sixel_kcenter_build_ctx_t *ctx)
{
    sixel_kcenter_build_runtime_t runtime;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_kcenter_build_runtime_init(&runtime, ctx);

    if (runtime.allocator == NULL
            || runtime.data == NULL
            || runtime.result == NULL) {
        return runtime.status;
    }

    runtime.status = sixel_kcenter_build_prepare_and_collect(&runtime);
    if (SIXEL_FAILED(runtime.status)) {
        goto end;
    }

    /*
     * Keep build stages strictly ordered:
     * collect -> solve -> merge -> export.
     * This preserves tie-break and logging behavior.
     */
    if (runtime.point_count == 0u) {
        runtime.status = SIXEL_OK;
        goto end;
    }

    runtime.status = sixel_kcenter_build_solve_phase(&runtime);
    if (SIXEL_FAILED(runtime.status)) {
        goto end;
    }

    runtime.status = sixel_kcenter_build_merge_phase(&runtime);
    if (SIXEL_FAILED(runtime.status)) {
        goto end;
    }

    runtime.status = sixel_kcenter_build_export_phase(&runtime);
    if (SIXEL_FAILED(runtime.status)) {
        goto end;
    }

    runtime.status = SIXEL_OK;

end:
    sixel_kcenter_build_finalize_telemetry(&runtime);
    sixel_kcenter_build_cleanup(&runtime);
    return runtime.status;
}

static SIXELSTATUS
sixel_palette_build_kcenter_internal(sixel_kcenter_internal_ctx_t *ctx)
{
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_timeline_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    int treat_input_as_float32;
    sixel_palette_telemetry_t *telemetry;
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
    sixel_kcenter_build_ctx_t *build_ctx;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = ctx->palette;
    data = ctx->data;
    length = ctx->length;
    pixelformat = ctx->pixelformat;
    allocator = ctx->allocator;
    logger = ctx->logger;
    job_seq = ctx->job_seq;
    engine_name = ctx->engine_name;
    treat_input_as_float32 = ctx->treat_input_as_float32;
    telemetry = ctx->telemetry;

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
    build_ctx = NULL;

    if (palette == NULL) {
        return status;
    }
    if (work_allocator == NULL) {
        work_allocator = SIXEL_PALETTE_STORAGE(palette)->allocator;
    }
    if (work_allocator == NULL) {
        return status;
    }

    build_ctx = (sixel_kcenter_build_ctx_t *)sixel_allocator_malloc(
        work_allocator,
        sizeof(sixel_kcenter_build_ctx_t));
    if (build_ctx == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(build_ctx, 0, sizeof(sixel_kcenter_build_ctx_t));

    depth_result = sixel_helper_compute_depth(pixelformat);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kcenter: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    input_depth = (unsigned int)depth_result;

    depth_result = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kcenter: rgb888 depth lookup failed.");
        return SIXEL_BAD_ARGUMENT;
    }
    entry_depth = (unsigned int)depth_result;

    build_ctx->result = &entries;
    build_ctx->result_float32 = &entries_float32;
    build_ctx->data = data;
    build_ctx->length = length;
    build_ctx->depth = input_depth;
    build_ctx->reqcolors = SIXEL_PALETTE_STORAGE(palette)->requested_colors;
    build_ctx->ncolors = &ncolors;
    build_ctx->origcolors = &origcolors;
    build_ctx->quality_mode = SIXEL_PALETTE_CONTEXT(palette)->quality_mode;
    build_ctx->force_palette = SIXEL_PALETTE_CONTEXT(palette)->force_palette;
    build_ctx->use_reversible = SIXEL_PALETTE_CONTEXT(palette)->use_reversible;
    build_ctx->final_merge_mode = SIXEL_PALETTE_CONTEXT(palette)
        ->final_merge_mode;
    build_ctx->allocator = work_allocator;
    build_ctx->pixelformat = pixelformat;
    build_ctx->treat_input_as_float32 = treat_input_as_float32;
    build_ctx->logger = logger;
    build_ctx->job_seq = job_seq;
    build_ctx->engine_name = engine_name;
    build_ctx->telemetry = telemetry;

    status = build_palette_kcenter(build_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (SIXEL_PALETTE_CONTEXT(palette)->use_reversible && entries != NULL) {
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
        if (SIXEL_PALETTE_STORAGE(palette)->entries == NULL
                || entries == NULL) {
            sixel_helper_set_additional_message(
                "sixel_palette_build_kcenter: palette payload is missing.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(SIXEL_PALETTE_STORAGE(palette)->entries,
               entries,
               payload_size);
    }
    SIXEL_PALETTE_STORAGE(palette)->entry_count = ncolors;
    SIXEL_PALETTE_STORAGE(palette)->original_colors = origcolors;
    SIXEL_PALETTE_STORAGE(palette)->depth = (int)entry_depth;

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
    if (build_ctx != NULL) {
        sixel_allocator_free(work_allocator, build_ctx);
    }
    if (entries_float32 != NULL) {
        sixel_allocator_free(work_allocator, entries_float32);
    }
    if (entries != NULL) {
        sixel_allocator_free(work_allocator, entries);
    }
    return status;
}

SIXELSTATUS
sixel_palette_build_kcenter(sixel_palette_t *palette,
                            unsigned char const *data,
                            unsigned int length,
                            int pixelformat,
                            sixel_allocator_t *allocator,
                            sixel_timeline_logger_t *logger,
                            int *job_seq,
                            char const *engine_name,
                            sixel_palette_telemetry_t *telemetry)
{
    sixel_kcenter_internal_ctx_t internal_ctx;

    memset(&internal_ctx, 0, sizeof(internal_ctx));
    internal_ctx.palette = palette;
    internal_ctx.data = data;
    internal_ctx.length = length;
    internal_ctx.pixelformat = pixelformat;
    internal_ctx.allocator = allocator;
    internal_ctx.logger = logger;
    internal_ctx.job_seq = job_seq;
    internal_ctx.engine_name = engine_name;
    internal_ctx.treat_input_as_float32 = 0;
    internal_ctx.telemetry = telemetry;
    return sixel_palette_build_kcenter_internal(&internal_ctx);
}

SIXELSTATUS
sixel_palette_build_kcenter_float32(sixel_palette_t *palette,
                                    float const *data,
                                    unsigned int length,
                                    int pixelformat,
                                    sixel_allocator_t *allocator,
                                    sixel_timeline_logger_t *logger,
                                    int *job_seq,
                                    char const *engine_name,
                                    sixel_palette_telemetry_t *telemetry)
{
    sixel_kcenter_internal_ctx_t internal_ctx;

    memset(&internal_ctx, 0, sizeof(internal_ctx));
    internal_ctx.palette = palette;
    internal_ctx.data = (unsigned char const *)data;
    internal_ctx.length = length;
    internal_ctx.pixelformat = pixelformat;
    internal_ctx.allocator = allocator;
    internal_ctx.logger = logger;
    internal_ctx.job_seq = job_seq;
    internal_ctx.engine_name = engine_name;
    internal_ctx.treat_input_as_float32 = 1;
    internal_ctx.telemetry = telemetry;
    return sixel_palette_build_kcenter_internal(&internal_ctx);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
