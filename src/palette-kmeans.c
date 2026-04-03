/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * This translation unit owns the K-means palette quantizer.  The structure is
 * organised so palette.c can delegate the algorithm-specific work while it
 * continues handling configuration and result publication.  The processing
 * pipeline follows the stages below:
 *
 *   [sample collection] -> [k-means++ seeding] -> [Lloyd iteration]
 *                      -> [optional final merge] -> [palette export]
 *
 * Each stage is implemented in a dedicated block with extensive comments so
 * future maintainers can reason about the data flow without cross-referencing
 * the orchestrator.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans.h"
#include "palette.h"
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

static SIXEL_TLS int sixel_kmeans_init_type_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_init_type sixel_kmeans_init_type_override_value
    = SIXEL_PALETTE_KMEANS_INIT_AUTO;
static SIXEL_TLS int sixel_kmeans_binning_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_binning_mode
    sixel_kmeans_binning_mode_override_value
        = SIXEL_PALETTE_KMEANS_BINNING_AUTO;
static SIXEL_TLS int sixel_kmeans_binbits_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_binbits_override_value = 6u;
static SIXEL_TLS int sixel_kmeans_mapping_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_mapping_mode
    sixel_kmeans_mapping_mode_override_value
        = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
static SIXEL_TLS int sixel_kmeans_softdist_mode_override_enabled = 0;
static SIXEL_TLS sixel_kmeans_softdist_mode
    sixel_kmeans_softdist_mode_override_value
        = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
static SIXEL_TLS int sixel_kmeans_autoratio_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_autoratio_override_value = 32u;

#undef SIXEL_TLS

typedef struct sixel_kmeans_projection_entry {
    double projection;
    double weight;
    unsigned int index;
} sixel_kmeans_projection_entry_t;

static int
sixel_palette_kmeans_log_start(sixel_logger_t *logger,
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
sixel_palette_kmeans_log_finish(sixel_logger_t *logger,
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

static const char *
sixel_kmeans_init_type_to_string(sixel_kmeans_init_type init_type)
{
    switch (init_type) {
    case SIXEL_PALETTE_KMEANS_INIT_NONE:
        return "none";
    case SIXEL_PALETTE_KMEANS_INIT_PCA:
        return "pca";
    case SIXEL_PALETTE_KMEANS_INIT_AUTO:
    default:
        return "auto";
    }
}

static sixel_kmeans_init_type
sixel_kmeans_resolve_init_type(sixel_kmeans_init_type init_type)
{
    if (init_type == SIXEL_PALETTE_KMEANS_INIT_PCA) {
        return init_type;
    }

    return SIXEL_PALETTE_KMEANS_INIT_NONE;
}

void
sixel_set_kmeans_init_type_override(int enabled,
                                    sixel_kmeans_init_type init_type)
{
    sixel_kmeans_init_type_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_init_type_override_value = init_type;
}

SIXELAPI sixel_kmeans_init_type
sixel_get_kmeans_init_type(void)
{
    sixel_kmeans_init_type parsed;
    sixel_kmeans_init_type resolved;
    char const *env_value;
    static int init_loaded = 0;
    static sixel_kmeans_init_type cached_value
        = SIXEL_PALETTE_KMEANS_INIT_PCA;

    parsed = SIXEL_PALETTE_KMEANS_INIT_AUTO;
    resolved = SIXEL_PALETTE_KMEANS_INIT_PCA;
    env_value = NULL;
    if (sixel_kmeans_init_type_override_enabled) {
        return sixel_kmeans_resolve_init_type(
            sixel_kmeans_init_type_override_value);
    }
    if (init_loaded) {
        return cached_value;
    }
    init_loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_INITTYPE");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "none") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_INIT_NONE;
        } else if (sixel_compat_strcasecmp(env_value, "pca") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_INIT_PCA;
        } else if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            parsed = SIXEL_PALETTE_KMEANS_INIT_AUTO;
        }
    }

    resolved = sixel_kmeans_resolve_init_type(parsed);
    cached_value = resolved;
    sixel_debugf("k-means init type: %s",
                 sixel_kmeans_init_type_to_string(resolved));

    return resolved;
}

static sixel_kmeans_binning_mode
sixel_kmeans_resolve_binning_mode(sixel_kmeans_binning_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_BINNING_NONE:
    case SIXEL_PALETTE_KMEANS_BINNING_HARD:
    case SIXEL_PALETTE_KMEANS_BINNING_SOFT:
    case SIXEL_PALETTE_KMEANS_BINNING_AUTO:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_BINNING_AUTO;
    }
}

void
sixel_set_kmeans_binning_mode_override(int enabled,
                                       sixel_kmeans_binning_mode mode)
{
    sixel_kmeans_binning_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_binning_mode_override_value
        = sixel_kmeans_resolve_binning_mode(mode);
}

SIXEL_INTERNAL_API sixel_kmeans_binning_mode
sixel_get_kmeans_binning_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_binning_mode cached
        = SIXEL_PALETTE_KMEANS_BINNING_AUTO;

    env_value = NULL;
    if (sixel_kmeans_binning_mode_override_enabled) {
        return sixel_kmeans_binning_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_BINNING");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "none") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_NONE;
        } else if (sixel_compat_strcasecmp(env_value, "hard") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_HARD;
        } else if (sixel_compat_strcasecmp(env_value, "soft") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_SOFT;
        } else if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            cached = SIXEL_PALETTE_KMEANS_BINNING_AUTO;
        }
    }

    return cached;
}

void
sixel_set_kmeans_binbits_override(int enabled,
                                  unsigned int bits)
{
    sixel_kmeans_binbits_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_binbits_override_value = bits;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_binbits(void)
{
    char const *env_value;
    char *endptr;
    long parsed;
    static int loaded = 0;
    static unsigned int cached = 6u;

    env_value = NULL;
    endptr = NULL;
    parsed = 0L;
    if (sixel_kmeans_binbits_override_enabled) {
        if (sixel_kmeans_binbits_override_value < 4u) {
            return 4u;
        }
        if (sixel_kmeans_binbits_override_value > 8u) {
            return 8u;
        }
        return sixel_kmeans_binbits_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_BINBITS");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed = strtol(env_value, &endptr, 10);
        if (endptr != env_value && endptr != NULL && endptr[0] == '\0'
                && errno == 0) {
            if (parsed < 4L) {
                parsed = 4L;
            }
            if (parsed > 8L) {
                parsed = 8L;
            }
            cached = (unsigned int)parsed;
        }
    }

    return cached;
}

static sixel_kmeans_mapping_mode
sixel_kmeans_resolve_mapping_mode(sixel_kmeans_mapping_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM:
    case SIXEL_PALETTE_KMEANS_MAPPING_SRGB:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
    }
}

void
sixel_set_kmeans_mapping_mode_override(int enabled,
                                       sixel_kmeans_mapping_mode mode)
{
    sixel_kmeans_mapping_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_mapping_mode_override_value
        = sixel_kmeans_resolve_mapping_mode(mode);
}

SIXEL_INTERNAL_API sixel_kmeans_mapping_mode
sixel_get_kmeans_mapping_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_mapping_mode cached
        = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;

    env_value = NULL;
    if (sixel_kmeans_mapping_mode_override_enabled) {
        return sixel_kmeans_mapping_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_MAPPING");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "srgb") == 0) {
            cached = SIXEL_PALETTE_KMEANS_MAPPING_SRGB;
        } else if (sixel_compat_strcasecmp(env_value, "uniform") == 0) {
            cached = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
        }
    }

    return cached;
}

static sixel_kmeans_softdist_mode
sixel_kmeans_resolve_softdist_mode(sixel_kmeans_softdist_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR:
        return mode;
    default:
        return SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
    }
}

void
sixel_set_kmeans_softdist_mode_override(int enabled,
                                        sixel_kmeans_softdist_mode mode)
{
    sixel_kmeans_softdist_mode_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_softdist_mode_override_value
        = sixel_kmeans_resolve_softdist_mode(mode);
}

SIXEL_INTERNAL_API sixel_kmeans_softdist_mode
sixel_get_kmeans_softdist_mode(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmeans_softdist_mode cached
        = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;

    env_value = NULL;
    if (sixel_kmeans_softdist_mode_override_enabled) {
        return sixel_kmeans_softdist_mode_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_SOFTDIST");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "trilinear") == 0) {
            cached = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;
        }
    }

    return cached;
}

void
sixel_set_kmeans_autoratio_override(int enabled,
                                    unsigned int ratio)
{
    sixel_kmeans_autoratio_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_autoratio_override_value = ratio;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmeans_autoratio(void)
{
    char const *env_value;
    char *endptr;
    long parsed;
    static int loaded = 0;
    static unsigned int cached = 32u;

    env_value = NULL;
    endptr = NULL;
    parsed = 0L;
    if (sixel_kmeans_autoratio_override_enabled) {
        if (sixel_kmeans_autoratio_override_value < 1u) {
            return 1u;
        }
        return sixel_kmeans_autoratio_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_AUTORATIO");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed = strtol(env_value, &endptr, 10);
        if (endptr != env_value && endptr != NULL && endptr[0] == '\0'
                && errno == 0) {
            if (parsed < 1L) {
                parsed = 1L;
            }
            if (parsed > 1048576L) {
                parsed = 1048576L;
            }
            cached = (unsigned int)parsed;
        }
    }

    return cached;
}

static int
sixel_kmeans_projection_compare(void const *lhs, void const *rhs)
{
    sixel_kmeans_projection_entry_t const *left;
    sixel_kmeans_projection_entry_t const *right;

    left = (sixel_kmeans_projection_entry_t const *)lhs;
    right = (sixel_kmeans_projection_entry_t const *)rhs;
    if (left->projection < right->projection) {
        return -1;
    }
    if (left->projection > right->projection) {
        return 1;
    }

    return 0;
}

static int
sixel_kmeans_compute_mean(double const *samples,
                          double const *weights,
                          unsigned int sample_count,
                          double mean[3],
                          double *total_weight)
{
    unsigned int index;
    double weight;
    double weight_sum;
    double accum[3];

    index = 0U;
    weight = 1.0;
    weight_sum = 0.0;
    accum[0] = 0.0;
    accum[1] = 0.0;
    accum[2] = 0.0;
    if (samples == NULL || mean == NULL || total_weight == NULL) {
        return 1;
    }
    for (index = 0U; index < sample_count; ++index) {
        if (weights != NULL) {
            weight = weights[index];
        }
        if (weight <= 0.0) {
            continue;
        }
        weight_sum += weight;
        accum[0] += samples[index * 3U + 0U] * weight;
        accum[1] += samples[index * 3U + 1U] * weight;
        accum[2] += samples[index * 3U + 2U] * weight;
    }
    if (weight_sum <= 0.0) {
        return 1;
    }

    mean[0] = accum[0] / weight_sum;
    mean[1] = accum[1] / weight_sum;
    mean[2] = accum[2] / weight_sum;
    *total_weight = weight_sum;

    return 0;
}

static int
sixel_kmeans_compute_covariance(double const *samples,
                                double const *weights,
                                unsigned int sample_count,
                                double const mean[3],
                                double covariance[3][3])
{
    unsigned int index;
    unsigned int row;
    unsigned int col;
    double weight;
    double weight_sum;
    double centered[3];

    index = 0U;
    row = 0U;
    col = 0U;
    weight = 1.0;
    weight_sum = 0.0;
    centered[0] = 0.0;
    centered[1] = 0.0;
    centered[2] = 0.0;
    if (samples == NULL || covariance == NULL || mean == NULL) {
        return 1;
    }
    for (row = 0U; row < 3U; ++row) {
        for (col = 0U; col < 3U; ++col) {
            covariance[row][col] = 0.0;
        }
    }

    for (index = 0U; index < sample_count; ++index) {
        if (weights != NULL) {
            weight = weights[index];
        }
        if (weight <= 0.0) {
            continue;
        }
        centered[0] = samples[index * 3U + 0U] - mean[0];
        centered[1] = samples[index * 3U + 1U] - mean[1];
        centered[2] = samples[index * 3U + 2U] - mean[2];
        for (row = 0U; row < 3U; ++row) {
            for (col = row; col < 3U; ++col) {
                covariance[row][col]
                    += centered[row] * centered[col] * weight;
            }
        }
        weight_sum += weight;
    }
    if (weight_sum <= 0.0) {
        return 1;
    }
    for (row = 0U; row < 3U; ++row) {
        for (col = row; col < 3U; ++col) {
            covariance[row][col] /= weight_sum;
            if (row != col) {
                covariance[col][row] = covariance[row][col];
            }
        }
    }

    return 0;
}

static int
sixel_kmeans_power_iteration(double covariance[3][3],
                             double axis[3],
                             unsigned int iterations)
{
    double vector[3];
    double next[3];
    double norm;
    unsigned int iter;

    vector[0] = 1.0;
    vector[1] = 1.0;
    vector[2] = 1.0;
    next[0] = 0.0;
    next[1] = 0.0;
    next[2] = 0.0;
    norm = 0.0;
    for (iter = 0U; iter < iterations; ++iter) {
        next[0] = covariance[0][0] * vector[0]
            + covariance[0][1] * vector[1]
            + covariance[0][2] * vector[2];
        next[1] = covariance[1][0] * vector[0]
            + covariance[1][1] * vector[1]
            + covariance[1][2] * vector[2];
        next[2] = covariance[2][0] * vector[0]
            + covariance[2][1] * vector[1]
            + covariance[2][2] * vector[2];
        norm = next[0] * next[0]
            + next[1] * next[1]
            + next[2] * next[2];
        if (norm <= 0.0) {
            return 1;
        }
        norm = sqrt(norm);
        vector[0] = next[0] / norm;
        vector[1] = next[1] / norm;
        vector[2] = next[2] / norm;
    }

    axis[0] = vector[0];
    axis[1] = vector[1];
    axis[2] = vector[2];

    return 0;
}

static int
sixel_kmeans_seed_pca(double *centers,
                      unsigned int k,
                      double const *samples,
                      double const *weights,
                      unsigned int sample_count,
                      int use_reversible,
                      int pixelformat,
                      sixel_allocator_t *allocator)
{
    sixel_kmeans_projection_entry_t *projections = NULL;
    double covariance[3][3];
    double mean[3];
    double axis[3];
    double total_weight = 0.0;
    double bucket_start = 0.0;
    double bucket_end = 0.0;
    double bucket_weight = 0.0;
    double cumulative = 0.0;
    double sum[3] = { 0.0, 0.0, 0.0 };
    unsigned int bucket = 0U;
    unsigned int cursor = 0U;
    unsigned int channel = 0U;
    int status;

    status = sixel_kmeans_compute_mean(samples,
                                       weights,
                                       sample_count,
                                       mean,
                                       &total_weight);
    if (status != 0) {
        return 1;
    }
    status = sixel_kmeans_compute_covariance(samples,
                                             weights,
                                             sample_count,
                                             mean,
                                             covariance);
    if (status != 0) {
        return 1;
    }
    status = sixel_kmeans_power_iteration(covariance, axis, 16U);
    if (status != 0) {
        return 1;
    }

    projections = (sixel_kmeans_projection_entry_t *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_count * sizeof(sixel_kmeans_projection_entry_t));
    if (projections == NULL) {
        return 1;
    }
    for (cursor = 0U; cursor < sample_count; ++cursor) {
        double weight;

        weight = 1.0;
        if (weights != NULL) {
            weight = weights[cursor];
        }
        projections[cursor].projection
            = samples[cursor * 3U + 0U] * axis[0]
            + samples[cursor * 3U + 1U] * axis[1]
            + samples[cursor * 3U + 2U] * axis[2];
        projections[cursor].weight = weight;
        projections[cursor].index = cursor;
    }
    qsort(projections,
          (size_t)sample_count,
          sizeof(sixel_kmeans_projection_entry_t),
          sixel_kmeans_projection_compare);

    cumulative = 0.0;
    cursor = 0U;
    for (bucket = 0U; bucket < k; ++bucket) {
        bucket_start = total_weight * (double)bucket / (double)k;
        bucket_end = total_weight * (double)(bucket + 1U)
            / (double)k;
        bucket_weight = 0.0;
        sum[0] = 0.0;
        sum[1] = 0.0;
        sum[2] = 0.0;
        while (cursor < sample_count && cumulative < bucket_end) {
            double weight;
            unsigned int sample_index;

            weight = projections[cursor].weight;
            if (weight <= 0.0) {
                ++cursor;
                continue;
            }
            sample_index = projections[cursor].index;
            if (cumulative + weight < bucket_start) {
                cumulative += weight;
                ++cursor;
                continue;
            }
            bucket_weight += weight;
            sum[0] += samples[sample_index * 3U + 0U] * weight;
            sum[1] += samples[sample_index * 3U + 1U] * weight;
            sum[2] += samples[sample_index * 3U + 2U] * weight;
            cumulative += weight;
            ++cursor;
        }
        if (bucket_weight <= 0.0) {
            unsigned int fallback;

            fallback = 0U;
            if (cursor > 0U) {
                fallback = projections[cursor - 1U].index;
            }
            bucket_weight = 1.0;
            sum[0] = samples[fallback * 3U + 0U];
            sum[1] = samples[fallback * 3U + 1U];
            sum[2] = samples[fallback * 3U + 2U];
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[bucket * 3U + channel] = sum[channel] / bucket_weight;
        }
        sixel_palette_snap_triple(&centers[bucket * 3U],
                                  use_reversible,
                                  pixelformat,
                                  SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED);
    }

    sixel_allocator_free(allocator, projections);

    return 0;
}

static SIXELSTATUS
sixel_kmeans_seed_legacy(double *centers,
                         unsigned int k,
                         double const *samples,
                         double const *weights,
                         unsigned int sample_count,
                         double *distance_cache)
{
    unsigned int channel;
    unsigned int index;
    unsigned int sample_index;
    unsigned int center_index;
    unsigned int replace;
    unsigned int first_index;
    double total_weight;
    double random_point;
    double distance;
    double diff;
    double sample_weight;
    unsigned long rand_value;

    channel = 0U;
    index = 0U;
    sample_index = 0U;
    center_index = 0U;
    replace = 0U;
    first_index = 0U;
    total_weight = 0.0;
    random_point = 0.0;
    distance = 0.0;
    diff = 0.0;
    sample_weight = 1.0;
    rand_value = (unsigned long)rand();
    replace = 0U;
    if (weights != NULL) {
        for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
            sample_weight = weights[sample_index];
            if (sample_weight > 0.0) {
                total_weight += sample_weight;
            }
        }
        if (total_weight > 0.0) {
            random_point = ((double)rand() / ((double)RAND_MAX + 1.0))
                * total_weight;
            first_index = 0U;
            while (first_index + 1U < sample_count) {
                sample_weight = weights[first_index];
                if (sample_weight > 0.0) {
                    if (random_point <= sample_weight) {
                        break;
                    }
                    random_point -= sample_weight;
                }
                ++first_index;
            }
            replace = first_index;
        } else {
            replace = (unsigned int)(rand_value % sample_count);
        }
    } else {
        replace = (unsigned int)(rand_value % sample_count);
    }
    for (channel = 0U; channel < 3U; ++channel) {
        centers[channel] = (double)samples[replace * 3U + channel];
    }
    for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
        distance = 0.0;
        for (channel = 0U; channel < 3U; ++channel) {
            diff = (double)samples[sample_index * 3U + channel]
                - centers[channel];
            distance += diff * diff;
        }
        distance_cache[sample_index] = distance;
    }
    for (center_index = 1U; center_index < k; ++center_index) {
        total_weight = 0.0;
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            sample_weight = 1.0;
            if (weights != NULL) {
                sample_weight = weights[sample_index];
            }
            if (sample_weight <= 0.0) {
                continue;
            }
            total_weight += distance_cache[sample_index] * sample_weight;
        }
        random_point = 0.0;
        if (total_weight > 0.0) {
            random_point = ((double)rand() / ((double)RAND_MAX + 1.0))
                * total_weight;
        }
        sample_index = 0U;
        while (sample_index + 1U < sample_count &&
               random_point > 0.0) {
            sample_weight = 1.0;
            if (weights != NULL) {
                sample_weight = weights[sample_index];
            }
            if (sample_weight > 0.0) {
                random_point -= distance_cache[sample_index] * sample_weight;
            }
            ++sample_index;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[center_index * 3U + channel]
                = (double)samples[sample_index * 3U + channel];
        }
        for (index = 0U; index < sample_count; ++index) {
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[index * 3U + channel]
                    - centers[center_index * 3U + channel];
                distance += diff * diff;
            }
            if (distance < distance_cache[index]) {
                distance_cache[index] = distance;
            }
        }
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_kmeans_choose_initial_centroids(double *centers,
                                      unsigned int k,
                                      double const *samples,
                                      double const *weights,
                                      unsigned int sample_count,
                                      int use_reversible,
                                      int pixelformat,
                                      double *distance_cache,
                                      sixel_allocator_t *allocator,
                                      sixel_kmeans_init_type init_type)
{
    sixel_kmeans_init_type resolved;
    SIXELSTATUS status;
    double *scratch_distances;
    double snapped[3];
    unsigned int center_index;
    unsigned int channel;

    resolved = sixel_kmeans_resolve_init_type(init_type);
    status = SIXEL_BAD_ARGUMENT;
    scratch_distances = distance_cache;
    if (centers == NULL || samples == NULL || allocator == NULL) {
        return status;
    }
    if (k == 0U || sample_count == 0U) {
        return status;
    }
    if (resolved == SIXEL_PALETTE_KMEANS_INIT_PCA) {
        int seed_status;

        seed_status = sixel_kmeans_seed_pca(centers,
                                            k,
                                            samples,
                                            weights,
                                            sample_count,
                                            use_reversible,
                                            pixelformat,
                                            allocator);
        if (seed_status == 0) {
            return SIXEL_OK;
        }
        sixel_debugf("PCA seeding failed, falling back to legacy mode");
        resolved = SIXEL_PALETTE_KMEANS_INIT_NONE;
    }

    if (scratch_distances == NULL) {
        scratch_distances = (double *)sixel_allocator_malloc(
            allocator, (size_t)sample_count * sizeof(double));
        if (scratch_distances == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
    }
    status = sixel_kmeans_seed_legacy(centers,
                                      k,
                                      samples,
                                      weights,
                                      sample_count,
                                      scratch_distances);
    if (scratch_distances != distance_cache) {
        sixel_allocator_free(allocator, scratch_distances);
    }

    /*
     * Snap initial centroids when the timing policy requests it.  This keeps
     * seed positions aligned with the reversible grid before Lloyd
     * refinement begins.
     */
    if (SIXEL_SUCCEEDED(status)) {
        for (center_index = 0U; center_index < k; ++center_index) {
            for (channel = 0U; channel < 3U; ++channel) {
                snapped[channel]
                    = centers[center_index * 3U + channel];
            }
            sixel_palette_snap_triple(
                snapped,
                use_reversible,
                pixelformat,
                SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED);
            for (channel = 0U; channel < 3U; ++channel) {
                centers[center_index * 3U + channel] = snapped[channel];
            }
        }
    }

    return status;
}

static int
sixel_palette_float32_alpha_visible(double alpha)
{
#if HAVE_MATH_H
    if (!isfinite(alpha)) {
        return 0;
    }
#endif

    return alpha > 0.0;
}

/*
 * Probe the input stream to count unique colours up to the requested limit.
 * The helper is used to skip the expensive merge stage when the source image
 * already fits within the desired palette size.  The function only considers
 * opaque pixels to remain consistent with the quantizer sampling logic.
 */
static SIXELSTATUS
sixel_palette_count_unique_within_limit(unsigned char const *data,
                                        unsigned int length,
                                        unsigned int channels,
                                        unsigned int limit,
                                        unsigned int *unique_count,
                                        int *within_limit,
                                        sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    uint32_t *table;
    unsigned int table_size;
    unsigned int mask;
    unsigned int pixel_count;
    unsigned int index;
    unsigned int base;
    unsigned int slot;
    unsigned int unique;
    uint32_t color;
    int limited;

    status = SIXEL_BAD_ARGUMENT;
    table = NULL;
    table_size = 0U;
    mask = 0U;
    pixel_count = 0U;
    index = 0U;
    base = 0U;
    slot = 0U;
    unique = 0U;
    color = 0U;
    limited = 0;

    if (unique_count != NULL) {
        *unique_count = 0U;
    }
    if (within_limit != NULL) {
        *within_limit = 0;
    }
    if (data == NULL || allocator == NULL) {
        return status;
    }
    if (channels != 3U && channels != 4U) {
        return status;
    }
    if (limit == 0U) {
        return status;
    }

    pixel_count = length / channels;
    if (pixel_count == 0U) {
        status = SIXEL_OK;
        if (within_limit != NULL) {
            *within_limit = 1;
        }
        return status;
    }

    table_size = 1U;
    while (table_size < limit * 2U) {
        table_size <<= 1U;
    }
    if (table_size < 8U) {
        table_size = 8U;
    }
    mask = table_size - 1U;

    table = (uint32_t *)sixel_allocator_malloc(
        allocator, (size_t)table_size * sizeof(uint32_t));
    if (table == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0U; index < table_size; ++index) {
        table[index] = 0xffffffffU;
    }

    limited = 1;
    for (index = 0U; index < pixel_count; ++index) {
        base = index * channels;
        if (channels == 4U && data[base + 3U] == 0U) {
            continue;
        }
        color = ((uint32_t)data[base] << 16)
              | ((uint32_t)data[base + 1U] << 8)
              | (uint32_t)data[base + 2U];
        slot = (unsigned int)(((uint32_t)0x9e3779b9U * color) & mask);
        while (table[slot] != 0xffffffffU && table[slot] != color) {
            slot = (slot + 1U) & mask;
        }
        if (table[slot] == color) {
            continue;
        }
        table[slot] = color;
        ++unique;
        if (unique > limit) {
            limited = 0;
            unique = limit + 1U;
            break;
        }
    }

    status = SIXEL_OK;
    if (unique_count != NULL) {
        *unique_count = unique;
    }
    if (within_limit != NULL) {
        *within_limit = limited;
    }

    sixel_allocator_free(allocator, table);
    return status;
}

static double
sixel_palette_kmeans_sum_float_to_byte(double component,
                                       double sample_total,
                                       unsigned int channel,
                                       double const *scale,
                                       double const *offset)
{
    double scaled;

    if (scale == NULL || offset == NULL) {
        return component;
    }
    if (scale[channel] <= 0.0) {
        return 0.0;
    }

    scaled = component * scale[channel];
    scaled += (double)sample_total * offset[channel];
    return scaled;
}

static double
sixel_palette_kmeans_sum_byte_to_float(double component,
                                       double sample_total,
                                       unsigned int channel,
                                       double const *scale,
                                       double const *offset)
{
    double restored;

    if (scale == NULL || offset == NULL) {
        return component;
    }
    if (scale[channel] <= 0.0) {
        return 0.0;
    }

    restored = component - (double)sample_total * offset[channel];
    restored /= scale[channel];
    return restored;
}

typedef struct sixel_kmeans_bin_entry {
    uint32_t key;
    double weight;
    double sum[3];
} sixel_kmeans_bin_entry_t;

typedef struct sixel_kmeans_histogram {
    sixel_kmeans_bin_entry_t *entries;
    unsigned int capacity;
    unsigned int mask;
    unsigned int size;
    unsigned int binbits;
    unsigned int bin_count;
} sixel_kmeans_histogram_t;

static uint32_t
sixel_kmeans_hash_u32(uint32_t key)
{
    key ^= key >> 16;
    key *= 0x7feb352dU;
    key ^= key >> 15;
    key *= 0x846ca68bU;
    key ^= key >> 16;

    return key;
}

static unsigned int
sixel_kmeans_histogram_recommended_capacity(size_t expected)
{
    unsigned int capacity;
    size_t threshold;

    capacity = 8u;
    threshold = 0u;
    while (capacity < (1u << 30)) {
        threshold = (size_t)capacity * 7u / 10u;
        if (threshold >= expected) {
            break;
        }
        capacity <<= 1u;
    }

    return capacity;
}

static SIXELSTATUS
sixel_kmeans_histogram_init(sixel_kmeans_histogram_t *histogram,
                            size_t expected_entries,
                            unsigned int binbits,
                            sixel_allocator_t *allocator)
{
    size_t i;
    unsigned int capacity;

    i = 0u;
    capacity = 0u;
    if (histogram == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (binbits < 4u || binbits > 8u) {
        return SIXEL_BAD_ARGUMENT;
    }

    histogram->entries = NULL;
    histogram->capacity = 0u;
    histogram->mask = 0u;
    histogram->size = 0u;
    histogram->binbits = binbits;
    histogram->bin_count = 1u << binbits;

    capacity = sixel_kmeans_histogram_recommended_capacity(expected_entries);
    histogram->entries = (sixel_kmeans_bin_entry_t *)sixel_allocator_malloc(
        allocator, (size_t)capacity * sizeof(sixel_kmeans_bin_entry_t));
    if (histogram->entries == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (i = 0u; i < (size_t)capacity; ++i) {
        histogram->entries[i].key = UINT32_MAX;
        histogram->entries[i].weight = 0.0;
        histogram->entries[i].sum[0] = 0.0;
        histogram->entries[i].sum[1] = 0.0;
        histogram->entries[i].sum[2] = 0.0;
    }
    histogram->capacity = capacity;
    histogram->mask = capacity - 1u;

    return SIXEL_OK;
}

static void
sixel_kmeans_histogram_dispose(sixel_kmeans_histogram_t *histogram,
                               sixel_allocator_t *allocator)
{
    if (histogram == NULL || allocator == NULL) {
        return;
    }
    if (histogram->entries != NULL) {
        sixel_allocator_free(allocator, histogram->entries);
    }
    histogram->entries = NULL;
    histogram->capacity = 0u;
    histogram->mask = 0u;
    histogram->size = 0u;
    histogram->binbits = 0u;
    histogram->bin_count = 0u;
}

static SIXELSTATUS
sixel_kmeans_histogram_grow(sixel_kmeans_histogram_t *histogram,
                            sixel_allocator_t *allocator)
{
    sixel_kmeans_bin_entry_t *grown;
    unsigned int old_capacity;
    unsigned int new_capacity;
    unsigned int old_mask;
    unsigned int slot;
    unsigned int probe;
    unsigned int index;

    grown = NULL;
    old_capacity = 0u;
    new_capacity = 0u;
    old_mask = 0u;
    slot = 0u;
    probe = 0u;
    index = 0u;
    if (histogram == NULL || allocator == NULL || histogram->entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    old_capacity = histogram->capacity;
    if (old_capacity == 0u || old_capacity >= (1u << 30)) {
        return SIXEL_BAD_ALLOCATION;
    }
    new_capacity = old_capacity << 1u;
    old_mask = new_capacity - 1u;
    grown = (sixel_kmeans_bin_entry_t *)sixel_allocator_malloc(
        allocator, (size_t)new_capacity * sizeof(sixel_kmeans_bin_entry_t));
    if (grown == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < new_capacity; ++index) {
        grown[index].key = UINT32_MAX;
        grown[index].weight = 0.0;
        grown[index].sum[0] = 0.0;
        grown[index].sum[1] = 0.0;
        grown[index].sum[2] = 0.0;
    }

    for (index = 0u; index < old_capacity; ++index) {
        if (histogram->entries[index].key == UINT32_MAX) {
            continue;
        }
        slot = sixel_kmeans_hash_u32(histogram->entries[index].key) & old_mask;
        while (grown[slot].key != UINT32_MAX) {
            probe = (slot + 1u) & old_mask;
            slot = probe;
        }
        grown[slot] = histogram->entries[index];
    }

    sixel_allocator_free(allocator, histogram->entries);
    histogram->entries = grown;
    histogram->capacity = new_capacity;
    histogram->mask = old_mask;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kmeans_histogram_add(sixel_kmeans_histogram_t *histogram,
                           uint32_t key,
                           double weight,
                           double const sample[3],
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int slot;
    unsigned int probe;
    double *sum;

    status = SIXEL_OK;
    slot = 0u;
    probe = 0u;
    sum = NULL;
    if (histogram == NULL || histogram->entries == NULL ||
            sample == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (weight <= 0.0) {
        return SIXEL_OK;
    }

    if ((size_t)(histogram->size + 1u) * 10u
            > (size_t)histogram->capacity * 7u) {
        status = sixel_kmeans_histogram_grow(histogram, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    slot = sixel_kmeans_hash_u32(key) & histogram->mask;
    while (histogram->entries[slot].key != UINT32_MAX
            && histogram->entries[slot].key != key) {
        probe = (slot + 1u) & histogram->mask;
        slot = probe;
    }
    if (histogram->entries[slot].key == UINT32_MAX) {
        histogram->entries[slot].key = key;
        histogram->entries[slot].weight = 0.0;
        histogram->entries[slot].sum[0] = 0.0;
        histogram->entries[slot].sum[1] = 0.0;
        histogram->entries[slot].sum[2] = 0.0;
        histogram->size += 1u;
    }
    histogram->entries[slot].weight += weight;
    sum = histogram->entries[slot].sum;
    sum[0] += sample[0] * weight;
    sum[1] += sample[1] * weight;
    sum[2] += sample[2] * weight;

    return SIXEL_OK;
}

static double
sixel_kmeans_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static double
sixel_kmeans_srgb_encode(double value)
{
    double clamped;

    clamped = sixel_kmeans_clamp_unit(value);
    if (clamped <= 0.0031308) {
        return clamped * 12.92;
    }
    return 1.055 * pow(clamped, 1.0 / 2.4) - 0.055;
}

static double
sixel_kmeans_map_sample_to_unit(double sample,
                                int input_is_float32,
                                int pixelformat,
                                unsigned int channel,
                                double const *scale,
                                double const *offset,
                                sixel_kmeans_mapping_mode mapping_mode)
{
    double unit;

    unit = 0.0;
    if (input_is_float32) {
        if (scale != NULL && offset != NULL && scale[channel] > 0.0) {
            unit = sample * scale[channel];
            unit += offset[channel];
            unit /= 255.0;
        }
    } else {
        (void)pixelformat;
        unit = sample / 255.0;
    }
    unit = sixel_kmeans_clamp_unit(unit);
    if (mapping_mode == SIXEL_PALETTE_KMEANS_MAPPING_SRGB) {
        unit = sixel_kmeans_srgb_encode(unit);
    }
    return sixel_kmeans_clamp_unit(unit);
}

static uint32_t
sixel_kmeans_pack_bin_key(unsigned int r,
                          unsigned int g,
                          unsigned int b,
                          unsigned int bits)
{
    return (uint32_t)((r << (bits * 2u)) | (g << bits) | b);
}

static SIXELSTATUS
sixel_kmeans_histogram_add_hard(sixel_kmeans_histogram_t *histogram,
                                double const sample[3],
                                double const mapped[3],
                                sixel_allocator_t *allocator)
{
    unsigned int index[3];
    unsigned int channel;
    double scaled;
    uint32_t key;

    index[0] = 0u;
    index[1] = 0u;
    index[2] = 0u;
    channel = 0u;
    scaled = 0.0;
    key = 0u;
    for (channel = 0u; channel < 3u; ++channel) {
        scaled = mapped[channel] * (double)histogram->bin_count;
        if (scaled >= (double)histogram->bin_count) {
            scaled = (double)histogram->bin_count - 1.0;
        }
        if (scaled < 0.0) {
            scaled = 0.0;
        }
        index[channel] = (unsigned int)scaled;
    }
    key = sixel_kmeans_pack_bin_key(index[0],
                                    index[1],
                                    index[2],
                                    histogram->binbits);

    return sixel_kmeans_histogram_add(histogram, key, 1.0, sample, allocator);
}

static SIXELSTATUS
sixel_kmeans_histogram_add_soft_trilinear(sixel_kmeans_histogram_t *histogram,
                                          double const sample[3],
                                          double const mapped[3],
                                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    double coord[3];
    double frac[3];
    unsigned int low[3];
    unsigned int high[3];
    double weight_axis[3][2];
    unsigned int bit0;
    unsigned int bit1;
    unsigned int bit2;
    unsigned int index0;
    unsigned int index1;
    unsigned int index2;
    double weight;
    double low_as_double;
    uint32_t key;

    status = SIXEL_OK;
    coord[0] = 0.0;
    coord[1] = 0.0;
    coord[2] = 0.0;
    frac[0] = 0.0;
    frac[1] = 0.0;
    frac[2] = 0.0;
    low[0] = 0u;
    low[1] = 0u;
    low[2] = 0u;
    high[0] = 0u;
    high[1] = 0u;
    high[2] = 0u;
    weight_axis[0][0] = 0.0;
    weight_axis[0][1] = 0.0;
    weight_axis[1][0] = 0.0;
    weight_axis[1][1] = 0.0;
    weight_axis[2][0] = 0.0;
    weight_axis[2][1] = 0.0;
    bit0 = 0u;
    bit1 = 0u;
    bit2 = 0u;
    index0 = 0u;
    index1 = 0u;
    index2 = 0u;
    weight = 0.0;
    low_as_double = 0.0;
    key = 0u;

    for (bit0 = 0u; bit0 < 3u; ++bit0) {
        low_as_double = 0.0;
        coord[bit0] = mapped[bit0] * (double)(histogram->bin_count - 1u);
        if (coord[bit0] < 0.0) {
            coord[bit0] = 0.0;
        }
        if (coord[bit0] > (double)(histogram->bin_count - 1u)) {
            coord[bit0] = (double)(histogram->bin_count - 1u);
        }
        low_as_double = floor(coord[bit0]);
        if (low_as_double < 0.0) {
            low_as_double = 0.0;
        }
        if (low_as_double > (double)(histogram->bin_count - 1u)) {
            low_as_double = (double)(histogram->bin_count - 1u);
        }
        low[bit0] = (unsigned int)low_as_double;
        high[bit0] = low[bit0];
        if (high[bit0] + 1u < histogram->bin_count) {
            high[bit0] += 1u;
        }
        frac[bit0] = coord[bit0] - (double)low[bit0];
        if (high[bit0] == low[bit0]) {
            frac[bit0] = 0.0;
        }
        weight_axis[bit0][0] = 1.0 - frac[bit0];
        weight_axis[bit0][1] = frac[bit0];
    }

    for (bit0 = 0u; bit0 < 2u; ++bit0) {
        for (bit1 = 0u; bit1 < 2u; ++bit1) {
            for (bit2 = 0u; bit2 < 2u; ++bit2) {
                index0 = (bit0 == 0u) ? low[0] : high[0];
                index1 = (bit1 == 0u) ? low[1] : high[1];
                index2 = (bit2 == 0u) ? low[2] : high[2];
                weight = weight_axis[0][bit0]
                    * weight_axis[1][bit1]
                    * weight_axis[2][bit2];
                if (weight <= 0.0) {
                    continue;
                }
                key = sixel_kmeans_pack_bin_key(index0,
                                                index1,
                                                index2,
                                                histogram->binbits);
                status = sixel_kmeans_histogram_add(histogram,
                                                    key,
                                                    weight,
                                                    sample,
                                                    allocator);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
            }
        }
    }

    return SIXEL_OK;
}

static sixel_kmeans_binning_mode
sixel_kmeans_resolve_binning_for_input(sixel_kmeans_binning_mode mode,
                                       unsigned int sample_count,
                                       unsigned int reqcolors,
                                       unsigned int autoratio)
{
    unsigned int colors;
    unsigned int ratio;
    unsigned int threshold;

    colors = reqcolors;
    ratio = autoratio;
    threshold = 0u;
    if (mode != SIXEL_PALETTE_KMEANS_BINNING_AUTO) {
        return mode;
    }

    if (colors == 0u) {
        colors = 1u;
    }
    if (ratio == 0u) {
        ratio = 1u;
    }
    if (colors > UINT_MAX / ratio) {
        threshold = UINT_MAX;
    } else {
        threshold = colors * ratio;
    }

    if (sample_count >= threshold) {
        return SIXEL_PALETTE_KMEANS_BINNING_SOFT;
    }

    return SIXEL_PALETTE_KMEANS_BINNING_HARD;
}

static SIXELSTATUS
sixel_kmeans_build_weighted_histogram(
    double const *samples,
    unsigned int sample_count,
    int input_is_float32,
    int pixelformat,
    double const *scale,
    double const *offset,
    sixel_kmeans_binning_mode mode,
    unsigned int binbits,
    sixel_kmeans_mapping_mode mapping_mode,
    sixel_kmeans_softdist_mode softdist_mode,
    sixel_allocator_t *allocator,
    double **compressed_samples_out,
    double **weights_out,
    unsigned int *compressed_count_out)
{
    SIXELSTATUS status;
    sixel_kmeans_histogram_t histogram;
    size_t expected_entries;
    unsigned int index;
    unsigned int channel;
    unsigned int output_index;
    double sample[3];
    double mapped[3];
    double *compressed_samples;
    double *weights;
    sixel_kmeans_bin_entry_t const *entry;

    status = SIXEL_FALSE;
    histogram.entries = NULL;
    histogram.capacity = 0u;
    histogram.mask = 0u;
    histogram.size = 0u;
    histogram.binbits = 0u;
    histogram.bin_count = 0u;
    expected_entries = 0u;
    index = 0u;
    channel = 0u;
    output_index = 0u;
    sample[0] = 0.0;
    sample[1] = 0.0;
    sample[2] = 0.0;
    mapped[0] = 0.0;
    mapped[1] = 0.0;
    mapped[2] = 0.0;
    compressed_samples = NULL;
    weights = NULL;
    entry = NULL;

    if (samples == NULL || allocator == NULL ||
            compressed_samples_out == NULL ||
            weights_out == NULL ||
            compressed_count_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *compressed_samples_out = NULL;
    *weights_out = NULL;
    *compressed_count_out = 0u;
    if (sample_count == 0u) {
        return SIXEL_OK;
    }

    expected_entries = (size_t)sample_count;
    if (mode == SIXEL_PALETTE_KMEANS_BINNING_SOFT) {
        if (expected_entries > SIZE_MAX / 8u) {
            expected_entries = SIZE_MAX / 8u;
        }
        expected_entries *= 8u;
    }

    status = sixel_kmeans_histogram_init(&histogram,
                                         expected_entries,
                                         binbits,
                                         allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    for (index = 0u; index < sample_count; ++index) {
        sample[0] = samples[(size_t)index * 3u + 0u];
        sample[1] = samples[(size_t)index * 3u + 1u];
        sample[2] = samples[(size_t)index * 3u + 2u];
        for (channel = 0u; channel < 3u; ++channel) {
            mapped[channel] = sixel_kmeans_map_sample_to_unit(
                sample[channel],
                input_is_float32,
                pixelformat,
                channel,
                scale,
                offset,
                mapping_mode);
        }
        if (mode == SIXEL_PALETTE_KMEANS_BINNING_SOFT) {
            if (softdist_mode == SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR) {
                status = sixel_kmeans_histogram_add_soft_trilinear(
                    &histogram,
                    sample,
                    mapped,
                    allocator);
            } else {
                status = SIXEL_BAD_ARGUMENT;
            }
        } else {
            status = sixel_kmeans_histogram_add_hard(
                &histogram,
                sample,
                mapped,
                allocator);
        }
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    compressed_samples = (double *)sixel_allocator_malloc(
        allocator, (size_t)histogram.size * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator, (size_t)histogram.size * sizeof(double));
    if (compressed_samples == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    output_index = 0u;
    for (index = 0u; index < histogram.capacity; ++index) {
        entry = histogram.entries + index;
        if (entry->key == UINT32_MAX || entry->weight <= 0.0) {
            continue;
        }
        weights[output_index] = entry->weight;
        for (channel = 0u; channel < 3u; ++channel) {
            compressed_samples[(size_t)output_index * 3u + channel]
                = entry->sum[channel] / entry->weight;
        }
        ++output_index;
    }

    *compressed_samples_out = compressed_samples;
    *weights_out = weights;
    *compressed_count_out = output_index;
    compressed_samples = NULL;
    weights = NULL;
    status = SIXEL_OK;

cleanup:
    if (compressed_samples != NULL) {
        sixel_allocator_free(allocator, compressed_samples);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    sixel_kmeans_histogram_dispose(&histogram, allocator);

    return status;
}

/*
 * Execute the full k-means clustering routine and return the generated palette
 * as a freshly allocated RGB array.  The implementation mirrors the previous
 * palette.c logic but is reorganised around clearly labelled segments:
 *
 *   - Sample ingestion: consume every pixel provided by the encoder's sampler
 *     so palette generation retains spatial coverage.
 *   - K-means++ seeding: initialise cluster centres in a distance-aware order.
 *   - Lloyd refinement: iterate until convergence or an iteration budget is
 *     reached.
 *   - Optional Ward/HK-means merge: reuse the palette final-merge utilities to
 *     trim excess clusters.
 *   - Palette export: copy the finished centroids into a compact RGB buffer.
 */
static SIXELSTATUS
build_palette_kmeans(unsigned char **result,
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
    unsigned int sample_cap;
    unsigned int valid_seen;
    unsigned int sample_count;
    unsigned int work_sample_count;
    unsigned int compressed_count;
    unsigned int k;
    unsigned int index;
    unsigned int channel;
    unsigned int center_index;
    unsigned int sample_index;
    unsigned int max_iterations;
    unsigned int iteration;
    unsigned int best_index;
    unsigned int old_cluster;
    unsigned int farthest_index;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    unsigned int base;
    unsigned int binbits;
    unsigned int autoratio;
    double removed_component;
    double sample_weight;
    double old_weight;
    double cluster_weight;
    double farthest_sample_weight;
    unsigned int unique_colors;
    unsigned int *membership;
    unsigned int *order;
    double *samples;
    double *work_samples;
    double *work_weights;
    double *compressed_samples;
    double *compressed_weights;
    unsigned char *palette;
    unsigned char *new_palette;
    double *centers;
    double *distance_cache;
    double best_distance;
    double distance;
    double diff;
    double update;
    double farthest_distance;
    double delta;
    double lloyd_threshold;
    double merge_component;
    double restored_component;
    double float32_channel_scale[3];
    double float32_channel_offset[3];
    double float32_lloyd_scale;
    double snapped_center[3];
    double previous_center[3];
    float *float_palette;
    float *float_palette_new;
    sixel_kmeans_init_type init_type;
    sixel_kmeans_binning_mode binning_mode;
    sixel_kmeans_binning_mode resolved_binning_mode;
    sixel_kmeans_mapping_mode mapping_mode;
    sixel_kmeans_softdist_mode softdist_mode;
    unsigned long *merge_weights;
    double *cluster_weights;
    double *accum;
    double *channel_sum;
    double *merge_sums;
    size_t farthest_base;
    unsigned char *unique_buffer;
    size_t unique_pixels;
    int apply_merge;
    int resolved_merge;
    unsigned int overshoot;
    unsigned int refine_iterations;
    int cluster_total;
    int unique_within;
    int input_is_;
    SIXELSTATUS unique_status;
    int job_init;
    int job_iteration;
    int job_merge;
    int job_export;
    char log_detail[128];
    double wall_start;
    double init_stop;
    double iterate_start;
    double iterate_stop;
    double iteration_wall_start;
    double iteration_wall_stop;
    double merge_start;
    double merge_stop;
    double export_start;
    double export_stop;
    unsigned int lloyd_iterations;
    unsigned int merge_iterations;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_stride = depth;
    pixel_count = 0U;
    sample_cap = 0U;
    valid_seen = 0U;
    sample_count = 0U;
    work_sample_count = 0U;
    compressed_count = 0U;
    k = 0U;
    index = 0U;
    channel = 0U;
    center_index = 0U;
    sample_index = 0U;
    max_iterations = 0U;
    iteration = 0U;
    best_index = 0U;
    old_cluster = 0U;
    farthest_index = 0U;
    fill = 0U;
    source = 0U;
    swap_temp = 0U;
    base = 0U;
    binbits = 0U;
    autoratio = 0U;
    removed_component = 0.0;
    sample_weight = 0.0;
    old_weight = 0.0;
    cluster_weight = 0.0;
    farthest_sample_weight = 0.0;
    unique_colors = 0U;
    membership = NULL;
    order = NULL;
    samples = NULL;
    work_samples = NULL;
    work_weights = NULL;
    compressed_samples = NULL;
    compressed_weights = NULL;
    palette = NULL;
    new_palette = NULL;
    centers = NULL;
    distance_cache = NULL;
    merge_weights = NULL;
    cluster_weights = NULL;
    accum = NULL;
    channel_sum = NULL;
    merge_sums = NULL;
    best_distance = 0.0;
    distance = 0.0;
    diff = 0.0;
    update = 0.0;
    farthest_distance = 0.0;
    farthest_base = 0U;
    delta = 0.0;
    lloyd_threshold = 0.0;
    merge_component = 0.0;
    restored_component = 0.0;
    unique_buffer = NULL;
    unique_pixels = 0U;
    apply_merge = 0;
    resolved_merge = SIXEL_FINAL_MERGE_NONE;
    overshoot = 0U;
    refine_iterations = 0U;
    cluster_total = 0;
    unique_within = 0;
    input_is_ = 0;
    unique_status = SIXEL_OK;
    job_init = -1;
    job_iteration = -1;
    job_merge = -1;
    job_export = -1;
    log_detail[0] = '\0';
    wall_start = sixel_timer_now();
    init_stop = wall_start;
    iterate_start = wall_start;
    iterate_stop = wall_start;
    iteration_wall_start = wall_start;
    iteration_wall_stop = wall_start;
    merge_start = wall_start;
    merge_stop = wall_start;
    export_start = wall_start;
    export_stop = wall_start;
    lloyd_iterations = 0U;
    merge_iterations = 0U;
    float32_channel_scale[0U] = 0.0;
    float32_channel_scale[1U] = 0.0;
    float32_channel_scale[2U] = 0.0;
    float32_channel_offset[0U] = 0.0;
    float32_channel_offset[1U] = 0.0;
    float32_channel_offset[2U] = 0.0;
    float32_lloyd_scale = 0.0;
    float_palette = NULL;
    float_palette_new = NULL;
    init_type = SIXEL_PALETTE_KMEANS_INIT_AUTO;
    binning_mode = SIXEL_PALETTE_KMEANS_BINNING_AUTO;
    resolved_binning_mode = SIXEL_PALETTE_KMEANS_BINNING_NONE;
    mapping_mode = SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM;
    softdist_mode = SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR;

    if (result != NULL) {
        *result = NULL;
    }
    if (result_float32 != NULL) {
        *result_float32 = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0U;
    }
    if (origcolors != NULL) {
        *origcolors = 0U;
    }
    if (allocator == NULL) {
        return status;
    }

    job_init = sixel_palette_kmeans_log_start(logger,
                                              job_seq,
                                              engine_name,
                                              "palette/init",
                                              "init");

    channels = depth;
    pixel_stride = depth;
    input_is_ = (treat_input_as_float32
                           && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat));
    if (input_is_) {
        for (channel = 0U; channel < 3U; ++channel) {
            float float_minimum;
            float float_maximum;
            double range;

#if HAVE_FLOAT_H
# define SIXEL_KMEANS_FLOAT_BOUND FLT_MAX
#else
# define SIXEL_KMEANS_FLOAT_BOUND 1.0e9f
#endif
            float_minimum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                -SIXEL_KMEANS_FLOAT_BOUND);
            float_maximum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                SIXEL_KMEANS_FLOAT_BOUND);
#undef SIXEL_KMEANS_FLOAT_BOUND
            range = (double)float_maximum - (double)float_minimum;
            if (range <= 0.0) {
                float32_channel_scale[channel] = 0.0;
                float32_channel_offset[channel] = 0.0;
                continue;
            }
            float32_channel_scale[channel] = 255.0 / range;
            float32_channel_offset[channel] =
                -((double)float_minimum) * float32_channel_scale[channel];
            if (float32_channel_scale[channel] > float32_lloyd_scale) {
                float32_lloyd_scale = float32_channel_scale[channel];
            }
        }
        if (depth == 0U || depth % (unsigned int)sizeof(float) != 0U) {
            return status;
        }
        channels = depth / (unsigned int)sizeof(float);
        pixel_stride = channels * (unsigned int)sizeof(float);
    }
    if (channels != 3U && channels != 4U) {
        return status;
    }
    if (pixel_stride == 0U) {
        return status;
    }
    pixel_count = length / pixel_stride;
    if (pixel_count == 0U) {
        status = SIXEL_OK;
        goto end;
    }

    /*
     * The encoder performs spatial sampling before invoking the palette
     * builder.  Consume every surviving pixel so k-means operates on the
     * caller's full reservoir instead of applying a second stage of
     * reservoir sampling here.
     */
    sample_cap = pixel_count;
    samples = (double *)sixel_allocator_malloc(
        allocator, (size_t)sample_cap * 3U * sizeof(double));
    if (samples == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    valid_seen = 0U;
    sample_count = 0U;
    for (index = 0U; index < pixel_count; ++index) {
        base = index * pixel_stride;
        if (input_is_) {
            float const *fpixels;

            fpixels = (float const *)(void const *)(data + base);
            if (channels == 4U
                && !sixel_palette_float32_alpha_visible(
                       (double)fpixels[3U])) {
                continue;
            }
            ++valid_seen;
            if (sample_count < sample_cap) {
                for (channel = 0U; channel < 3U; ++channel) {
                    samples[sample_count * 3U + channel] =
                        (double)fpixels[channel];
                }
                ++sample_count;
            }
        } else {
            if (channels == 4U && data[base + 3U] == 0U) {
                continue;
            }
            ++valid_seen;
            if (sample_count < sample_cap) {
                for (channel = 0U; channel < 3U; ++channel) {
                    samples[sample_count * 3U + channel] =
                        (double)data[base + channel];
                }
                ++sample_count;
            }
        }
    }

    if (origcolors != NULL) {
        *origcolors = valid_seen;
    }
    if (sample_count == 0U) {
        goto end;
    }

    if (reqcolors == 0U) {
        reqcolors = 1U;
    }
    binning_mode = sixel_get_kmeans_binning_mode();
    binbits = sixel_get_kmeans_binbits();
    mapping_mode = sixel_get_kmeans_mapping_mode();
    softdist_mode = sixel_get_kmeans_softdist_mode();
    autoratio = sixel_get_kmeans_autoratio();
    resolved_binning_mode = sixel_kmeans_resolve_binning_for_input(
        binning_mode,
        sample_count,
        reqcolors,
        autoratio);
    work_samples = samples;
    work_weights = NULL;
    work_sample_count = sample_count;
    if (resolved_binning_mode == SIXEL_PALETTE_KMEANS_BINNING_HARD
            || resolved_binning_mode == SIXEL_PALETTE_KMEANS_BINNING_SOFT) {
        status = sixel_kmeans_build_weighted_histogram(
            samples,
            sample_count,
            input_is_,
            pixelformat,
            float32_channel_scale,
            float32_channel_offset,
            resolved_binning_mode,
            binbits,
            mapping_mode,
            softdist_mode,
            allocator,
            &compressed_samples,
            &compressed_weights,
            &compressed_count);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (compressed_count == 0U) {
            goto end;
        }
        work_samples = compressed_samples;
        work_weights = compressed_weights;
        work_sample_count = compressed_count;
    }

    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    if (apply_merge) {
        if (input_is_) {
            unique_buffer = (unsigned char *)sixel_allocator_malloc(
                allocator, (size_t)pixel_count * 3U);
            if (unique_buffer == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            unique_pixels = 0U;
            for (index = 0U; index < pixel_count; ++index) {
                float const *fpixels;

                base = index * pixel_stride;
                fpixels = (float const *)(void const *)(data + base);
                if (channels == 4U
                    && !sixel_palette_float32_alpha_visible(
                           (double)fpixels[3U])) {
                    continue;
                }
                for (channel = 0U; channel < 3U; ++channel) {
                    unique_buffer[unique_pixels * 3U + channel] =
                        sixel_pixelformat_float_channel_to_byte(
                            pixelformat,
                            (int)channel,
                            fpixels[channel]);
                }
                ++unique_pixels;
            }
            unique_status = sixel_palette_count_unique_within_limit(
                unique_buffer,
                (unsigned int)(unique_pixels * 3U),
                3U,
                reqcolors,
                &unique_colors,
                &unique_within,
                allocator);
        } else {
            unique_status = sixel_palette_count_unique_within_limit(
                data,
                length,
                channels,
                reqcolors,
                &unique_colors,
                &unique_within,
                allocator);
        }
        if (unique_status == SIXEL_OK && unique_within != 0) {
            apply_merge = 0;
        }
    }
    overshoot = reqcolors;
    if (apply_merge) {
        sixel_final_merge_load_env();
        refine_iterations =
            sixel_final_merge_lloyd_iterations(resolved_merge);
        overshoot = sixel_final_merge_target(reqcolors, resolved_merge);
        sixel_debugf("overshoot: %u", overshoot);
    }
    if (overshoot > work_sample_count) {
        overshoot = work_sample_count;
    }
    k = overshoot;
    if (k == 0U) {
        goto end;
    }

    centers = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(double));
    distance_cache = (double *)sixel_allocator_malloc(
        allocator, (size_t)work_sample_count * sizeof(double));
    cluster_weights = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * sizeof(double));
    accum = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(double));
    membership = (unsigned int *)sixel_allocator_malloc(
        allocator, (size_t)work_sample_count * sizeof(unsigned int));
    if (centers == NULL || distance_cache == NULL || cluster_weights == NULL
            || accum == NULL || membership == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    init_type = sixel_get_kmeans_init_type();
    status = sixel_kmeans_choose_initial_centroids(centers,
                                                   k,
                                                   work_samples,
                                                   work_weights,
                                                   work_sample_count,
                                                   use_reversible,
                                                   pixelformat,
                                                   distance_cache,
                                                   allocator,
                                                   init_type);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        max_iterations = 6U;
        break;
    case SIXEL_QUALITY_HIGH:
        max_iterations = 24U;
        break;
    case SIXEL_QUALITY_FULL:
        max_iterations = 48U;
        break;
    case SIXEL_QUALITY_HIGHCOLOR:
        max_iterations = 24U;
        break;
    case SIXEL_QUALITY_AUTO:
    default:
        max_iterations = 12U;
        break;
    }
    if (max_iterations == 0U) {
        max_iterations = 1U;
    }
    {
        unsigned int iter_cap;

        iter_cap = sixel_palette_kmeans_iter_max();
        if (max_iterations > iter_cap) {
            max_iterations = iter_cap;
        }
        if (max_iterations == 0U) {
            max_iterations = 1U;
        }
        lloyd_threshold = sixel_palette_kmeans_threshold();
        if (input_is_ && float32_lloyd_scale > 0.0) {
            double threshold_scale;

            threshold_scale = float32_lloyd_scale * float32_lloyd_scale;
            lloyd_threshold /= threshold_scale;
        }
    }
    init_stop = sixel_timer_now();
    iterate_start = init_stop;
    (void)snprintf(log_detail,
                   sizeof(log_detail),
                   "samples=%u k=%u init=%s",
                   work_sample_count,
                   k,
                   sixel_kmeans_init_type_to_string(init_type));
    sixel_palette_kmeans_log_finish(logger,
                                    job_init,
                                    engine_name,
                                    "palette/init",
                                    "init",
                                    log_detail);
    for (iteration = 0U; iteration < max_iterations; ++iteration) {
        iteration_wall_start = sixel_timer_now();
        if (lloyd_iterations == 0U) {
            iterate_start = iteration_wall_start;
        }
        ++lloyd_iterations;
        job_iteration = sixel_palette_kmeans_log_start(logger,
                                                       job_seq,
                                                       engine_name,
                                                       "palette/iterate",
                                                       "iterate");
        for (index = 0U; index < k; ++index) {
            cluster_weights[index] = 0.0;
        }
        for (index = 0U; index < k * 3U; ++index) {
            accum[index] = 0.0;
        }
        for (sample_index = 0U; sample_index < work_sample_count;
                ++sample_index) {
            sample_weight = 1.0;
            if (work_weights != NULL) {
                sample_weight = work_weights[sample_index];
            }
            if (sample_weight <= 0.0) {
                distance_cache[sample_index] = 0.0;
                membership[sample_index] = 0U;
                continue;
            }
            best_index = 0U;
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)work_samples[sample_index * 3U + channel]
                    - centers[channel];
                distance += diff * diff;
            }
            best_distance = distance;
            for (center_index = 1U; center_index < k;
                    ++center_index) {
                distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)work_samples[sample_index * 3U + channel]
                        - centers[center_index * 3U + channel];
                    distance += diff * diff;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = center_index;
                }
            }
            membership[sample_index] = best_index;
            distance_cache[sample_index] = best_distance * sample_weight;
            cluster_weights[best_index] += sample_weight;
            channel_sum = accum + (size_t)best_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] +=
                    work_samples[sample_index * 3U + channel] * sample_weight;
            }
        }
        for (center_index = 0U; center_index < k; ++center_index) {
            if (cluster_weights[center_index] > 0.0) {
                continue;
            }
            farthest_distance = -1.0;
            farthest_index = 0U;
            for (sample_index = 0U; sample_index < work_sample_count;
                    ++sample_index) {
                if (distance_cache[sample_index] > farthest_distance) {
                    farthest_distance = distance_cache[sample_index];
                    farthest_index = sample_index;
                }
            }
            old_cluster = membership[farthest_index];
            farthest_base = (size_t)farthest_index * 3U;
            farthest_sample_weight = 1.0;
            if (work_weights != NULL) {
                farthest_sample_weight = work_weights[farthest_index];
            }
            old_weight = cluster_weights[old_cluster];
            if (old_weight > 0.0) {
                channel_sum = accum + (size_t)old_cluster * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    removed_component =
                        work_samples[farthest_base + channel]
                        * farthest_sample_weight;
                    channel_sum[channel] -= removed_component;
                    if (channel_sum[channel] < 0.0) {
                        channel_sum[channel] = 0.0;
                    }
                }
                old_weight -= farthest_sample_weight;
                if (old_weight < 0.0) {
                    old_weight = 0.0;
                }
                cluster_weights[old_cluster] = old_weight;
            }
            membership[farthest_index] = center_index;
            cluster_weights[center_index] = farthest_sample_weight;
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] =
                    work_samples[farthest_base + channel]
                    * farthest_sample_weight;
            }
            distance_cache[farthest_index] = 0.0;
        }
        delta = 0.0;
        for (center_index = 0U; center_index < k; ++center_index) {
            cluster_weight = cluster_weights[center_index];
            if (cluster_weight <= 0.0) {
                continue;
            }
            /*
             * Record the previous centre so the Lloyd delta measures the
             * snapped position rather than the unclamped average.
             */
            for (channel = 0U; channel < 3U; ++channel) {
                previous_center[channel]
                    = centers[center_index * 3U + channel];
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                snapped_center[channel] = channel_sum[channel]
                    / cluster_weight;
            }
            sixel_palette_snap_triple(snapped_center,
                                      use_reversible,
                                      pixelformat,
                                      SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
            for (channel = 0U; channel < 3U; ++channel) {
                diff = previous_center[channel] - snapped_center[channel];
                delta += diff * diff;
                centers[center_index * 3U + channel] = snapped_center[channel];
            }
        }
        if (delta <= lloyd_threshold) {
            iteration_wall_stop = sixel_timer_now();
            iterate_stop = iteration_wall_stop;
            (void)snprintf(log_detail,
                           sizeof(log_detail),
                           "iter=%u delta=%.4f threshold=%.4f",
                           lloyd_iterations,
                           delta,
                           lloyd_threshold);
            sixel_palette_kmeans_log_finish(logger,
                                            job_iteration,
                                            engine_name,
                                            "palette/iterate",
                                            "iterate",
                                            log_detail);
            break;
        }
        iteration_wall_stop = sixel_timer_now();
        iterate_stop = iteration_wall_stop;
        (void)snprintf(log_detail,
                       sizeof(log_detail),
                       "iter=%u delta=%.4f threshold=%.4f",
                       lloyd_iterations,
                       delta,
                       lloyd_threshold);
        sixel_palette_kmeans_log_finish(logger,
                                        job_iteration,
                                        engine_name,
                                        "palette/iterate",
                                        "iterate",
                                        log_detail);
    }
    merge_start = iterate_stop;
    merge_stop = iterate_stop;
    if (apply_merge && k > reqcolors) {
        merge_start = sixel_timer_now();
        job_merge = sixel_palette_kmeans_log_start(logger,
                                                   job_seq,
                                                   engine_name,
                                                   "palette/merge",
                                                   "merge");
        /*
         * Preserve fractional channel contributions while still sharing the
         * final merge code path that expects 0-255 scaled sums.  We convert
         * float samples into the 0-255 domain here and convert them back after
         * the merge completed.
         */
        merge_sums = (double *)sixel_allocator_malloc(
            allocator, (size_t)k * 3U * sizeof(double));
        merge_weights = (unsigned long *)sixel_allocator_malloc(
            allocator, (size_t)k * sizeof(unsigned long));
        if (merge_sums == NULL || merge_weights == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k; ++index) {
            cluster_weight = cluster_weights[index];
            if (cluster_weight <= 0.0) {
                merge_weights[index] = 1UL;
                merge_sums[index * 3U + 0U] = 0.0;
                merge_sums[index * 3U + 1U] = 0.0;
                merge_sums[index * 3U + 2U] = 0.0;
                continue;
            }
            if (cluster_weight > (double)ULONG_MAX) {
                merge_weights[index] = ULONG_MAX;
            } else {
                merge_weights[index] = (unsigned long)(cluster_weight + 0.5);
                if (merge_weights[index] == 0UL) {
                    merge_weights[index] = 1UL;
                }
            }
            for (channel = 0U; channel < 3U; ++channel) {
                merge_component = accum[index * 3U + channel] / cluster_weight;
                if (input_is_) {
                    merge_component = sixel_palette_kmeans_sum_float_to_byte(
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
                merge_sums[index * 3U + channel]
                    = merge_component * (double)merge_weights[index];
            }
        }
        cluster_total = sixel_palette_apply_merge(merge_weights,
                                                  merge_sums,
                                                  3U,
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
        k = (unsigned int)cluster_total;
        if (k == 0U) {
            k = 1U;
        }
        for (index = 0U; index < k; ++index) {
            cluster_weight = (double)merge_weights[index];
            if (cluster_weight < 1.0) {
                cluster_weight = 1.0;
            }
            cluster_weights[index] = cluster_weight;
            for (channel = 0U; channel < 3U; ++channel) {
                restored_component = merge_sums[index * 3U + channel];
                if (input_is_) {
                    restored_component = sixel_palette_kmeans_sum_byte_to_float(
                        restored_component,
                        cluster_weight,
                        channel,
                        float32_channel_scale,
                        float32_channel_offset);
                }
                accum[index * 3U + channel] = restored_component;
            }
        }
        sixel_allocator_free(allocator, merge_sums);
        merge_sums = NULL;
        sixel_allocator_free(allocator, merge_weights);
        merge_weights = NULL;
        for (center_index = 0U; center_index < k; ++center_index) {
            cluster_weight = cluster_weights[center_index];
            if (cluster_weight <= 0.0) {
                cluster_weight = 1.0;
                cluster_weights[center_index] = cluster_weight;
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                centers[center_index * 3U + channel] =
                    (double)channel_sum[channel]
                    / cluster_weight;
            }
        }
        for (iteration = 0U; iteration < refine_iterations; ++iteration) {
            ++merge_iterations;
            for (index = 0U; index < k; ++index) {
                cluster_weights[index] = 0.0;
            }
            for (index = 0U; index < k * 3U; ++index) {
                accum[index] = 0.0;
            }
            for (sample_index = 0U; sample_index < work_sample_count;
                    ++sample_index) {
                sample_weight = 1.0;
                if (work_weights != NULL) {
                    sample_weight = work_weights[sample_index];
                }
                if (sample_weight <= 0.0) {
                    distance_cache[sample_index] = 0.0;
                    membership[sample_index] = 0U;
                    continue;
                }
                best_index = 0U;
                best_distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)work_samples[sample_index * 3U + channel]
                        - centers[channel];
                    best_distance += diff * diff;
                }
                for (center_index = 1U; center_index < k;
                        ++center_index) {
                    distance = 0.0;
                    for (channel = 0U; channel < 3U; ++channel) {
                        diff = (double)work_samples[sample_index * 3U + channel]
                            - centers[center_index * 3U + channel];
                        distance += diff * diff;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = center_index;
                    }
                }
                membership[sample_index] = best_index;
                distance_cache[sample_index] = best_distance * sample_weight;
                cluster_weights[best_index] += sample_weight;
                channel_sum = accum + (size_t)best_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] +=
                        work_samples[sample_index * 3U + channel]
                        * sample_weight;
                }
            }
            for (center_index = 0U; center_index < k; ++center_index) {
                if (cluster_weights[center_index] > 0.0) {
                    continue;
                }
                farthest_distance = -1.0;
                farthest_index = 0U;
                for (sample_index = 0U; sample_index < work_sample_count;
                        ++sample_index) {
                    if (distance_cache[sample_index] > farthest_distance) {
                        farthest_distance = distance_cache[sample_index];
                        farthest_index = sample_index;
                    }
                }
                old_cluster = membership[farthest_index];
                farthest_base = (size_t)farthest_index * 3U;
                farthest_sample_weight = 1.0;
                if (work_weights != NULL) {
                    farthest_sample_weight = work_weights[farthest_index];
                }
                old_weight = cluster_weights[old_cluster];
                if (old_weight > 0.0) {
                    channel_sum = accum + (size_t)old_cluster * 3U;
                    for (channel = 0U; channel < 3U; ++channel) {
                        removed_component =
                            work_samples[farthest_base + channel]
                            * farthest_sample_weight;
                        channel_sum[channel] -= removed_component;
                        if (channel_sum[channel] < 0.0) {
                            channel_sum[channel] = 0.0;
                        }
                    }
                    old_weight -= farthest_sample_weight;
                    if (old_weight < 0.0) {
                        old_weight = 0.0;
                    }
                    cluster_weights[old_cluster] = old_weight;
                }
                membership[farthest_index] = center_index;
                cluster_weights[center_index] = farthest_sample_weight;
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] =
                        work_samples[farthest_base + channel]
                        * farthest_sample_weight;
                }
                distance_cache[farthest_index] = 0.0;
            }
            delta = 0.0;
            for (center_index = 0U; center_index < k; ++center_index) {
                cluster_weight = cluster_weights[center_index];
                if (cluster_weight <= 0.0) {
                    continue;
                }
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    update = channel_sum[channel]
                        / cluster_weight;
                    diff = centers[center_index * 3U + channel] - update;
                    delta += diff * diff;
                    centers[center_index * 3U + channel] = update;
                }
            }
            if (delta <= lloyd_threshold) {
                break;
            }
        }
    }

    merge_stop = sixel_timer_now();
    if (job_merge >= 0) {
        (void)snprintf(log_detail,
                       sizeof(log_detail),
                       "clusters=%u refine=%u merge=%d",
                       k,
                       merge_iterations,
                       resolved_merge);
        sixel_palette_kmeans_log_finish(logger,
                                        job_merge,
                                        engine_name,
                                        "palette/merge",
                                        "merge",
                                        log_detail);
    }
    export_start = sixel_timer_now();
    job_export = sixel_palette_kmeans_log_start(logger,
                                                job_seq,
                                                engine_name,
                                                "palette/export",
                                                "export");

    palette = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U);
    if (palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (result_float32 != NULL && input_is_ && k > 0U) {
        float_palette = (float *)sixel_allocator_malloc(
            allocator, (size_t)k * 3U * sizeof(float));
        if (float_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    for (center_index = 0U; center_index < k; ++center_index) {
        for (channel = 0U; channel < 3U; ++channel) {
            update = centers[center_index * 3U + channel];
            if (float_palette != NULL) {
                float clamped;

                clamped = sixel_pixelformat_float_channel_clamp(
                    pixelformat,
                    (int)channel,
                    (float)update);
                float_palette[center_index * 3U + channel] = clamped;
            }
            if (input_is_) {
                update = (double)sixel_pixelformat_float_channel_to_byte(
                    pixelformat,
                    (int)channel,
                    (float)update);
            }
            if (update < 0.0) {
                update = 0.0;
            }
            if (update > 255.0) {
                update = 255.0;
            }
            palette[center_index * 3U + channel] =
                (unsigned char)(update + 0.5);
        }
    }

    if (force_palette && k < reqcolors) {
        new_palette = (unsigned char *)sixel_allocator_malloc(
            allocator, (size_t)reqcolors * 3U);
        if (new_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (float_palette != NULL) {
            float_palette_new = (float *)sixel_allocator_malloc(
                allocator, (size_t)reqcolors * 3U * sizeof(float));
            if (float_palette_new == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            for (index = 0U; index < k * 3U; ++index) {
                float_palette_new[index] = float_palette[index];
            }
        }
        for (index = 0U; index < k * 3U; ++index) {
            new_palette[index] = palette[index];
        }
        order = (unsigned int *)sixel_allocator_malloc(
            allocator, (size_t)k * sizeof(unsigned int));
        if (order == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k; ++index) {
            order[index] = index;
        }
        for (index = 0U; index < k; ++index) {
            for (center_index = index + 1U; center_index < k;
                    ++center_index) {
                if (cluster_weights[order[center_index]]
                        > cluster_weights[order[index]]) {
                    swap_temp = order[index];
                    order[index] = order[center_index];
                    order[center_index] = swap_temp;
                }
            }
        }
        fill = k;
        source = 0U;
        while (fill < reqcolors && k > 0U) {
            center_index = order[source];
            for (channel = 0U; channel < 3U; ++channel) {
                new_palette[fill * 3U + channel] =
                    palette[center_index * 3U + channel];
            }
            if (float_palette_new != NULL) {
                float_palette_new[fill * 3U + 0U] =
                    float_palette[center_index * 3U + 0U];
                float_palette_new[fill * 3U + 1U] =
                    float_palette[center_index * 3U + 1U];
                float_palette_new[fill * 3U + 2U] =
                    float_palette[center_index * 3U + 2U];
            }
            ++fill;
            ++source;
            if (source >= k) {
                source = 0U;
            }
        }
        sixel_allocator_free(allocator, palette);
        palette = new_palette;
        new_palette = NULL;
        if (float_palette_new != NULL) {
            sixel_allocator_free(allocator, float_palette);
            float_palette = float_palette_new;
            float_palette_new = NULL;
        }
        k = reqcolors;
    }

    status = SIXEL_OK;
    if (result != NULL) {
        *result = palette;
    } else {
        palette = NULL;
    }
    if (result_float32 != NULL) {
        if (float_palette != NULL) {
            *result_float32 = float_palette;
            float_palette = NULL;
        } else {
            *result_float32 = NULL;
        }
    }
    if (ncolors != NULL) {
        *ncolors = k;
    }

    export_stop = sixel_timer_now();
    (void)snprintf(log_detail,
                   sizeof(log_detail),
                   "colors=%u merge=%d",
                   k,
                   resolved_merge);
    sixel_palette_kmeans_log_finish(logger,
                                    job_export,
                                    engine_name,
                                    "palette/export",
                                    "export",
                                    log_detail);

end:
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
        telemetry->iterate_count = lloyd_iterations;
        telemetry->merge_iterate_count = merge_iterations;
        telemetry->merge_mode = apply_merge ? resolved_merge
                                            : SIXEL_FINAL_MERGE_NONE;
    }

    if (status != SIXEL_OK && palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (new_palette != NULL) {
        sixel_allocator_free(allocator, new_palette);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (membership != NULL) {
        sixel_allocator_free(allocator, membership);
    }
    if (accum != NULL) {
        sixel_allocator_free(allocator, accum);
    }
    if (merge_weights != NULL) {
        sixel_allocator_free(allocator, merge_weights);
    }
    if (cluster_weights != NULL) {
        sixel_allocator_free(allocator, cluster_weights);
    }
    if (distance_cache != NULL) {
        sixel_allocator_free(allocator, distance_cache);
    }
    if (centers != NULL) {
        sixel_allocator_free(allocator, centers);
    }
    if (samples != NULL) {
        sixel_allocator_free(allocator, samples);
    }
    if (compressed_samples != NULL) {
        sixel_allocator_free(allocator, compressed_samples);
    }
    if (compressed_weights != NULL) {
        sixel_allocator_free(allocator, compressed_weights);
    }
    if (merge_sums != NULL) {
        sixel_allocator_free(allocator, merge_sums);
    }
    if (unique_buffer != NULL) {
        sixel_allocator_free(allocator, unique_buffer);
    }
    if (float_palette != NULL) {
        sixel_allocator_free(allocator, float_palette);
    }
    if (float_palette_new != NULL) {
        sixel_allocator_free(allocator, float_palette_new);
    }
    return status;
}

/*
 * Public entry point used by palette.c.  The function wraps
 * build_palette_kmeans and writes the resulting palette into the provided
 * sixel_palette_t instance.  The exported interface therefore mirrors the
 * median-cut builder and keeps the orchestrator agnostic of
 * algorithm-specific memory juggling.
 */
static SIXELSTATUS
sixel_palette_build_kmeans_internal(sixel_palette_t *palette,
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
    SIXELSTATUS build_status;
    sixel_allocator_t *work_allocator;
    unsigned char *entries;
    float *entries_float32;
    unsigned int ncolors;
    unsigned int origcolors;
    unsigned int input_depth;
    unsigned int entry_depth;
    int depth_result;
    size_t payload_size;
    int reversible_for_quantizer;

    status = SIXEL_BAD_ARGUMENT;
    build_status = SIXEL_FALSE;
    work_allocator = allocator;
    entries = NULL;
    entries_float32 = NULL;
    ncolors = 0U;
    origcolors = 0U;
    input_depth = 0U;
    entry_depth = 0U;
    depth_result = 0;
    payload_size = 0U;

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
            "sixel_palette_build_kmeans: invalid pixel format depth.");
        return status;
    }
    input_depth = (unsigned int)depth_result;

    /*
     * Palette objects keep their 8bit representation in RGB triplets so the
     * downstream dithering code can continue using historical assumptions.
     * When the source pixels arrive as RGBFLOAT32 we stash the float copy
     * separately, therefore the entry depth always follows RGB888.
     */
    depth_result = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kmeans: rgb888 depth lookup failed.");
        return status;
    }
    entry_depth = (unsigned int)depth_result;

    reversible_for_quantizer = palette->use_reversible;
    build_status = build_palette_kmeans(&entries,
                                        &entries_float32,
                                        data,
                                        length,
                                        input_depth,
                                        palette->requested_colors,
                                        &ncolors,
                                        &origcolors,
                                        palette->quality_mode,
                                        palette->force_palette,
                                        reversible_for_quantizer,
                                        palette->final_merge_mode,
                                        work_allocator,
                                        pixelformat,
                                        treat_input_as_float32,
                                        logger,
                                        job_seq,
                                        engine_name,
                                        telemetry);
    if (SIXEL_FAILED(build_status)) {
        status = build_status;
        goto end;
    }

    if (reversible_for_quantizer) {
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
    if (payload_size > 0U && palette->entries != NULL) {
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
            (int)(3U * (unsigned int)sizeof(float)),
            work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        status = sixel_palette_set_entries_float32(palette,
                                                   NULL,
                                                   0U,
                                                   0,
                                                   work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    if (entries != NULL) {
        sixel_allocator_free(work_allocator, entries);
    }
    if (entries_float32 != NULL) {
        sixel_allocator_free(work_allocator, entries_float32);
    }
    return status;
}

SIXELSTATUS
sixel_palette_build_kmeans(sixel_palette_t *palette,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           sixel_allocator_t *allocator,
                           sixel_logger_t *logger,
                           int *job_seq,
                           char const *engine_name,
                           sixel_palette_telemetry_t *telemetry)
{
    return sixel_palette_build_kmeans_internal(palette,
                                               (unsigned char const *)data,
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
sixel_palette_build_kmeans_float32(sixel_palette_t *palette,
                                   float const *data,
                                   unsigned int length,
                                   int pixelformat,
                                   sixel_allocator_t *allocator,
                                   sixel_logger_t *logger,
                                   int *job_seq,
                                   char const *engine_name,
                                   sixel_palette_telemetry_t *telemetry)
{
    return sixel_palette_build_kmeans_internal(palette,
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
