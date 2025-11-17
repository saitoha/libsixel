/*
 * SPDX-License-Identifier: MIT
 *
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

#include "allocator.h"
#include "compat_stub.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans.h"
#include "palette.h"
#include "status.h"

/*
 * Clamp a float32 channel stored in the 0.0-1.0 range and convert it to the
 * 0-255 domain used throughout the palette builder.  The helper intentionally
 * returns a double so downstream arithmetic keeps fractional precision until
 * the very last quantization step.
 */
static double
sixel_palette_float32_channel_to_u8(double value)
{
#if HAVE_MATH_H
    if (!isfinite(value)) {
        value = 0.0;
    }
#endif

    if (value <= 0.0) {
        return 0.0;
    }
    if (value >= 1.0) {
        return 255.0;
    }

    return value * 255.0;
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

/*
 * Execute the full k-means clustering routine and return the generated palette
 * as a freshly allocated RGB array.  The implementation mirrors the previous
 * palette.c logic but is reorganised around clearly labelled segments:
 *
 *   - Sample reservoir: collect up to 50k opaque pixels.
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
                     int treat_input_as_float32)
{
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int pixel_count;
    unsigned int sample_limit;
    unsigned int sample_cap;
    unsigned int valid_seen;
    unsigned int sample_count;
    unsigned int k;
    unsigned int index;
    unsigned int channel;
    unsigned int center_index;
    unsigned int sample_index;
    unsigned int replace;
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
    double total_weight;
    double random_point;
    double best_distance;
    double distance;
    double diff;
    double update;
    double farthest_distance;
    double delta;
    double lloyd_threshold;
    double float32_scale;
    float *float_palette;
    float *float_palette_new;
    unsigned long *counts;
    double *accum;
    double *channel_sum;
    double *merge_sums;
    unsigned long rand_value;
    size_t farthest_base;
    unsigned char *unique_buffer;
    size_t unique_pixels;
    int apply_merge;
    int resolved_merge;
    unsigned int overshoot;
    unsigned int refine_iterations;
    int cluster_total;
    int unique_within;
    int input_is_rgbfloat32;
    SIXELSTATUS unique_status;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_stride = depth;
    pixel_count = 0U;
    sample_limit = 50000U;
    sample_cap = sample_limit;
    valid_seen = 0U;
    sample_count = 0U;
    k = 0U;
    index = 0U;
    channel = 0U;
    center_index = 0U;
    sample_index = 0U;
    replace = 0U;
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
    rand_value = 0UL;
    total_weight = 0.0;
    random_point = 0.0;
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
    input_is_rgbfloat32 = 0;
    unique_status = SIXEL_OK;
    float32_scale = 255.0;
    float_palette = NULL;
    float_palette_new = NULL;

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

    channels = depth;
    pixel_stride = depth;
    input_is_rgbfloat32 = (treat_input_as_float32
                           && pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32);
    if (input_is_rgbfloat32) {
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

    sample_cap = sample_limit;
    if (sample_cap > pixel_count) {
        sample_cap = pixel_count;
    }
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
        if (input_is_rgbfloat32) {
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
            } else {
                rand_value = (unsigned long)rand();
                replace = (unsigned int)(rand_value % valid_seen);
                if (replace < sample_cap) {
                    for (channel = 0U; channel < 3U; ++channel) {
                        samples[replace * 3U + channel] =
                            (double)fpixels[channel];
                    }
                }
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
            } else {
                rand_value = (unsigned long)rand();
                replace = (unsigned int)(rand_value % valid_seen);
                if (replace < sample_cap) {
                    for (channel = 0U; channel < 3U; ++channel) {
                        samples[replace * 3U + channel] =
                            (double)data[base + channel];
                    }
                }
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
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD
                   || resolved_merge == SIXEL_FINAL_MERGE_HKMEANS);
    if (apply_merge) {
        if (input_is_rgbfloat32) {
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
                        (unsigned char)(
                            sixel_palette_float32_channel_to_u8(
                                (double)fpixels[channel])
                            + 0.5);
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

    rand_value = (unsigned long)rand();
    replace = (unsigned int)(rand_value % sample_count);
    for (channel = 0U; channel < 3U; ++channel) {
        centers[channel] =
            (double)samples[replace * 3U + channel];
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
            random_point =
                ((double)rand() / ((double)RAND_MAX + 1.0)) *
                total_weight;
        }
        sample_index = 0U;
        while (sample_index + 1U < sample_count &&
               random_point > distance_cache[sample_index]) {
            random_point -= distance_cache[sample_index];
            ++sample_index;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[center_index * 3U + channel] =
                (double)samples[sample_index * 3U + channel];
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
        if (input_is_rgbfloat32) {
            double threshold_scale;

            threshold_scale = float32_scale * float32_scale;
            if (threshold_scale > 0.0) {
                lloyd_threshold /= threshold_scale;
            }
        }
    }
    for (iteration = 0U; iteration < max_iterations; ++iteration) {
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

    if (apply_merge && k > reqcolors) {
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

            component = accum[index];
            if (input_is_rgbfloat32) {
                component *= float32_scale;
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

            /* Translate merged 0-255 sums back to the original sample scale */
            restored = merge_sums[index];
            if (input_is_rgbfloat32 && float32_scale > 0.0) {
                restored /= float32_scale;
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

    palette = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U);
    if (palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (result_float32 != NULL && input_is_rgbfloat32 && k > 0U) {
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
                double normalized;

                normalized = update;
                if (normalized < 0.0) {
                    normalized = 0.0;
                }
                if (normalized > 1.0) {
                    normalized = 1.0;
                }
                float_palette[center_index * 3U + channel] =
                    (float)normalized;
            }
            if (input_is_rgbfloat32) {
                update = sixel_palette_float32_channel_to_u8(update);
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

end:
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
                                    int treat_input_as_float32)
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
                                        palette->use_reversible,
                                        palette->final_merge_mode,
                                        work_allocator,
                                        pixelformat,
                                        treat_input_as_float32);
    if (SIXEL_FAILED(build_status)) {
        status = build_status;
        goto end;
    }

    if (palette->use_reversible) {
        sixel_palette_reversible_palette(entries,
                                         ncolors,
                                         entry_depth);
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
                           sixel_allocator_t *allocator)
{
    return sixel_palette_build_kmeans_internal(palette,
                                               data,
                                               length,
                                               pixelformat,
                                               allocator,
                                               0);
}

SIXELSTATUS
sixel_palette_build_kmeans_float32(sixel_palette_t *palette,
                                   unsigned char const *data,
                                   unsigned int length,
                                   int pixelformat,
                                   sixel_allocator_t *allocator)
{
    return sixel_palette_build_kmeans_internal(palette,
                                               data,
                                               length,
                                               pixelformat,
                                               allocator,
                                               1);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */

