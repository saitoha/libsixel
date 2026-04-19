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
#include "logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kcenter.h"
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
    sixel_logger_t *logger;
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
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    int treat_input_as_float32;
    sixel_palette_telemetry_t *telemetry;
} sixel_kcenter_internal_ctx_t;

static int
sixel_palette_kcenter_log_start(sixel_logger_t *logger,
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
sixel_palette_kcenter_log_finish(sixel_logger_t *logger,
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
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int pixel_count;
    unsigned int bin_count;
    unsigned int shift_bits;
    unsigned int active_count;
    unsigned int visible_count;
    unsigned int keep_count;
    unsigned int budget;
    unsigned int selected_count;
    unsigned int rare_limit;
    unsigned int high_target;
    unsigned int tail_index;
    unsigned int retained_count;
    unsigned int index;
    unsigned int ri;
    unsigned int gi;
    unsigned int bi;
    unsigned int bin_index;
    unsigned int hue_bucket;
    unsigned int luma_bucket;
    unsigned int chroma_bucket;
    unsigned int strata_index;
    unsigned int strata_rank;
    unsigned int strata_swap;
    unsigned int offset;
    unsigned int alpha_byte;
    double total_weight;
    double keep_target;
    double accum_weight;
    double retained_weight;
    double entropy;
    double entropy_norm;
    double effective_bins;
    double effective_density;
    double probability;
    double mean_r;
    double mean_g;
    double mean_b;
    double dr;
    double dg;
    double db;
    double red;
    double green;
    double blue;
    double max_component;
    double min_component;
    double delta_component;
    double hue_value;
    double hue_phase;
    double luma_value;
    double normalized_hue;
    double chroma_value;
    double chroma_target;
    double chroma_accum;
    int use_perceptual_strata;
    int oklab_perceptual_space;
    int chroma_edge_found;
    double *sums;
    unsigned int *counts;
    unsigned char *bin_selected;
    sixel_kcenter_bin_t *bins;
    sixel_kcenter_dispersion_rank_t *dispersion;
    sixel_kcenter_dispersion_rank_t *chroma_rank;
    double *chroma_cache;
    double *points;
    double *weights;
    float const *pixel_float;
    int input_is_float32;
    unsigned int strata_picks[SIXEL_KCENTER_STRATA_BUCKETS];
    double strata_scores[SIXEL_KCENTER_STRATA_BUCKETS];
    unsigned int strata_order[SIXEL_KCENTER_STRATA_BUCKETS];
    double chroma_edges[SIXEL_KCENTER_CHROMA_BUCKETS - 1u];

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_stride = depth;
    pixel_count = 0u;
    bin_count = 0u;
    shift_bits = 0u;
    active_count = 0u;
    visible_count = 0u;
    keep_count = 0u;
    budget = point_budget;
    selected_count = 0u;
    rare_limit = 0u;
    high_target = 0u;
    tail_index = 0u;
    retained_count = 0u;
    index = 0u;
    ri = 0u;
    gi = 0u;
    bi = 0u;
    bin_index = 0u;
    hue_bucket = 0u;
    luma_bucket = 0u;
    chroma_bucket = 0u;
    strata_index = 0u;
    strata_rank = 0u;
    strata_swap = 0u;
    offset = 0u;
    alpha_byte = 0u;
    total_weight = 0.0;
    keep_target = 0.0;
    accum_weight = 0.0;
    retained_weight = 0.0;
    entropy = 0.0;
    entropy_norm = 0.0;
    effective_bins = 0.0;
    effective_density = 0.0;
    probability = 0.0;
    mean_r = 0.0;
    mean_g = 0.0;
    mean_b = 0.0;
    dr = 0.0;
    dg = 0.0;
    db = 0.0;
    red = 0.0;
    green = 0.0;
    blue = 0.0;
    max_component = 0.0;
    min_component = 0.0;
    delta_component = 0.0;
    hue_value = 0.0;
    luma_value = 0.0;
    normalized_hue = 0.0;
    chroma_value = 0.0;
    hue_phase = 0.0;
    chroma_target = 0.0;
    chroma_accum = 0.0;
    use_perceptual_strata = 0;
    oklab_perceptual_space = 0;
    chroma_edge_found = 0;
    sums = NULL;
    counts = NULL;
    bin_selected = NULL;
    bins = NULL;
    dispersion = NULL;
    chroma_rank = NULL;
    chroma_cache = NULL;
    points = NULL;
    weights = NULL;
    pixel_float = NULL;
    input_is_float32 = 0;
    for (strata_index = 0u;
            strata_index < SIXEL_KCENTER_STRATA_BUCKETS;
            ++strata_index) {
        strata_picks[strata_index] = UINT_MAX;
        strata_scores[strata_index] = -1.0;
        strata_order[strata_index] = strata_index;
    }
    for (strata_index = 0u;
            strata_index + 1u < SIXEL_KCENTER_CHROMA_BUCKETS;
            ++strata_index) {
        chroma_edges[strata_index] = 0.0;
    }

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

    input_is_float32 = (treat_input_as_float32
                        && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat));
    if (input_is_float32) {
        if (depth == 0u || depth % (unsigned int)sizeof(float) != 0u) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = depth / (unsigned int)sizeof(float);
        pixel_stride = depth;
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

    shift_bits = 8u - histbits;
    bin_count = 1u << (histbits * 3u);

    counts = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * sizeof(unsigned int));
    sums = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * 3u * sizeof(double));
    if (counts == NULL || sums == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memset(counts, 0, (size_t)bin_count * sizeof(unsigned int));
    memset(sums, 0, (size_t)bin_count * 3u * sizeof(double));

    for (index = 0u; index < pixel_count; ++index) {
        offset = index * pixel_stride;
        if (channels == 4u) {
            if (input_is_float32) {
                pixel_float = (float const *)(void const *)(data + offset);
                if (sixel_pixelformat_float_channel_clamp(
                        pixelformat,
                        3,
                        pixel_float[3]) <= 0.0f) {
                    continue;
                }
            } else {
                alpha_byte = data[offset + 3u];
                if (alpha_byte == 0u) {
                    continue;
                }
            }
        }

        if (input_is_float32) {
            pixel_float = (float const *)(void const *)(data + offset);
            red = (double)sixel_pixelformat_float_channel_clamp(
                pixelformat,
                0,
                pixel_float[0]);
            green = (double)sixel_pixelformat_float_channel_clamp(
                pixelformat,
                1,
                pixel_float[1]);
            blue = (double)sixel_pixelformat_float_channel_clamp(
                pixelformat,
                2,
                pixel_float[2]);
            red = red * float32_channel_scale[0] + float32_channel_offset[0];
            green = green * float32_channel_scale[1]
                + float32_channel_offset[1];
            blue = blue * float32_channel_scale[2]
                + float32_channel_offset[2];
        } else {
            red = (double)data[offset + 0u];
            green = (double)data[offset + 1u];
            blue = (double)data[offset + 2u];
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

        ri = ((unsigned int)red) >> shift_bits;
        gi = ((unsigned int)green) >> shift_bits;
        bi = ((unsigned int)blue) >> shift_bits;
        bin_index = (ri << (histbits * 2u)) | (gi << histbits) | bi;
        if (bin_index >= bin_count) {
            continue;
        }
        counts[bin_index] += 1u;
        sums[bin_index * 3u + 0u] += red;
        sums[bin_index * 3u + 1u] += green;
        sums[bin_index * 3u + 2u] += blue;
        ++visible_count;
    }

    *visible_count_out = visible_count;
    if (visible_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    for (index = 0u; index < bin_count; ++index) {
        if (counts[index] > 0u) {
            ++active_count;
        }
    }
    if (active_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    bins = (sixel_kcenter_bin_t *)sixel_allocator_malloc(
        allocator,
        (size_t)active_count * sizeof(sixel_kcenter_bin_t));
    if (bins == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    keep_count = 0u;
    total_weight = 0.0;
    for (index = 0u; index < bin_count; ++index) {
        if (counts[index] == 0u) {
            continue;
        }
        bins[keep_count].index = index;
        bins[keep_count].count = counts[index];
        bins[keep_count].r = sums[index * 3u + 0u] / (double)counts[index];
        bins[keep_count].g = sums[index * 3u + 1u] / (double)counts[index];
        bins[keep_count].b = sums[index * 3u + 2u] / (double)counts[index];
        total_weight += (double)counts[index];
        ++keep_count;
    }

    qsort(bins,
          keep_count,
          sizeof(sixel_kcenter_bin_t),
          sixel_kcenter_compare_bin_desc);

    keep_target = prune_mass * total_weight;
    if (keep_target < 1.0) {
        keep_target = 1.0;
    }
    accum_weight = 0.0;
    active_count = 0u;
    for (index = 0u; index < keep_count; ++index) {
        accum_weight += (double)bins[index].count;
        ++active_count;
        if (accum_weight >= keep_target && active_count >= reqcolors) {
            break;
        }
    }
    if (active_count == 0u) {
        active_count = 1u;
    }
    retained_count = active_count;
    *active_count_out = retained_count;
    retained_weight = 0.0;
    entropy = 0.0;
    for (index = 0u; index < retained_count; ++index) {
        retained_weight += (double)bins[index].count;
    }
    if (retained_weight > 0.0 && retained_count > 1u) {
        for (index = 0u; index < retained_count; ++index) {
            probability = (double)bins[index].count / retained_weight;
            if (probability <= 0.0) {
                continue;
            }
            entropy -= probability * log(probability);
        }
        entropy_norm = entropy / log((double)retained_count);
        if (entropy_norm < 0.0) {
            entropy_norm = 0.0;
        } else if (entropy_norm > 1.0) {
            entropy_norm = 1.0;
        }
        effective_bins = exp(entropy);
        effective_density = effective_bins / (double)retained_count;
        if (effective_density < 0.0) {
            effective_density = 0.0;
        } else if (effective_density > 1.0) {
            effective_density = 1.0;
        }
    } else {
        entropy_norm = 0.0;
        effective_density = 0.0;
    }

    if (budget == 0u) {
        if (sixel_kcenter_resolve_budget_policy(budget_policy)
                == SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE) {
            budget = sixel_kcenter_auto_point_budget_adaptive(
                reqcolors,
                visible_count,
                retained_count,
                quality_mode,
                entropy_norm,
                effective_density);
        } else {
            budget = sixel_kcenter_auto_point_budget_legacy(reqcolors,
                                                            visible_count,
                                                            quality_mode);
        }
        budget = sixel_kcenter_apply_budget_scale(budget, budget_scale);
    }
    if (budget > retained_count) {
        budget = retained_count;
    }
    if (budget == 0u) {
        budget = 1u;
    }

    points = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)budget * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)budget * sizeof(double));
    if (points == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (sixel_kcenter_resolve_candidate_policy(candidate_policy)
            == SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID) {
        use_perceptual_strata = 0;
        if (sixel_kcenter_resolve_space_policy(space_policy)
                == SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL
                && (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32
                    || pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32
                    || pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32)) {
            use_perceptual_strata = 1;
            if (pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32) {
                oklab_perceptual_space = 1;
            }
        }
        bin_selected = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)retained_count);
        dispersion = (sixel_kcenter_dispersion_rank_t *)
            sixel_allocator_malloc(
                allocator,
                (size_t)retained_count
                * sizeof(sixel_kcenter_dispersion_rank_t));
        if (use_perceptual_strata) {
            chroma_cache = (double *)sixel_allocator_malloc(
                allocator,
                (size_t)retained_count * sizeof(double));
            chroma_rank = (sixel_kcenter_dispersion_rank_t *)
                sixel_allocator_malloc(
                    allocator,
                    (size_t)retained_count
                    * sizeof(sixel_kcenter_dispersion_rank_t));
        }
        if (bin_selected == NULL || dispersion == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (use_perceptual_strata
                && (chroma_cache == NULL || chroma_rank == NULL)) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memset(bin_selected, 0, (size_t)retained_count);

        rare_limit = rare_keep;
        if (rare_limit > budget) {
            rare_limit = budget;
        }
        if (rare_limit > retained_count) {
            rare_limit = retained_count;
        }
        for (index = 0u; index < rare_limit; ++index) {
            tail_index = retained_count - 1u - index;
            if (bin_selected[tail_index] != 0u) {
                continue;
            }
            bin_selected[tail_index] = 1u;
            ++selected_count;
        }

        mean_r = 0.0;
        mean_g = 0.0;
        mean_b = 0.0;
        total_weight = 0.0;
        for (index = 0u; index < retained_count; ++index) {
            total_weight += (double)bins[index].count;
            mean_r += bins[index].r * (double)bins[index].count;
            mean_g += bins[index].g * (double)bins[index].count;
            mean_b += bins[index].b * (double)bins[index].count;
        }
        if (total_weight > 0.0) {
            mean_r /= total_weight;
            mean_g /= total_weight;
            mean_b /= total_weight;
        }

        for (index = 0u; index < retained_count; ++index) {
            dr = bins[index].r - mean_r;
            dg = bins[index].g - mean_g;
            db = bins[index].b - mean_b;
            dispersion[index].index = index;
            dispersion[index].score = (double)bins[index].count
                * (dr * dr + dg * dg + db * db);
            if (use_perceptual_strata) {
                /*
                 * Perceptual spaces already encode opponent channels on a/b.
                 * Use raw channels so strata bins stay stable even if the
                 * frame-wise mean drifts.
                 */
                chroma_value = sqrt(bins[index].g * bins[index].g
                                    + bins[index].b * bins[index].b);
                chroma_cache[index] = chroma_value;
                chroma_rank[index].index = index;
                chroma_rank[index].score = chroma_value;
            }
        }
        qsort(dispersion,
              retained_count,
              sizeof(sixel_kcenter_dispersion_rank_t),
              sixel_kcenter_compare_dispersion_desc);
        /*
         * Keep runtime guards minimal for MSVC /analyze.  The bucket count is
         * currently a fixed compile-time constant.
         */
        if (use_perceptual_strata
                && retained_count > 0u) {
            qsort(chroma_rank,
                  retained_count,
                  sizeof(sixel_kcenter_dispersion_rank_t),
                  sixel_kcenter_compare_dispersion_asc);
            for (strata_rank = 0u;
                    strata_rank + 1u < SIXEL_KCENTER_CHROMA_BUCKETS;
                    ++strata_rank) {
                chroma_target = retained_weight
                    * (double)(strata_rank + 1u)
                    / (double)SIXEL_KCENTER_CHROMA_BUCKETS;
                chroma_accum = 0.0;
                chroma_edges[strata_rank] = chroma_rank[
                    retained_count - 1u].score;
                chroma_edge_found = 0;
                for (strata_swap = 0u;
                        strata_swap < retained_count;
                        ++strata_swap) {
                    bin_index = chroma_rank[strata_swap].index;
                    chroma_accum += (double)bins[bin_index].count;
                    if (chroma_accum + 1.0e-12 < chroma_target) {
                        continue;
                    }
                    chroma_edges[strata_rank]
                        = chroma_rank[strata_swap].score;
                    chroma_edge_found = 1;
                    break;
                }
                if (!chroma_edge_found) {
                    chroma_edges[strata_rank] = chroma_rank[
                        retained_count - 1u].score;
                }
            }
        }

        for (strata_index = 0u;
                strata_index < SIXEL_KCENTER_STRATA_BUCKETS;
                ++strata_index) {
            strata_picks[strata_index] = UINT_MAX;
            strata_scores[strata_index] = -1.0;
            strata_order[strata_index] = strata_index;
        }
        for (index = 0u; index < retained_count; ++index) {
            if (use_perceptual_strata) {
                luma_value = bins[index].r;
            } else {
                luma_value = bins[index].r * 0.299
                    + bins[index].g * 0.587
                    + bins[index].b * 0.114;
            }
            luma_bucket = (unsigned int)(
                (luma_value * (double)SIXEL_KCENTER_LUMA_BUCKETS) / 256.0);
            if (luma_bucket >= SIXEL_KCENTER_LUMA_BUCKETS) {
                luma_bucket = SIXEL_KCENTER_LUMA_BUCKETS - 1u;
            }

            if (use_perceptual_strata) {
                chroma_value = chroma_cache[index];
                if (chroma_value <= 1.0e-9) {
                    /*
                     * Near-neutral bins have unstable hue.  Pin the hue bucket
                     * instead of amplifying angle noise.
                     */
                    normalized_hue = 0.0;
                } else {
                    hue_phase = atan2(bins[index].b, bins[index].g);
                    normalized_hue = (hue_phase + SIXEL_KCENTER_PI)
                        / (2.0 * SIXEL_KCENTER_PI);
                    if (normalized_hue < 0.0) {
                        normalized_hue = 0.0;
                    }
                    if (normalized_hue >= 1.0) {
                        normalized_hue = 0.999999;
                    }
                }
                dr = bins[index].r - mean_r;
                chroma_bucket = 0u;
                while (chroma_bucket + 1u < SIXEL_KCENTER_CHROMA_BUCKETS) {
                    if (chroma_value
                            <= chroma_edges[chroma_bucket] + 1.0e-12) {
                        break;
                    }
                    ++chroma_bucket;
                }
                probability = (double)bins[index].count
                    * (1.10 * dr * dr
                       + 1.50 * chroma_value * chroma_value);
            } else {
                max_component = bins[index].r;
                if (bins[index].g > max_component) {
                    max_component = bins[index].g;
                }
                if (bins[index].b > max_component) {
                    max_component = bins[index].b;
                }
                min_component = bins[index].r;
                if (bins[index].g < min_component) {
                    min_component = bins[index].g;
                }
                if (bins[index].b < min_component) {
                    min_component = bins[index].b;
                }
                delta_component = max_component - min_component;
                hue_value = 0.0;
                if (delta_component > 1.0e-9) {
                    if (max_component == bins[index].r) {
                        hue_value = (bins[index].g - bins[index].b)
                            / delta_component;
                        if (hue_value < 0.0) {
                            hue_value += 6.0;
                        }
                    } else if (max_component == bins[index].g) {
                        hue_value = 2.0 + (bins[index].b - bins[index].r)
                            / delta_component;
                    } else {
                        hue_value = 4.0 + (bins[index].r - bins[index].g)
                            / delta_component;
                    }
                }
                normalized_hue = hue_value / 6.0;
                if (normalized_hue < 0.0) {
                    normalized_hue = 0.0;
                }
                if (normalized_hue >= 1.0) {
                    normalized_hue = 0.999999;
                }
                dr = bins[index].r - mean_r;
                dg = bins[index].g - mean_g;
                db = bins[index].b - mean_b;
                probability = (double)bins[index].count
                    * (dr * dr + dg * dg + db * db);
                chroma_bucket = 0u;
            }
            hue_bucket = (unsigned int)(
                normalized_hue * (double)SIXEL_KCENTER_HUE_BUCKETS);
            if (hue_bucket >= SIXEL_KCENTER_HUE_BUCKETS) {
                hue_bucket = SIXEL_KCENTER_HUE_BUCKETS - 1u;
            }
            strata_index = (luma_bucket * SIXEL_KCENTER_CHROMA_BUCKETS
                + chroma_bucket) * SIXEL_KCENTER_HUE_BUCKETS
                + hue_bucket;
            if (probability
                    > strata_scores[strata_index] + 1.0e-12) {
                strata_scores[strata_index] = probability;
                strata_picks[strata_index] = index;
            } else if (probability
                    >= strata_scores[strata_index] - 1.0e-12
                    && strata_picks[strata_index] != UINT_MAX
                    && index < strata_picks[strata_index]) {
                strata_picks[strata_index] = index;
            }
        }
        for (strata_rank = 0u;
                strata_rank < SIXEL_KCENTER_STRATA_BUCKETS;
                ++strata_rank) {
            for (strata_swap = strata_rank + 1u;
                    strata_swap < SIXEL_KCENTER_STRATA_BUCKETS;
                    ++strata_swap) {
                if (strata_scores[strata_order[strata_swap]]
                        > strata_scores[strata_order[strata_rank]]
                        + 1.0e-12) {
                    strata_index = strata_order[strata_rank];
                    strata_order[strata_rank] = strata_order[strata_swap];
                    strata_order[strata_swap] = strata_index;
                }
            }
        }
        for (strata_rank = 0u;
                strata_rank < SIXEL_KCENTER_STRATA_BUCKETS
                && selected_count < budget;
                ++strata_rank) {
            strata_index = strata_order[strata_rank];
            bin_index = strata_picks[strata_index];
            if (bin_index == UINT_MAX) {
                continue;
            }
            if (bin_index >= retained_count || bin_selected[bin_index] != 0u) {
                continue;
            }
            bin_selected[bin_index] = 1u;
            ++selected_count;
        }

        high_target = budget;
        if (high_target > selected_count) {
            if (oklab_perceptual_space) {
                /*
                 * Reserve more slots for dispersion picks in OKLab so the
                 * candidate set does not over-focus on only high-mass bins.
                 */
                high_target = selected_count
                    + (budget - selected_count) / 2u;
            } else {
                high_target = selected_count
                    + ((budget - selected_count) * 3u) / 4u;
            }
            if (high_target > budget) {
                high_target = budget;
            }
        }
        for (index = 0u; index < retained_count && selected_count < high_target;
                ++index) {
            if (bin_selected[index] != 0u) {
                continue;
            }
            bin_selected[index] = 1u;
            ++selected_count;
        }
        for (index = 0u; index < retained_count && selected_count < budget;
                ++index) {
            bin_index = dispersion[index].index;
            if (bin_index >= retained_count || bin_selected[bin_index] != 0u) {
                continue;
            }
            bin_selected[bin_index] = 1u;
            ++selected_count;
        }
        for (index = 0u; index < retained_count && selected_count < budget;
                ++index) {
            if (bin_selected[index] != 0u) {
                continue;
            }
            bin_selected[index] = 1u;
            ++selected_count;
        }
        if (selected_count == 0u) {
            bin_selected[0u] = 1u;
            selected_count = 1u;
        }

        budget = selected_count;
        bin_index = 0u;
        for (index = 0u;
                index < retained_count && bin_index < budget;
                ++index) {
            if (bin_selected[index] == 0u) {
                continue;
            }
            points[bin_index * 3u + 0u] = bins[index].r;
            points[bin_index * 3u + 1u] = bins[index].g;
            points[bin_index * 3u + 2u] = bins[index].b;
            weights[bin_index] = (double)bins[index].count;
            ++bin_index;
        }
    } else {
        for (index = 0u; index < budget; ++index) {
            points[index * 3u + 0u] = bins[index].r;
            points[index * 3u + 1u] = bins[index].g;
            points[index * 3u + 2u] = bins[index].b;
            weights[index] = (double)bins[index].count;
        }
    }

    *points_out = points;
    *weights_out = weights;
    *point_count_out = budget;
    points = NULL;
    weights = NULL;
    status = SIXEL_OK;

end:
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    if (dispersion != NULL) {
        sixel_allocator_free(allocator, dispersion);
    }
    if (chroma_rank != NULL) {
        sixel_allocator_free(allocator, chroma_rank);
    }
    if (chroma_cache != NULL) {
        sixel_allocator_free(allocator, chroma_cache);
    }
    if (bin_selected != NULL) {
        sixel_allocator_free(allocator, bin_selected);
    }
    if (bins != NULL) {
        sixel_allocator_free(allocator, bins);
    }
    if (sums != NULL) {
        sixel_allocator_free(allocator, sums);
    }
    if (counts != NULL) {
        sixel_allocator_free(allocator, counts);
    }
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
    }

    *radius2_out = radius2;
    *sse_out = sse;
    return 1;
}

static int
sixel_kcenter_try_worst_swap(sixel_kcenter_swap_ctx_t *ctx)
{
    unsigned int point_count;
    unsigned int k;
    double const *points;
    double const *weights;
    unsigned int *centers;
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
    unsigned int candidate_list[16];
    unsigned int global_list[16];
    double global_dist[16];
    double global_weight[16];
    unsigned int cluster_list[16];
    double cluster_dist[16];
    double cluster_weight[16];
    unsigned int cluster_slot_list[16];
    unsigned int candidate_count;
    unsigned int global_count;
    unsigned int cluster_count;
    unsigned int cluster_slot_count;
    unsigned int topk;
    sixel_kcenter_swap_update_t swap_update;
    double swap_min_gain;
    int use_cluster_candidates;
    uint32_t *rng_state;
    unsigned int candidate;
    unsigned int old_slot;
    unsigned int index;
    unsigned int slot;
    unsigned int best_slot;
    unsigned int best_candidate;
    unsigned int old_center;
    unsigned int scan_slot;
    unsigned int tries;
    unsigned int pick;
    unsigned int cluster_slot;
    unsigned int cluster_best;
    unsigned int global_index;
    unsigned int cluster_index;
    unsigned int selected_source;
    unsigned int affected_count;
    double radius2;
    double sse;
    double best_radius2;
    double best_sse;
    double allowed_radius2;
    double distance_to_new;
    double new_distance;
    double old_distance;
    double old_second;
    unsigned int old_nearest_slot;
    unsigned int old_second_slot;
    unsigned int scan_best_slot;
    unsigned int scan_second_slot;
    double scan_best_dist;
    double scan_second_dist;
    double scan_distance;
    double lhs_weight;
    double best_cluster_dist;
    double best_cluster_weight;
    int added;
    int found;
    int rejected;

    if (ctx == NULL) {
        return 0;
    }

    point_count = ctx->point_count;
    k = ctx->k;
    points = ctx->points;
    weights = ctx->weights;
    centers = ctx->centers;
    center_mask = ctx->center_mask;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    second_slot = ctx->second_slot;
    second_dist = ctx->second_dist;
    radius2_io = ctx->radius2_io;
    sse_io = ctx->sse_io;
    scratch_slot = ctx->scratch_slot;
    scratch_dist = ctx->scratch_dist;
    scratch_second_slot = ctx->scratch_second_slot;
    scratch_second_dist = ctx->scratch_second_dist;
    topk = ctx->swap_topk;
    swap_update = ctx->swap_update;
    swap_min_gain = ctx->swap_min_gain;
    use_cluster_candidates = ctx->use_cluster_candidates;
    rng_state = ctx->rng_state;

    candidate_count = 0u;
    global_count = 0u;
    cluster_count = 0u;
    cluster_slot_count = 0u;
    candidate = 0u;
    old_slot = 0u;
    index = 0u;
    slot = 0u;
    best_slot = 0u;
    best_candidate = 0u;
    old_center = 0u;
    scan_slot = 0u;
    tries = 0u;
    pick = 0u;
    cluster_slot = 0u;
    cluster_best = UINT_MAX;
    global_index = 0u;
    cluster_index = 0u;
    selected_source = 0u;
    affected_count = 0u;
    radius2 = 0.0;
    sse = 0.0;
    best_radius2 = *radius2_io;
    best_sse = *sse_io;
    allowed_radius2 = 0.0;
    distance_to_new = 0.0;
    new_distance = 0.0;
    old_distance = 0.0;
    old_second = 0.0;
    old_nearest_slot = 0u;
    old_second_slot = 0u;
    scan_best_slot = 0u;
    scan_second_slot = 0u;
    scan_best_dist = 0.0;
    scan_second_dist = 0.0;
    scan_distance = 0.0;
    lhs_weight = 0.0;
    best_cluster_dist = 0.0;
    best_cluster_weight = 0.0;
    added = 0;
    found = 0;
    rejected = 0;

    if (point_count == 0u
            || k == 0u
            || point_count <= k
            || center_mask == NULL) {
        return 0;
    }
    if (topk < 1u) {
        topk = 1u;
    }
    if (topk > 16u) {
        topk = 16u;
    }
    if (swap_min_gain < 0.0) {
        swap_min_gain = 0.0;
    }
    if (!use_cluster_candidates) {
        cluster_slot_count = 0u;
        cluster_count = 0u;
    }
    allowed_radius2 = sixel_kcenter_swap_allowed_radius_sq(best_radius2,
                                                           swap_min_gain);

    for (index = 0u; index < point_count; ++index) {
        if (center_mask[index] != 0u) {
            continue;
        }
        lhs_weight = (weights != NULL) ? weights[index] : 1.0;
        sixel_kcenter_swap_insert_candidate(global_list,
                                            global_dist,
                                            global_weight,
                                            &global_count,
                                            topk,
                                            index,
                                            nearest_dist[index],
                                            lhs_weight);
    }
    if (use_cluster_candidates) {
        for (index = 0u; index < global_count; ++index) {
            cluster_slot = nearest_slot[global_list[index]];
            if (cluster_slot >= k) {
                continue;
            }
            if (sixel_kcenter_swap_candidate_present(cluster_slot_list,
                                                     cluster_slot_count,
                                                     cluster_slot)) {
                continue;
            }
            if (cluster_slot_count >= topk) {
                break;
            }
            cluster_slot_list[cluster_slot_count] = cluster_slot;
            ++cluster_slot_count;
        }
        for (index = 0u; index < cluster_slot_count; ++index) {
            cluster_slot = cluster_slot_list[index];
            cluster_best = UINT_MAX;
            best_cluster_dist = -1.0;
            best_cluster_weight = -1.0;
            for (pick = 0u; pick < point_count; ++pick) {
                if (center_mask[pick] != 0u
                        || nearest_slot[pick] != cluster_slot) {
                    continue;
                }
                lhs_weight = (weights != NULL) ? weights[pick] : 1.0;
                if (lhs_weight <= 0.0) {
                    lhs_weight = 1.0;
                }
                if (nearest_dist[pick] > best_cluster_dist + 1.0e-12) {
                    cluster_best = pick;
                    best_cluster_dist = nearest_dist[pick];
                    best_cluster_weight = lhs_weight;
                    continue;
                }
                if (nearest_dist[pick] >= best_cluster_dist - 1.0e-12
                        && (lhs_weight > best_cluster_weight + 1.0e-12
                            || (lhs_weight >= best_cluster_weight - 1.0e-12
                                && pick < cluster_best))) {
                    cluster_best = pick;
                    best_cluster_dist = nearest_dist[pick];
                    best_cluster_weight = lhs_weight;
                }
            }
            if (cluster_best == UINT_MAX) {
                continue;
            }
            sixel_kcenter_swap_insert_candidate(cluster_list,
                                                cluster_dist,
                                                cluster_weight,
                                                &cluster_count,
                                                topk,
                                                cluster_best,
                                                best_cluster_dist,
                                                best_cluster_weight);
        }

        global_index = 0u;
        cluster_index = 0u;
        selected_source = 0u;
        while (candidate_count < topk
                && (global_index < global_count
                    || cluster_index < cluster_count)) {
            if (selected_source == 0u && global_index < global_count) {
                candidate = global_list[global_index];
                ++global_index;
                selected_source = 1u;
            } else if (selected_source == 1u
                    && cluster_index < cluster_count) {
                candidate = cluster_list[cluster_index];
                ++cluster_index;
                selected_source = 0u;
            } else if (global_index < global_count) {
                candidate = global_list[global_index];
                ++global_index;
            } else {
                candidate = cluster_list[cluster_index];
                ++cluster_index;
            }
            if (sixel_kcenter_swap_candidate_present(candidate_list,
                                                     candidate_count,
                                                     candidate)) {
                continue;
            }
            candidate_list[candidate_count] = candidate;
            ++candidate_count;
        }
        while (candidate_count < topk && global_index < global_count) {
            candidate = global_list[global_index];
            if (!sixel_kcenter_swap_candidate_present(candidate_list,
                                                      candidate_count,
                                                      candidate)) {
                candidate_list[candidate_count] = candidate;
                ++candidate_count;
            }
            ++global_index;
        }
        while (candidate_count < topk && cluster_index < cluster_count) {
            candidate = cluster_list[cluster_index];
            if (!sixel_kcenter_swap_candidate_present(candidate_list,
                                                      candidate_count,
                                                      candidate)) {
                candidate_list[candidate_count] = candidate;
                ++candidate_count;
            }
            ++cluster_index;
        }
    } else {
        for (index = 0u; index < global_count; ++index) {
            candidate_list[candidate_count] = global_list[index];
            ++candidate_count;
        }
    }

    while (candidate_count < topk && rng_state != NULL && point_count > 0u) {
        tries = 0u;
        pick = 0u;
        added = 0;
        while (tries < point_count * 2u + 8u) {
            pick = sixel_kcenter_rng_bounded(rng_state, point_count);
            if (center_mask[pick] != 0u) {
                ++tries;
                continue;
            }
            if (!sixel_kcenter_swap_candidate_present(candidate_list,
                                                      candidate_count,
                                                      pick)) {
                candidate_list[candidate_count] = pick;
                ++candidate_count;
                added = 1;
                break;
            }
            ++tries;
        }
        if (!added) {
            break;
        }
    }

    if (candidate_count == 0u) {
        return 0;
    }

    found = 0;
    if (swap_update == SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL
            && second_slot != NULL
            && second_dist != NULL
            && scratch_second_slot != NULL
            && scratch_second_dist != NULL) {
        for (index = 0u; index < candidate_count; ++index) {
            candidate = candidate_list[index];
            slot = nearest_slot[candidate];
            if (slot >= k || centers[slot] == candidate) {
                continue;
            }
            if (center_mask[candidate] != 0u) {
                continue;
            }

            radius2 = 0.0;
            sse = 0.0;
            rejected = 0;
            affected_count = 0u;
            for (old_slot = 0u; old_slot < point_count; ++old_slot) {
                old_nearest_slot = nearest_slot[old_slot];
                if (old_nearest_slot == slot) {
                    scratch_slot[old_slot] = UINT_MAX;
                    ++affected_count;
                    continue;
                }
                old_second_slot = second_slot[old_slot];
                old_distance = nearest_dist[old_slot];
                old_second = second_dist[old_slot];
                distance_to_new = sixel_kcenter_distance_sq(points,
                                                            old_slot,
                                                            candidate);
                if (distance_to_new < old_distance) {
                    new_distance = distance_to_new;
                    scratch_slot[old_slot] = slot;
                    scratch_dist[old_slot] = distance_to_new;
                    scratch_second_slot[old_slot] = old_nearest_slot;
                    scratch_second_dist[old_slot] = old_distance;
                } else {
                    new_distance = old_distance;
                    scratch_slot[old_slot] = old_nearest_slot;
                    scratch_dist[old_slot] = old_distance;
                    if (distance_to_new < old_second) {
                        scratch_second_slot[old_slot] = slot;
                        scratch_second_dist[old_slot] = distance_to_new;
                    } else {
                        scratch_second_slot[old_slot] = old_second_slot;
                        scratch_second_dist[old_slot] = old_second;
                    }
                }
                if (new_distance > radius2) {
                    radius2 = new_distance;
                    if (radius2 > allowed_radius2 + 1.0e-12) {
                        rejected = 1;
                        break;
                    }
                }
                lhs_weight = (weights != NULL) ? weights[old_slot] : 1.0;
                if (lhs_weight <= 0.0) {
                    lhs_weight = 1.0;
                }
                sse += new_distance * lhs_weight;
            }
            if (rejected) {
                continue;
            }

            if (affected_count > 0u) {
                for (old_slot = 0u; old_slot < point_count; ++old_slot) {
                    if (scratch_slot[old_slot] != UINT_MAX) {
                        continue;
                    }
                    scan_best_slot = 0u;
                    if (slot == 0u) {
                        scan_best_dist = sixel_kcenter_distance_sq(points,
                                                                   old_slot,
                                                                   candidate);
                    } else {
                        scan_best_dist = sixel_kcenter_distance_sq(
                            points,
                            old_slot,
                            centers[0u]);
                    }
                    scan_second_slot = 0u;
                    scan_second_dist = DBL_MAX;
                    for (scan_slot = 1u; scan_slot < k; ++scan_slot) {
                        if (scan_slot == slot) {
                            scan_distance = sixel_kcenter_distance_sq(
                                points,
                                old_slot,
                                candidate);
                        } else {
                            scan_distance = sixel_kcenter_distance_sq(
                                points,
                                old_slot,
                                centers[scan_slot]);
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
                    if (k == 1u) {
                        scan_second_slot = scan_best_slot;
                        scan_second_dist = scan_best_dist;
                    }
                    scratch_slot[old_slot] = scan_best_slot;
                    scratch_dist[old_slot] = scan_best_dist;
                    scratch_second_slot[old_slot] = scan_second_slot;
                    scratch_second_dist[old_slot] = scan_second_dist;
                    if (scan_best_dist > radius2) {
                        radius2 = scan_best_dist;
                        if (radius2 > allowed_radius2 + 1.0e-12) {
                            rejected = 1;
                            break;
                        }
                    }
                    lhs_weight = (weights != NULL) ? weights[old_slot] : 1.0;
                    if (lhs_weight <= 0.0) {
                        lhs_weight = 1.0;
                    }
                    sse += scan_best_dist * lhs_weight;
                }
            }
            if (rejected) {
                continue;
            }

            if (sixel_kcenter_swap_candidate_is_better(radius2,
                                                       sse,
                                                       best_radius2,
                                                       best_sse,
                                                       allowed_radius2)) {
                best_radius2 = radius2;
                best_sse = sse;
                best_slot = slot;
                best_candidate = candidate;
                allowed_radius2 = sixel_kcenter_swap_allowed_radius_sq(
                    best_radius2,
                    swap_min_gain);
                found = 1;
            }
        }

        if (found) {
            old_center = centers[best_slot];
            centers[best_slot] = best_candidate;
            center_mask[old_center] = 0u;
            center_mask[best_candidate] = 1u;
            radius2 = 0.0;
            sse = 0.0;
            for (old_slot = 0u; old_slot < point_count; ++old_slot) {
                old_nearest_slot = nearest_slot[old_slot];
                if (old_nearest_slot == best_slot) {
                    scan_best_slot = 0u;
                    scan_second_slot = 0u;
                    scan_best_dist = sixel_kcenter_distance_sq(points,
                                                               old_slot,
                                                               centers[0u]);
                    scan_second_dist = DBL_MAX;
                    for (scan_slot = 1u; scan_slot < k; ++scan_slot) {
                        scan_distance = sixel_kcenter_distance_sq(
                            points,
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
                } else if (second_slot[old_slot] == best_slot) {
                    old_distance = nearest_dist[old_slot];
                    distance_to_new = sixel_kcenter_distance_sq(points,
                                                                old_slot,
                                                                best_candidate);
                    if (distance_to_new < old_distance) {
                        scratch_slot[old_slot] = best_slot;
                        scratch_dist[old_slot] = distance_to_new;
                        scratch_second_slot[old_slot] = old_nearest_slot;
                        scratch_second_dist[old_slot] = old_distance;
                    } else {
                        scratch_slot[old_slot] = old_nearest_slot;
                        scratch_dist[old_slot] = old_distance;
                        if (k == 1u) {
                            scratch_second_slot[old_slot]
                                = scratch_slot[old_slot];
                            scratch_second_dist[old_slot]
                                = scratch_dist[old_slot];
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
                    distance_to_new = sixel_kcenter_distance_sq(points,
                                                                old_slot,
                                                                best_candidate);
                    if (distance_to_new < old_distance) {
                        scratch_slot[old_slot] = best_slot;
                        scratch_dist[old_slot] = distance_to_new;
                        scratch_second_slot[old_slot] = old_nearest_slot;
                        scratch_second_dist[old_slot] = old_distance;
                    } else {
                        scratch_slot[old_slot] = old_nearest_slot;
                        scratch_dist[old_slot] = old_distance;
                        if (distance_to_new < old_second) {
                            scratch_second_slot[old_slot] = best_slot;
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
            *radius2_io = radius2;
            *sse_io = sse;
            return 1;
        }
        return 0;
    }

    found = 0;
    for (index = 0u; index < candidate_count; ++index) {
        candidate = candidate_list[index];
        slot = nearest_slot[candidate];
        if (slot >= k || centers[slot] == candidate) {
            continue;
        }
        if (center_mask[candidate] != 0u) {
            continue;
        }

        if (nearest_slot != NULL
                && nearest_dist != NULL
                && second_dist != NULL) {
            if (!sixel_kcenter_swap_eval_with_cutoff(points,
                                                     weights,
                                                     point_count,
                                                     slot,
                                                     candidate,
                                                     nearest_slot,
                                                     nearest_dist,
                                                     second_dist,
                                                     allowed_radius2,
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
                                                         allowed_radius2,
                                                         &radius2,
                                                         &sse)) {
                centers[slot] = old_center;
                continue;
            }
            centers[slot] = old_center;
        }

        if (sixel_kcenter_swap_candidate_is_better(radius2,
                                                   sse,
                                                   best_radius2,
                                                   best_sse,
                                                   allowed_radius2)) {
            best_radius2 = radius2;
            best_sse = sse;
            best_slot = slot;
            best_candidate = candidate;
            allowed_radius2 = sixel_kcenter_swap_allowed_radius_sq(
                best_radius2,
                swap_min_gain);
            found = 1;
        }
    }

    if (!found) {
        return 0;
    }

    old_center = centers[best_slot];
    centers[best_slot] = best_candidate;
    center_mask[old_center] = 0u;
    center_mask[best_candidate] = 1u;
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
                                                &radius2,
                                                &sse,
                                                NULL,
                                                NULL);
    } else {
        sixel_kcenter_assign_points(points,
                                    weights,
                                    point_count,
                                    centers,
                                    k,
                                    nearest_slot,
                                    nearest_dist,
                                    &radius2,
                                    &sse,
                                    NULL,
                                    NULL);
    }
    *radius2_io = radius2;
    *sse_io = sse;
    if (scratch_second_slot != NULL && scratch_second_dist != NULL) {
        /*
         * Keep buffers initialized for debugability in constrained builds.
         * The next incremental pass will overwrite them anyway.
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

static SIXELSTATUS
sixel_kcenter_run_solver(sixel_kcenter_solver_ctx_t *ctx)
{
    SIXELSTATUS status;
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
    unsigned int init_trial;
    unsigned int iterations;
    unsigned int trial_iterations;
    unsigned int no_improve;
    unsigned int effective_topk;
    unsigned int patience_limit;
    double radius2;
    double sse;
    double gain2;
    double tiny_gain2;
    double best_trial_radius2;
    double best_trial_sse;
    unsigned int best_trial_iterations;
    uint32_t base_state;
    uint32_t trial_state;
    int adaptive_swap_controls;
    sixel_kcenter_swap_ctx_t swap_ctx;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    points = ctx->points;
    weights = ctx->weights;
    point_count = ctx->point_count;
    k = ctx->k;
    resolved_algo = ctx->resolved_algo;
    profile = ctx->profile;
    init_seeds = ctx->init_seeds;
    iter_limit = ctx->iter_limit;
    rng_state = ctx->rng_state;
    centers = ctx->centers;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    second_slot = ctx->second_slot;
    second_dist = ctx->second_dist;
    scratch_slot = ctx->scratch_slot;
    scratch_dist = ctx->scratch_dist;
    scratch_second_slot = ctx->scratch_second_slot;
    scratch_second_dist = ctx->scratch_second_dist;
    swap_topk = ctx->swap_topk;
    swap_update = ctx->swap_update;
    swap_patience = ctx->swap_patience;
    swap_min_gain = ctx->swap_min_gain;
    use_cluster_candidates = ctx->use_cluster_candidates;
    radius2_out = ctx->radius2_out;
    sse_out = ctx->sse_out;
    iterations_out = ctx->iterations_out;
    scratch_indices = ctx->scratch_indices;
    fft_dist_cache = ctx->fft_dist_cache;
    center_mask = ctx->center_mask;

    status = SIXEL_OK;
    init_trial = 0u;
    iterations = 0u;
    trial_iterations = 0u;
    no_improve = 0u;
    effective_topk = 0u;
    patience_limit = 0u;
    radius2 = 0.0;
    sse = 0.0;
    gain2 = 0.0;
    tiny_gain2 = 0.0;
    best_trial_radius2 = -1.0;
    best_trial_sse = 0.0;
    best_trial_iterations = 0u;
    base_state = 1u;
    trial_state = 1u;
    adaptive_swap_controls = 0;

    if (points == NULL || centers == NULL
            || nearest_slot == NULL || nearest_dist == NULL
            || scratch_slot == NULL || scratch_dist == NULL
            || scratch_indices == NULL
            || fft_dist_cache == NULL || center_mask == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(&swap_ctx, 0, sizeof(swap_ctx));
    swap_ctx.points = points;
    swap_ctx.weights = weights;
    swap_ctx.point_count = point_count;
    swap_ctx.centers = centers;
    swap_ctx.k = k;
    swap_ctx.center_mask = center_mask;
    swap_ctx.nearest_slot = nearest_slot;
    swap_ctx.nearest_dist = nearest_dist;
    swap_ctx.second_slot = second_slot;
    swap_ctx.second_dist = second_dist;
    swap_ctx.radius2_io = &radius2;
    swap_ctx.sse_io = &sse;
    swap_ctx.scratch_slot = scratch_slot;
    swap_ctx.scratch_dist = scratch_dist;
    swap_ctx.scratch_second_slot = scratch_second_slot;
    swap_ctx.scratch_second_dist = scratch_second_dist;
    swap_ctx.swap_topk = swap_topk;
    swap_ctx.swap_update = swap_update;
    swap_ctx.swap_min_gain = swap_min_gain;
    swap_ctx.use_cluster_candidates = use_cluster_candidates;
    swap_ctx.rng_state = &trial_state;

    if (point_count == 0u || k == 0u) {
        if (radius2_out != NULL) {
            *radius2_out = 0.0;
        }
        if (sse_out != NULL) {
            *sse_out = 0.0;
        }
        if (iterations_out != NULL) {
            *iterations_out = 0u;
        }
        return SIXEL_OK;
    }

    if (init_seeds < 1u) {
        init_seeds = 1u;
    }
    if (init_seeds > 8u) {
        init_seeds = 8u;
    }
    if (swap_topk < 1u) {
        swap_topk = 1u;
    }
    if (swap_topk > 16u) {
        swap_topk = 16u;
    }
    if (swap_patience > 8u) {
        swap_patience = 8u;
    }
    if (swap_min_gain < 0.0) {
        swap_min_gain = 0.0;
    }
    if (swap_min_gain > 0.0) {
        tiny_gain2 = swap_min_gain * swap_min_gain * 0.25;
    } else {
        tiny_gain2 = 1.0e-6;
    }
    adaptive_swap_controls = (profile
                              != SIXEL_PALETTE_KCENTER_PROFILE_LEGACY);
    if (rng_state != NULL && *rng_state != 0u) {
        base_state = *rng_state;
    }
    swap_ctx.swap_min_gain = swap_min_gain;

    for (init_trial = 0u; init_trial < init_seeds; ++init_trial) {
        trial_state = base_state + 0x9e3779b9u * (init_trial + 1u);
        if (trial_state == 0u) {
            trial_state = 1u;
        }

        if (resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_SWAP) {
            sixel_kcenter_choose_random(point_count,
                                        k,
                                        &trial_state,
                                        centers,
                                        scratch_slot);
        } else {
            sixel_kcenter_choose_fft(points,
                                     weights,
                                     point_count,
                                     k,
                                     &trial_state,
                                     centers,
                                     fft_dist_cache,
                                     center_mask);
        }
        sixel_kcenter_refresh_center_mask(center_mask,
                                          point_count,
                                          centers,
                                          k);

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
                                                    &radius2,
                                                    &sse,
                                                    NULL,
                                                    NULL);
        } else {
            sixel_kcenter_assign_points(points,
                                        weights,
                                        point_count,
                                        centers,
                                        k,
                                        nearest_slot,
                                        nearest_dist,
                                        &radius2,
                                        &sse,
                                        NULL,
                                        NULL);
        }

        trial_iterations = 0u;
        no_improve = 0u;
        effective_topk = swap_topk;
        if (profile == SIXEL_PALETTE_KCENTER_PROFILE_SPEED
                && effective_topk > 2u) {
            effective_topk = 2u;
        }
        patience_limit = swap_patience;
        if (adaptive_swap_controls && patience_limit == 0u) {
            patience_limit = sixel_kcenter_profile_default_swap_patience(
                profile);
        }

        if (resolved_algo != SIXEL_PALETTE_KCENTER_ALGO_FFT) {
            while (trial_iterations < iter_limit) {
                swap_ctx.swap_topk = effective_topk;
                gain2 = radius2;
                if (sixel_kcenter_try_worst_swap(&swap_ctx)) {
                    gain2 -= radius2;
                    ++trial_iterations;
                    if (adaptive_swap_controls
                            && gain2 <= tiny_gain2 + 1.0e-12) {
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
                    } else if (adaptive_swap_controls
                            && effective_topk > 1u) {
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
        }

        if (best_trial_radius2 < 0.0
                || radius2 < best_trial_radius2 - 1.0e-12
                || (radius2 <= best_trial_radius2 + 1.0e-12
                    && sse < best_trial_sse - 1.0e-9)) {
            memcpy(scratch_indices,
                   centers,
                   (size_t)k * sizeof(unsigned int));
            best_trial_radius2 = radius2;
            best_trial_sse = sse;
            best_trial_iterations = trial_iterations;
        }
    }

    memcpy(centers, scratch_indices, (size_t)k * sizeof(unsigned int));
    sixel_kcenter_refresh_center_mask(center_mask,
                                      point_count,
                                      centers,
                                      k);
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
                                                &radius2,
                                                &sse,
                                                NULL,
                                                NULL);
    } else {
        sixel_kcenter_assign_points(points,
                                    weights,
                                    point_count,
                                    centers,
                                    k,
                                    nearest_slot,
                                    nearest_dist,
                                    &radius2,
                                    &sse,
                                    NULL,
                                    NULL);
    }
    iterations = best_trial_iterations;

    if (radius2_out != NULL) {
        *radius2_out = radius2;
    }
    if (sse_out != NULL) {
        *sse_out = sse;
    }
    if (iterations_out != NULL) {
        *iterations_out = iterations;
    }
    if (rng_state != NULL) {
        *rng_state = trial_state;
    }
    return status;
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
        if (quality_mode == SIXEL_QUALITY_LOW || point_count > 2048u) {
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

static SIXELSTATUS
build_palette_kcenter(sixel_kcenter_build_ctx_t *ctx)
{
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
    sixel_logger_t *logger;
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
    unsigned int slot;
    unsigned int index;
    unsigned int channel;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
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
    double weight_value;
    double component;
    double restored_component;
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
    float float_minimum;
    float float_maximum;
    double range;
    double now;
    double init_span;
    double iterate_span;
    double merge_span;
    double export_span;
    int override_lock_acquired;
    sixel_kcenter_solver_ctx_t solver_ctx;
    sixel_kcenter_polish_ctx_t polish_ctx;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result = ctx->result;
    result_float32 = ctx->result_float32;
    data = ctx->data;
    length = ctx->length;
    depth = ctx->depth;
    reqcolors = ctx->reqcolors;
    ncolors = ctx->ncolors;
    origcolors = ctx->origcolors;
    quality_mode = ctx->quality_mode;
    force_palette = ctx->force_palette;
    use_reversible = ctx->use_reversible;
    final_merge_mode = ctx->final_merge_mode;
    allocator = ctx->allocator;
    pixelformat = ctx->pixelformat;
    treat_input_as_float32 = ctx->treat_input_as_float32;
    logger = ctx->logger;
    job_seq = ctx->job_seq;
    engine_name = ctx->engine_name;
    telemetry = ctx->telemetry;

    status = SIXEL_BAD_ARGUMENT;
    points = NULL;
    weights = NULL;
    point_count = 0u;
    visible_count = 0u;
    active_count = 0u;
    k = 0u;
    overshoot = 0u;
    slot = 0u;
    index = 0u;
    channel = 0u;
    fill = 0u;
    source = 0u;
    swap_temp = 0u;
    seed = 1u;
    restarts = 1u;
    init_seeds = SIXEL_KCENTER_INIT_SEEDS_DEFAULT;
    iter_limit = 16u;
    histbits = 5u;
    point_budget = 0u;
    auto_fft_threshold = SIXEL_KCENTER_AUTO_FFT_THRESHOLD_DEFAULT;
    rare_keep = SIXEL_KCENTER_RARE_KEEP_DEFAULT;
    swap_topk = SIXEL_KCENTER_SWAP_TOPK_DEFAULT;
    swap_patience = SIXEL_KCENTER_SWAP_PATIENCE_DEFAULT;
    swap_min_gain = SIXEL_KCENTER_SWAP_MIN_GAIN_DEFAULT;
    use_cluster_candidates = 0;
    use_perceptual_polish = 0;
    total_iterations = 0u;
    best_iterations = 0u;
    run_iterations = 0u;
    polish_updates = 0u;
    resolved_merge = SIXEL_FINAL_MERGE_NONE;
    apply_merge = 0;
    algo = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    resolved_algo = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    profile = SIXEL_PALETTE_KCENTER_PROFILE_LEGACY;
    auto_policy = SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY;
    space_policy = SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY;
    candidate_policy = SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY;
    budget_policy = SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY;
    swap_update = SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL;
    rng_state = 1u;
    centers = NULL;
    best_centers = NULL;
    work_centers = NULL;
    nearest_slot = NULL;
    second_slot = NULL;
    scratch_slot = NULL;
    scratch_second_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    scratch_dist = NULL;
    scratch_second_dist = NULL;
    cluster_weights = NULL;
    cluster_sums = NULL;
    final_centers = NULL;
    sort_weights = NULL;
    order = NULL;
    scratch_indices = NULL;
    fft_dist_cache = NULL;
    center_mask = NULL;
    radius2 = 0.0;
    sse = 0.0;
    best_radius2 = 0.0;
    best_sse = 0.0;
    polish_pre_radius2 = 0.0;
    prune_mass = 0.995;
    budget_scale = SIXEL_KCENTER_BUDGET_SCALE_DEFAULT;
    merge_weights = NULL;
    merge_sums = NULL;
    cluster_total = 0;
    final_count = 0u;
    weight_value = 0.0;
    component = 0.0;
    restored_component = 0.0;
    palette = NULL;
    grown_palette = NULL;
    float_palette = NULL;
    grown_float = NULL;
    input_is_float32 = 0;
    float32_channel_scale[0] = 0.0;
    float32_channel_scale[1] = 0.0;
    float32_channel_scale[2] = 0.0;
    float32_channel_offset[0] = 0.0;
    float32_channel_offset[1] = 0.0;
    float32_channel_offset[2] = 0.0;
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
    float_minimum = 0.0f;
    float_maximum = 0.0f;
    range = 0.0;
    now = 0.0;
    init_span = 0.0;
    iterate_span = 0.0;
    merge_span = 0.0;
    export_span = 0.0;
    override_lock_acquired = 0;
    memset(&solver_ctx, 0, sizeof(solver_ctx));
    memset(&polish_ctx, 0, sizeof(polish_ctx));

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
#if HAVE_FLOAT_H
# define SIXEL_KCENTER_FLOAT_BOUND FLT_MAX
#else
# define SIXEL_KCENTER_FLOAT_BOUND 1.0e9f
#endif
            float_minimum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                -SIXEL_KCENTER_FLOAT_BOUND);
            float_maximum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                SIXEL_KCENTER_FLOAT_BOUND);
#undef SIXEL_KCENTER_FLOAT_BOUND
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
    }

    if (reqcolors == 0u) {
        reqcolors = 1u;
    }

    sixel_kcenter_last_polish_applied = 0;
    sixel_kcenter_last_polish_updates = 0u;
    sixel_kcenter_last_polish_pre_radius2 = 0.0;
    sixel_kcenter_last_polish_post_radius2 = 0.0;

    override_lock_acquired = sixel_kcenter_override_lock_acquire();

    job_init = sixel_palette_kcenter_log_start(logger,
                                               job_seq,
                                               engine_name,
                                               "palette/init",
                                               "init");

    algo = sixel_get_kcenter_algo();
    profile = sixel_get_kcenter_profile();
    seed = (unsigned int)sixel_get_kcenter_seed();
    restarts = sixel_get_kcenter_restarts();
    init_seeds = sixel_get_kcenter_init_seeds();
    iter_limit = sixel_get_kcenter_iter();
    histbits = sixel_get_kcenter_histbits();
    point_budget = sixel_get_kcenter_point_budget();
    prune_mass = sixel_get_kcenter_prune_mass();
    auto_policy = sixel_get_kcenter_auto_policy();
    auto_fft_threshold = sixel_get_kcenter_auto_fft_threshold();
    space_policy = sixel_get_kcenter_space_policy();
    candidate_policy = sixel_get_kcenter_candidate_policy();
    rare_keep = sixel_get_kcenter_rare_keep();
    budget_policy = sixel_get_kcenter_budget_policy();
    budget_scale = sixel_get_kcenter_budget_scale();
    swap_topk = sixel_get_kcenter_swap_topk();
    swap_update = sixel_get_kcenter_swap_update();
    swap_patience = sixel_get_kcenter_swap_patience();
    swap_min_gain = sixel_get_kcenter_swap_min_gain();
    sixel_kcenter_apply_perceptual_solver_bias(profile,
                                               space_policy,
                                               &init_seeds,
                                               &swap_topk);
    use_cluster_candidates = (profile != SIXEL_PALETTE_KCENTER_PROFILE_LEGACY
                              && swap_topk > 1u);
    use_perceptual_polish = (profile != SIXEL_PALETTE_KCENTER_PROFILE_LEGACY
                             && sixel_kcenter_resolve_space_policy(
                                 space_policy)
                             == SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL
                             && (pixelformat
                                 == SIXEL_PIXELFORMAT_OKLABFLOAT32
                                 || pixelformat
                                 == SIXEL_PIXELFORMAT_CIELABFLOAT32
                                 || pixelformat
                                 == SIXEL_PIXELFORMAT_DIN99DFLOAT32));

    status = sixel_kcenter_collect_points(&points,
                                          &weights,
                                          &point_count,
                                          &visible_count,
                                          &active_count,
                                          data,
                                          length,
                                          depth,
                                          pixelformat,
                                          treat_input_as_float32,
                                          histbits,
                                          point_budget,
                                          prune_mass,
                                          reqcolors,
                                          quality_mode,
                                          candidate_policy,
                                          space_policy,
                                          rare_keep,
                                          budget_policy,
                                          budget_scale,
                                          float32_channel_scale,
                                          float32_channel_offset,
                                          allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (origcolors != NULL) {
        *origcolors = visible_count;
    }
    if (point_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    resolved_merge = (unsigned int)sixel_resolve_final_merge_mode(
        final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    overshoot = reqcolors;
    if (apply_merge) {
        sixel_final_merge_load_env();
        overshoot = sixel_final_merge_target(reqcolors, (int)resolved_merge);
    }
    if (overshoot > point_count) {
        overshoot = point_count;
    }
    if (overshoot == 0u) {
        overshoot = 1u;
    }
    k = overshoot;

    centers = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    best_centers = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    work_centers = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    second_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    scratch_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    scratch_second_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    scratch_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    scratch_second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    cluster_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    cluster_sums = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    final_centers = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    scratch_indices = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    fft_dist_cache = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    center_mask = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count);

    if (centers == NULL || best_centers == NULL || work_centers == NULL
            || nearest_slot == NULL || second_slot == NULL
            || scratch_slot == NULL || scratch_second_slot == NULL
            || nearest_dist == NULL || second_dist == NULL
            || scratch_dist == NULL || scratch_second_dist == NULL
            || cluster_weights == NULL || cluster_sums == NULL
            || final_centers == NULL || scratch_indices == NULL
            || fft_dist_cache == NULL || center_mask == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    resolved_algo = algo;
    if (resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_AUTO) {
        resolved_algo = sixel_kcenter_choose_auto_algo(quality_mode,
                                                       point_count,
                                                       auto_policy,
                                                       auto_fft_threshold,
                                                       pixelformat,
                                                       space_policy);
    }

    init_stop = sixel_timer_now();
    iterate_start = init_stop;
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "samples=%u active=%u k=%u profile=%s "
                                "algo=%s/%s auto=%s fft_threshold=%u "
                                "space=%s cand=%s rare_keep=%u "
                                "budget=%s scale=%.2f req_budget=%u "
                                "prune_mass=%.3f swap_topk=%u "
                                "swap_update=%s swap_patience=%u "
                                "swap_min_gain=%.3f seed=%u "
                                "init_seeds=%u histbits=%u",
                                point_count,
                                active_count,
                                k,
                                sixel_kcenter_profile_to_string(profile),
                                sixel_kcenter_algo_to_string(algo),
                                sixel_kcenter_algo_to_string(resolved_algo),
                                sixel_kcenter_auto_policy_to_string(
                                    auto_policy),
                                auto_fft_threshold,
                                sixel_kcenter_space_policy_to_string(
                                    space_policy),
                                sixel_kcenter_candidate_policy_to_string(
                                    candidate_policy),
                                rare_keep,
                                sixel_kcenter_budget_policy_to_string(
                                    budget_policy),
                                budget_scale,
                                point_budget,
                                prune_mass,
                                swap_topk,
                                sixel_kcenter_swap_update_to_string(
                                    swap_update),
                                swap_patience,
                                swap_min_gain,
                                seed,
                                init_seeds,
                                histbits);
    sixel_palette_kcenter_log_finish(logger,
                                     job_init,
                                     engine_name,
                                     "palette/init",
                                     "init",
                                     log_detail);

    job_iter = sixel_palette_kcenter_log_start(logger,
                                               job_seq,
                                               engine_name,
                                               "palette/iterate",
                                               "iterate");

    solver_ctx.points = points;
    solver_ctx.weights = weights;
    solver_ctx.point_count = point_count;
    solver_ctx.k = k;
    solver_ctx.resolved_algo = resolved_algo;
    solver_ctx.profile = profile;
    solver_ctx.init_seeds = init_seeds;
    solver_ctx.iter_limit = iter_limit;
    solver_ctx.rng_state = &rng_state;
    solver_ctx.centers = work_centers;
    solver_ctx.nearest_slot = nearest_slot;
    solver_ctx.nearest_dist = nearest_dist;
    solver_ctx.second_slot = second_slot;
    solver_ctx.second_dist = second_dist;
    solver_ctx.scratch_slot = scratch_slot;
    solver_ctx.scratch_dist = scratch_dist;
    solver_ctx.scratch_second_slot = scratch_second_slot;
    solver_ctx.scratch_second_dist = scratch_second_dist;
    solver_ctx.swap_topk = swap_topk;
    solver_ctx.swap_update = swap_update;
    solver_ctx.swap_patience = swap_patience;
    solver_ctx.swap_min_gain = swap_min_gain;
    solver_ctx.use_cluster_candidates = use_cluster_candidates;
    solver_ctx.radius2_out = &radius2;
    solver_ctx.sse_out = &sse;
    solver_ctx.iterations_out = &run_iterations;
    solver_ctx.scratch_indices = scratch_indices;
    solver_ctx.fft_dist_cache = fft_dist_cache;
    solver_ctx.center_mask = center_mask;

    best_radius2 = -1.0;
    best_sse = 0.0;
    best_iterations = 0u;
    for (slot = 0u; slot < restarts; ++slot) {
        rng_state = (uint32_t)(seed + 0x9e3779b9u * (slot + 1u));
        status = sixel_kcenter_run_solver(&solver_ctx);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (best_radius2 < 0.0
                || radius2 < best_radius2 - 1.0e-12
                || (radius2 <= best_radius2 + 1.0e-12
                    && sse < best_sse - 1.0e-9)) {
            memcpy(best_centers,
                   work_centers,
                   (size_t)k * sizeof(unsigned int));
            best_radius2 = radius2;
            best_sse = sse;
            best_iterations = run_iterations;
        }
    }
    memcpy(centers, best_centers, (size_t)k * sizeof(unsigned int));
    total_iterations = best_iterations;

    sixel_kcenter_assign_points_with_second(points,
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
                                            cluster_weights,
                                            cluster_sums);
    polish_ctx.points = points;
    polish_ctx.weights = weights;
    polish_ctx.point_count = point_count;
    polish_ctx.centers = centers;
    polish_ctx.k = k;
    polish_ctx.nearest_slot = nearest_slot;
    polish_ctx.nearest_dist = nearest_dist;
    polish_ctx.second_slot = second_slot;
    polish_ctx.second_dist = second_dist;
    polish_ctx.scratch_slot = scratch_slot;
    polish_ctx.scratch_dist = scratch_dist;
    polish_ctx.scratch_second_slot = scratch_second_slot;
    polish_ctx.scratch_second_dist = scratch_second_dist;
    polish_ctx.center_mask = center_mask;
    polish_ctx.cluster_weights = cluster_weights;
    polish_ctx.cluster_sums = cluster_sums;
    polish_ctx.radius2_io = &radius2;
    polish_ctx.sse_io = &sse;
    if (use_perceptual_polish
            && resolved_algo != SIXEL_PALETTE_KCENTER_ALGO_FFT
            && point_count <= 8192u
            && k <= 128u) {
        polish_pre_radius2 = radius2;
        sixel_kcenter_polish_sse_with_radius_guard(
            &polish_ctx,
            &polish_updates);
        sixel_kcenter_last_polish_applied = 1;
        sixel_kcenter_last_polish_updates = polish_updates;
        sixel_kcenter_last_polish_pre_radius2 = polish_pre_radius2;
        sixel_kcenter_last_polish_post_radius2 = radius2;
    }
    if (best_radius2 < 0.0) {
        best_radius2 = radius2;
        best_sse = sse;
    } else if (radius2 < best_radius2 - 1.0e-12
            || (radius2 <= best_radius2 + 1.0e-12
                && sse < best_sse - 1.0e-9)) {
        best_radius2 = radius2;
        best_sse = sse;
    }

    for (slot = 0u; slot < k; ++slot) {
        final_centers[slot * 3u + 0u] = points[centers[slot] * 3u + 0u];
        final_centers[slot * 3u + 1u] = points[centers[slot] * 3u + 1u];
        final_centers[slot * 3u + 2u] = points[centers[slot] * 3u + 2u];
    }
    final_count = k;

    iterate_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "algo=%s iter=%u radius=%.4f sse=%.4f "
                                "polish_updates=%u",
                                sixel_kcenter_algo_to_string(resolved_algo),
                                total_iterations,
                                sqrt(best_radius2),
                                best_sse,
                                polish_updates);
    sixel_palette_kcenter_log_finish(logger,
                                     job_iter,
                                     engine_name,
                                     "palette/iterate",
                                     "iterate",
                                     log_detail);

    merge_start = iterate_stop;
    merge_stop = iterate_stop;
    if (apply_merge && k > reqcolors) {
        merge_start = sixel_timer_now();
        job_merge = sixel_palette_kcenter_log_start(logger,
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
                component = cluster_sums[slot * 3u + channel]
                    / cluster_weights[slot];
                if (component < 0.0) {
                    component = 0.0;
                }
                if (component > 255.0) {
                    component = 255.0;
                }
                merge_sums[slot * 3u + channel]
                    = component * (double)merge_weights[slot];
            }
        }

        cluster_total = sixel_palette_apply_merge(merge_weights,
                                                  merge_sums,
                                                  3u,
                                                  (int)k,
                                                  (int)reqcolors,
                                                  (int)resolved_merge,
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
            for (channel = 0u; channel < 3u; ++channel) {
                final_centers[slot * 3u + channel]
                    = merge_sums[slot * 3u + channel] / weight_value;
            }
            cluster_weights[slot] = weight_value;
        }

        merge_stop = sixel_timer_now();
        (void)sixel_compat_snprintf(log_detail,
                                    sizeof(log_detail),
                                    "clusters=%u merge=%u",
                                    final_count,
                                    resolved_merge);
        sixel_palette_kcenter_log_finish(logger,
                                         job_merge,
                                         engine_name,
                                         "palette/merge",
                                         "merge",
                                         log_detail);
    }

    export_start = sixel_timer_now();
    job_export = sixel_palette_kcenter_log_start(logger,
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
            component = final_centers[slot * 3u + channel];
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            if (float_palette != NULL) {
                if (float32_channel_scale[channel] > 0.0) {
                    restored_component = component;
                    restored_component -= float32_channel_offset[channel];
                    restored_component /= float32_channel_scale[channel];
                } else {
                    restored_component = 0.0;
                }
                float_palette[slot * 3u + channel]
                    = sixel_pixelformat_float_channel_clamp(
                        pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            palette[slot * 3u + channel] = (unsigned char)(component + 0.5);
        }
    }

    if (force_palette && final_count < reqcolors && final_count > 0u) {
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
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (float_palette != NULL) {
            grown_float = (float *)sixel_allocator_malloc(
                allocator,
                (size_t)reqcolors * 3u * sizeof(float));
            if (grown_float == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        memcpy(grown_palette,
               palette,
               (size_t)final_count * 3u * sizeof(unsigned char));
        if (grown_float != NULL) {
            memcpy(grown_float,
                   float_palette,
                   (size_t)final_count * 3u * sizeof(float));
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
        grown_palette = NULL;
        if (float_palette != NULL) {
            sixel_allocator_free(allocator, float_palette);
            float_palette = grown_float;
            grown_float = NULL;
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
    sixel_palette_kcenter_log_finish(logger,
                                     job_export,
                                     engine_name,
                                     "palette/export",
                                     "export",
                                     log_detail);

    status = SIXEL_OK;

end:
    if (telemetry != NULL) {
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
        telemetry->merge_iterate_count
            = (apply_merge && k > reqcolors) ? 1u : 0u;
        telemetry->merge_mode = (apply_merge && k > reqcolors)
            ? (int)resolved_merge
            : SIXEL_FINAL_MERGE_NONE;
    }

    if (grown_float != NULL) {
        sixel_allocator_free(allocator, grown_float);
    }
    if (grown_palette != NULL) {
        sixel_allocator_free(allocator, grown_palette);
    }
    if (float_palette != NULL) {
        sixel_allocator_free(allocator, float_palette);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (merge_sums != NULL) {
        sixel_allocator_free(allocator, merge_sums);
    }
    if (merge_weights != NULL) {
        sixel_allocator_free(allocator, merge_weights);
    }
    if (center_mask != NULL) {
        sixel_allocator_free(allocator, center_mask);
    }
    if (fft_dist_cache != NULL) {
        sixel_allocator_free(allocator, fft_dist_cache);
    }
    if (scratch_indices != NULL) {
        sixel_allocator_free(allocator, scratch_indices);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (sort_weights != NULL) {
        sixel_allocator_free(allocator, sort_weights);
    }
    if (final_centers != NULL) {
        sixel_allocator_free(allocator, final_centers);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weights != NULL) {
        sixel_allocator_free(allocator, cluster_weights);
    }
    if (scratch_dist != NULL) {
        sixel_allocator_free(allocator, scratch_dist);
    }
    if (scratch_second_dist != NULL) {
        sixel_allocator_free(allocator, scratch_second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (scratch_slot != NULL) {
        sixel_allocator_free(allocator, scratch_slot);
    }
    if (scratch_second_slot != NULL) {
        sixel_allocator_free(allocator, scratch_second_slot);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (second_slot != NULL) {
        sixel_allocator_free(allocator, second_slot);
    }
    if (work_centers != NULL) {
        sixel_allocator_free(allocator, work_centers);
    }
    if (best_centers != NULL) {
        sixel_allocator_free(allocator, best_centers);
    }
    if (centers != NULL) {
        sixel_allocator_free(allocator, centers);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    sixel_kcenter_override_lock_release(override_lock_acquired);
    return status;
}

static SIXELSTATUS
sixel_palette_build_kcenter_internal(sixel_kcenter_internal_ctx_t *ctx)
{
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_logger_t *logger;
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
        work_allocator = palette->allocator;
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
    build_ctx->reqcolors = palette->requested_colors;
    build_ctx->ncolors = &ncolors;
    build_ctx->origcolors = &origcolors;
    build_ctx->quality_mode = palette->quality_mode;
    build_ctx->force_palette = palette->force_palette;
    build_ctx->use_reversible = palette->use_reversible;
    build_ctx->final_merge_mode = palette->final_merge_mode;
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
                "sixel_palette_build_kcenter: palette payload is missing.");
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
                            sixel_logger_t *logger,
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
                                    sixel_logger_t *logger,
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
