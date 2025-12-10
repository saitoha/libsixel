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

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

sixel_kmeans_init_type
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
                         unsigned int sample_count,
                         double *distance_cache)
{
    unsigned int channel;
    unsigned int index;
    unsigned int sample_index;
    unsigned int center_index;
    unsigned int replace;
    double total_weight;
    double random_point;
    double distance;
    double diff;
    unsigned long rand_value;

    channel = 0U;
    index = 0U;
    sample_index = 0U;
    center_index = 0U;
    replace = 0U;
    total_weight = 0.0;
    random_point = 0.0;
    distance = 0.0;
    diff = 0.0;
    rand_value = (unsigned long)rand();
    replace = (unsigned int)(rand_value % sample_count);
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
            total_weight += distance_cache[sample_index];
        }
        random_point = 0.0;
        if (total_weight > 0.0) {
            random_point = ((double)rand() / ((double)RAND_MAX + 1.0))
                * total_weight;
        }
        sample_index = 0U;
        while (sample_index + 1U < sample_count &&
               random_point > distance_cache[sample_index]) {
            random_point -= distance_cache[sample_index];
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
                                       unsigned long sample_total,
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
                                       unsigned long sample_total,
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
    double removed_component;
    unsigned int unique_colors;
    unsigned int *membership;
    unsigned int *order;
    double *samples;
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
    double float32_channel_scale[3];
    double float32_channel_offset[3];
    double float32_lloyd_scale;
    double snapped_center[3];
    double previous_center[3];
    float *float_palette;
    float *float_palette_new;
    sixel_kmeans_init_type init_type;
    unsigned long *counts;
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
    removed_component = 0.0;
    unique_colors = 0U;
    membership = NULL;
    order = NULL;
    samples = NULL;
    palette = NULL;
    new_palette = NULL;
    centers = NULL;
    distance_cache = NULL;
    counts = NULL;
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
    wall_start = sixel_assessment_timer_now();
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
    if (overshoot > sample_count) {
        overshoot = sample_count;
    }
    k = overshoot;
    if (k == 0U) {
        goto end;
    }

    centers = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(double));
    distance_cache = (double *)sixel_allocator_malloc(
        allocator, (size_t)sample_count * sizeof(double));
    counts = (unsigned long *)sixel_allocator_malloc(
        allocator, (size_t)k * sizeof(unsigned long));
    accum = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(double));
    membership = (unsigned int *)sixel_allocator_malloc(
        allocator, (size_t)sample_count * sizeof(unsigned int));
    if (centers == NULL || distance_cache == NULL || counts == NULL
            || accum == NULL || membership == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    init_type = sixel_get_kmeans_init_type();
    status = sixel_kmeans_choose_initial_centroids(centers,
                                                   k,
                                                   samples,
                                                   NULL,
                                                   sample_count,
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
    init_stop = sixel_assessment_timer_now();
    iterate_start = init_stop;
    (void)snprintf(log_detail,
                   sizeof(log_detail),
                   "samples=%u k=%u init=%s",
                   sample_count,
                   k,
                   sixel_kmeans_init_type_to_string(init_type));
    sixel_palette_kmeans_log_finish(logger,
                                    job_init,
                                    engine_name,
                                    "palette/init",
                                    "init",
                                    log_detail);
    for (iteration = 0U; iteration < max_iterations; ++iteration) {
        iteration_wall_start = sixel_assessment_timer_now();
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
            counts[index] = 0UL;
        }
        for (index = 0U; index < k * 3U; ++index) {
            accum[index] = 0.0;
        }
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            best_index = 0U;
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[sample_index * 3U + channel]
                    - centers[channel];
                distance += diff * diff;
            }
            best_distance = distance;
            for (center_index = 1U; center_index < k;
                    ++center_index) {
                distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)samples[sample_index * 3U + channel]
                        - centers[center_index * 3U + channel];
                    distance += diff * diff;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = center_index;
                }
            }
            membership[sample_index] = best_index;
            distance_cache[sample_index] = best_distance;
            counts[best_index] += 1UL;
            channel_sum = accum + (size_t)best_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] +=
                    samples[sample_index * 3U + channel];
            }
        }
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] != 0UL) {
                continue;
            }
            farthest_distance = -1.0;
            farthest_index = 0U;
            for (sample_index = 0U; sample_index < sample_count;
                    ++sample_index) {
                if (distance_cache[sample_index] > farthest_distance) {
                    farthest_distance = distance_cache[sample_index];
                    farthest_index = sample_index;
                }
            }
            old_cluster = membership[farthest_index];
            farthest_base = (size_t)farthest_index * 3U;
            if (counts[old_cluster] > 0UL) {
                counts[old_cluster] -= 1UL;
                channel_sum = accum + (size_t)old_cluster * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    removed_component =
                        samples[farthest_base + channel];
                    channel_sum[channel] -= removed_component;
                    if (channel_sum[channel] < 0.0) {
                        channel_sum[channel] = 0.0;
                    }
                }
            }
            membership[farthest_index] = center_index;
            counts[center_index] = 1UL;
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] =
                    samples[farthest_base + channel];
            }
            distance_cache[farthest_index] = 0.0;
        }
        delta = 0.0;
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] == 0UL) {
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
                    / (double)counts[center_index];
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
            iteration_wall_stop = sixel_assessment_timer_now();
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
        iteration_wall_stop = sixel_assessment_timer_now();
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
        merge_start = sixel_assessment_timer_now();
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
        if (merge_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k * 3U; ++index) {
            double component;
            unsigned int merge_channel;
            unsigned int merge_cluster;

            component = accum[index];
            merge_channel = index % 3U;
            merge_cluster = index / 3U;
            if (input_is_) {
                component = sixel_palette_kmeans_sum_float_to_byte(
                    component,
                    counts[merge_cluster],
                    merge_channel,
                    float32_channel_scale,
                    float32_channel_offset);
            }
            if (component < 0.0) {
                component = 0.0;
            }
            merge_sums[index] = component;
        }
        cluster_total = sixel_palette_apply_merge(counts,
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
        for (index = 0U; index < k * 3U; ++index) {
            double restored;
            unsigned int merge_channel;
            unsigned int merge_cluster;

            /* Translate merged 0-255 sums back to the original sample scale */
            restored = merge_sums[index];
            merge_channel = index % 3U;
            merge_cluster = index / 3U;
            if (input_is_) {
                restored = sixel_palette_kmeans_sum_byte_to_float(
                    restored,
                    counts[merge_cluster],
                    merge_channel,
                    float32_channel_scale,
                    float32_channel_offset);
            }
            accum[index] = restored;
        }
        sixel_allocator_free(allocator, merge_sums);
        merge_sums = NULL;
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] == 0UL) {
                counts[center_index] = 1UL;
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                centers[center_index * 3U + channel] =
                    (double)channel_sum[channel]
                    / (double)counts[center_index];
            }
        }
        for (iteration = 0U; iteration < refine_iterations; ++iteration) {
            ++merge_iterations;
            for (index = 0U; index < k; ++index) {
                counts[index] = 0UL;
            }
            for (index = 0U; index < k * 3U; ++index) {
                accum[index] = 0.0;
            }
            for (sample_index = 0U; sample_index < sample_count;
                    ++sample_index) {
                best_index = 0U;
                best_distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)samples[sample_index * 3U + channel]
                        - centers[channel];
                    best_distance += diff * diff;
                }
                for (center_index = 1U; center_index < k;
                        ++center_index) {
                    distance = 0.0;
                    for (channel = 0U; channel < 3U; ++channel) {
                        diff = (double)samples[sample_index * 3U + channel]
                            - centers[center_index * 3U + channel];
                        distance += diff * diff;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = center_index;
                    }
                }
                membership[sample_index] = best_index;
                distance_cache[sample_index] = best_distance;
                counts[best_index] += 1UL;
                channel_sum = accum + (size_t)best_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] +=
                        samples[sample_index * 3U + channel];
                }
            }
            for (center_index = 0U; center_index < k; ++center_index) {
                if (counts[center_index] != 0UL) {
                    continue;
                }
                farthest_distance = -1.0;
                farthest_index = 0U;
                for (sample_index = 0U; sample_index < sample_count;
                        ++sample_index) {
                    if (distance_cache[sample_index] > farthest_distance) {
                        farthest_distance = distance_cache[sample_index];
                        farthest_index = sample_index;
                    }
                }
                old_cluster = membership[farthest_index];
                farthest_base = (size_t)farthest_index * 3U;
                if (counts[old_cluster] > 0UL) {
                    counts[old_cluster] -= 1UL;
                    channel_sum = accum + (size_t)old_cluster * 3U;
                    for (channel = 0U; channel < 3U; ++channel) {
                        removed_component =
                            samples[farthest_base + channel];
                        channel_sum[channel] -= removed_component;
                        if (channel_sum[channel] < 0.0) {
                            channel_sum[channel] = 0.0;
                        }
                    }
                }
                membership[farthest_index] = center_index;
                counts[center_index] = 1UL;
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] =
                        samples[farthest_base + channel];
                }
                distance_cache[farthest_index] = 0.0;
            }
            delta = 0.0;
            for (center_index = 0U; center_index < k; ++center_index) {
                if (counts[center_index] == 0UL) {
                    continue;
                }
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    update = channel_sum[channel]
                        / (double)counts[center_index];
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

    merge_stop = sixel_assessment_timer_now();
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
    export_start = sixel_assessment_timer_now();
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
                if (counts[order[center_index]] >
                        counts[order[index]]) {
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

    export_stop = sixel_assessment_timer_now();
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

        now = sixel_assessment_timer_now();
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
    if (counts != NULL) {
        sixel_allocator_free(allocator, counts);
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

#if HAVE_TESTS

/*
 * Validate that two pure float32 primaries end up as distinct palette entries.
 * The test uses the public K-means entry point to avoid peeking behind
 * palette.c's orchestration layer.
 */
int
palette_test_kmeans_float32_two_colors(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *allocator = NULL;
    sixel_palette_t *palette = NULL;
    float pixels[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
    float const *data;
    unsigned char const *entry;
    int found_red;
    int found_green;
    size_t index;

    data = pixels;
    found_red = 0;
    found_green = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_palette_new(&palette, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    palette->requested_colors = 2U;
    palette->quality_mode = SIXEL_QUALITY_HIGH;
    palette->force_palette = 1;
    palette->use_reversible = 0;
    palette->final_merge_mode = SIXEL_FINAL_MERGE_NONE;

    status = sixel_palette_build_kmeans_float32(palette,
                                                data,
                                                (unsigned int)sizeof(pixels),
                                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                                allocator,
                                                NULL,
                                                NULL,
                                                "kmeans-float32",
                                                NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (palette->entry_count != 2U || palette->entries == NULL) {
        goto error;
    }
    for (index = 0U; index < 2U; ++index) {
        entry = palette->entries + index * 3U;
        if (entry[0] > 240U && entry[1] < 16U && entry[2] < 16U) {
            found_red = 1;
        }
        if (entry[1] > 240U && entry[0] < 16U && entry[2] < 16U) {
            found_green = 1;
        }
    }
    if (!found_red || !found_green) {
        goto error;
    }

    status = SIXEL_OK;

error:
    if (palette != NULL) {
        sixel_palette_unref(palette);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * Confirm that the Ward merge path retains a bright highlight when float32
 * inputs contain both light and dark samples.
 */
int
palette_test_kmeans_float32_merge_scaling(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *allocator = NULL;
    sixel_palette_t *palette = NULL;
    float pixels[12] = {
        1.0f, 1.0f, 1.0f,
        0.8f, 0.8f, 0.8f,
        0.1f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
    };
    float const *data;
    unsigned char const *entry;
    size_t index;
    int found_highlight;

    data = pixels;
    found_highlight = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_palette_new(&palette, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    palette->requested_colors = 2U;
    palette->quality_mode = SIXEL_QUALITY_HIGH;
    palette->force_palette = 1;
    palette->use_reversible = 0;
    palette->final_merge_mode = SIXEL_FINAL_MERGE_WARD;

    status = sixel_palette_build_kmeans_float32(palette,
                                                data,
                                                (unsigned int)sizeof(pixels),
                                                SIXEL_PIXELFORMAT_RGBFLOAT32,
                                                allocator,
                                                NULL,
                                                NULL,
                                                "kmeans-float32",
                                                NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (palette->entry_count != 2U || palette->entries == NULL) {
        goto error;
    }
    for (index = 0U; index < 2U; ++index) {
        entry = palette->entries + index * 3U;
        if (entry[0] > 200U && entry[1] > 200U && entry[2] > 200U) {
            found_highlight = 1;
        }
    }
    if (!found_highlight) {
        goto error;
    }

    status = SIXEL_OK;

error:
    if (palette != NULL) {
        sixel_palette_unref(palette);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
