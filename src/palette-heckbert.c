/*
 * SPDX-License-Identifier: MIT
 *
 * mediancut algorithm implementation is derived from pnmcolormap.c
 * in netpbm library.
 * http://netpbm.sourceforge.net/
 *
 * *******************************************************************************
 *                  original license block of pnmcolormap.c
 * *******************************************************************************
 *
 *   Derived from ppmquant, originally by Jef Poskanzer.
 *
 *   Copyright (C) 1989, 1991 by Jef Poskanzer.
 *   Copyright (C) 2001 by Bryan Henderson.
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted, provided
 *   that the above copyright notice appear in all copies and that both that
 *   copyright notice and this permission notice appear in supporting
 *   documentation.  This software is provided "as is" without express or
 *   implied warranty.
 *
 * ******************************************************************************
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2018 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "threading.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_MATH_H
# include <math.h>
#endif

#if HAVE_ASSERT
# include <assert.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "timeline-logger.h"
#include "loader-common.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-heckbert.h"
#include "palette-heckbert-gpu.h"
#include "palette-private.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"
#include "simd-config.h"
#include "cpu.h"
#if SIXEL_SIMD_AVX2
# include <immintrin.h>
#endif
#if SIXEL_SIMD_NEON_A64
# include <arm_neon.h>
#endif

/*
 * K-means testcases live in palette-kmeans.c; declare them here so the
 * unified test driver can exercise both engines without importing the
 * entire implementation header.
 */
int palette_test_kmeans_float32_two_colors(void);
int palette_test_kmeans_float32_merge_scaling(void);

/* The binning filter supplies double tuples for both byte-bucket means and
 * native float input. Round byte representatives only after clustering. */
typedef double sample;
typedef sample *tuple;

enum {
    sixel_palette_heckbert_axis_bins = 256
};

typedef sixel_heckbert_tuple_t **tupletable;
typedef sixel_heckbert_histogram_t tupletable2;

static int
sixel_palette_heckbert_log_start(sixel_timeline_logger_t *logger,
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
sixel_palette_heckbert_log_finish(sixel_timeline_logger_t *logger,
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

/*
 * Convert each histogram tuple component onto the reversible SIXEL tone grid.
 * The conversion mirrors sixel_palette_reversible_palette but operates on the
 * temporary tuple tables used during Heckbert processing.
 */
static void
sixel_palette_reversible_tuple(sample *tuple_values,
                               unsigned int depth,
                               int pixelformat)
{
    double components[3];
    unsigned int plane;

    if (tuple_values == NULL || depth == 0U) {
        return;
    }
    for (plane = 0U; plane < 3U; ++plane) {
        components[plane] = 0.0;
    }
    for (plane = 0U; plane < depth && plane < 3U; ++plane) {
        if (SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat)) {
            /* Typed histograms use zero-origin, nominal-span scaling. */
            components[plane] = tuple_values[plane] / 255.0
                * sixel_pixelformat_float_channel_range(
                    pixelformat, (int)plane);
            continue;
        }
        components[plane] = (double)tuple_values[plane];
    }
    sixel_palette_snap_triple(components,
                              1,
                              pixelformat,
                              SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
    for (plane = 0U; plane < depth && plane < 3U; ++plane) {
        if (SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat)) {
            tuple_values[plane] = components[plane] * 255.0
                / sixel_pixelformat_float_channel_range(
                    pixelformat, (int)plane);
            continue;
        }
        tuple_values[plane] = (sample)components[plane];
    }
}


typedef struct box *boxVector;
struct box {
    unsigned int ind;
    unsigned int colors;
    unsigned int sum;
    double priority;
};

static tupletable2 const *force_palette_source;
static double compareplanePcaAxis[3];
static unsigned int compareplanePcaDimensions;
static unsigned int compareplaneTieDepth;

struct sixel_pca_sort_entry {
    sixel_heckbert_tuple_t *entry;
    unsigned int weight;
    double key;
};

static int
compareplane_lexicographic(sixel_heckbert_tuple_t const *lhs,
                           sixel_heckbert_tuple_t const *rhs,
                           unsigned int depth);
static int compareplanePcaCached(const void *arg1, const void *arg2);
static unsigned int sixel_palette_heckbert_entry_weight(
    sixel_heckbert_tuple_t const *entry);

static int
sixel_pca_entry_compare(struct sixel_pca_sort_entry const *lhs,
                        struct sixel_pca_sort_entry const *rhs)
{
    int diff;

    diff = 0;
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    if (lhs->key < rhs->key) {
        return -1;
    }
    if (lhs->key > rhs->key) {
        return 1;
    }
    diff = compareplane_lexicographic(lhs->entry,
                                      rhs->entry,
                                      compareplaneTieDepth);
    if (diff != 0) {
        return diff;
    }

    return 0;
}

static void
sixel_pca_entry_swap(struct sixel_pca_sort_entry *lhs,
                     struct sixel_pca_sort_entry *rhs)
{
    struct sixel_pca_sort_entry tmp;

    if (lhs == NULL || rhs == NULL || lhs == rhs) {
        return;
    }
    tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;
}

/*
 * Find a weighted median split point using in-place partitioning instead of
 * sorting the full PCA range.  The routine preserves split behavior by falling
 * back to full sort when selection fails to converge.
 */
static void
sixel_pca_select_weighted_median(struct sixel_pca_sort_entry cache[],
                                 unsigned int boxSize,
                                 unsigned int half,
                                 unsigned int *medianIndex,
                                 unsigned int *lowerSum)
{
    unsigned int left;
    unsigned int right;
    unsigned int lt;
    unsigned int gt;
    unsigned int i;
    unsigned int less_count;
    unsigned int equal_count;
    unsigned int prefix_count;
    unsigned int target;
    unsigned int index;
    unsigned int lower;
    unsigned int less_weight;
    unsigned int equal_weight;
    unsigned int prefix_weight;
    unsigned int iteration_limit;
    unsigned int weight;
    struct sixel_pca_sort_entry pivot;
    int cmp;

    left = 0U;
    right = boxSize;
    lt = 0U;
    gt = 0U;
    i = 0U;
    less_count = 0U;
    equal_count = 0U;
    prefix_count = 0U;
    target = half;
    index = 0U;
    lower = 0U;
    less_weight = 0U;
    equal_weight = 0U;
    prefix_weight = 0U;
    iteration_limit = 0U;
    weight = 0U;
    pivot.entry = NULL;
    pivot.key = 0.0;
    cmp = 0;

    if (medianIndex == NULL || lowerSum == NULL || cache == NULL
            || boxSize == 0U) {
        if (medianIndex != NULL) {
            *medianIndex = 0U;
        }
        if (lowerSum != NULL) {
            *lowerSum = 0U;
        }
        return;
    }
    if (boxSize == 1U) {
        *medianIndex = 1U;
        *lowerSum = cache[0U].weight;
        return;
    }
    if (target == 0U) {
        target = 1U;
    }
    iteration_limit = boxSize * 8U + 32U;

    while (left < right && iteration_limit > 0U) {
        if (prefix_weight >= target) {
            *medianIndex = prefix_count;
            *lowerSum = prefix_weight;
            return;
        }
        --iteration_limit;
        pivot = cache[left + (right - left) / 2U];
        lt = left;
        gt = right;
        i = left;
        while (i < gt) {
            cmp = sixel_pca_entry_compare(&cache[i], &pivot);
            if (cmp < 0) {
                sixel_pca_entry_swap(&cache[i], &cache[lt]);
                ++i;
                ++lt;
                continue;
            }
            if (cmp > 0) {
                --gt;
                sixel_pca_entry_swap(&cache[i], &cache[gt]);
                continue;
            }
            ++i;
        }
        less_count = lt - left;
        equal_count = gt - lt;
        less_weight = 0U;
        for (i = left; i < lt; ++i) {
            less_weight += cache[i].weight;
        }
        if (less_count > 0U && prefix_weight + less_weight >= target) {
            right = lt;
            continue;
        }
        lower = prefix_weight + less_weight;
        index = prefix_count + less_count;
        equal_weight = 0U;
        for (i = lt; i < gt; ++i) {
            weight = cache[i].weight;
            equal_weight += weight;
            if (lower < target) {
                lower += weight;
                ++index;
            }
        }
        if (lower >= target || prefix_weight + less_weight + equal_weight
                                  >= target) {
            *medianIndex = index;
            *lowerSum = lower;
            return;
        }
        prefix_count += less_count + equal_count;
        prefix_weight += less_weight + equal_weight;
        left = gt;
    }

    qsort((char *)cache,
          boxSize,
          sizeof(struct sixel_pca_sort_entry),
          compareplanePcaCached);
    index = 1U;
    lower = cache[0U].weight;
    while (index < boxSize - 1U && lower < target) {
        lower += cache[index].weight;
        ++index;
    }
    *medianIndex = index;
    *lowerSum = lower;
}

static int
compareplane_lexicographic(sixel_heckbert_tuple_t const *lhs,
                           sixel_heckbert_tuple_t const *rhs,
                           unsigned int depth)
{
    unsigned int plane;
    sample lhs_value;
    sample rhs_value;

    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    for (plane = 0U; plane < depth; ++plane) {
        lhs_value = lhs->components[plane];
        rhs_value = rhs->components[plane];
        if (lhs_value < rhs_value) {
            return -1;
        }
        if (lhs_value > rhs_value) {
            return 1;
        }
    }

    if (lhs->value < rhs->value) {
        return -1;
    }
    if (lhs->value > rhs->value) {
        return 1;
    }

    return 0;
}

/* qsort fallback for PCA split when key-buffer allocation fails. */
static int
compareplanePca(const void *arg1, const void *arg2)
{
    double lhs;
    double rhs;
    unsigned int plane;
    int diff;
    typedef const sixel_heckbert_tuple_t *const *const sortarg;
    sortarg comparandPP = (sortarg)arg1;
    sortarg comparatorPP = (sortarg)arg2;

    lhs = 0.0;
    rhs = 0.0;
    for (plane = 0U; plane < compareplanePcaDimensions; ++plane) {
        lhs += (double)(*comparandPP)->components[plane]
               * compareplanePcaAxis[plane];
        rhs += (double)(*comparatorPP)->components[plane]
               * compareplanePcaAxis[plane];
    }

    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }

    diff = compareplane_lexicographic(*comparandPP,
                                      *comparatorPP,
                                      compareplaneTieDepth);
    if (diff != 0) {
        return diff;
    }

    return 0;
}

/*
 * qsort callback for cached PCA projection keys.  Ties keep the previous
 * lexicographic order so split determinism matches the legacy path.
 */
static int
compareplanePcaCached(const void *arg1, const void *arg2)
{
    struct sixel_pca_sort_entry const *lhs;
    struct sixel_pca_sort_entry const *rhs;
    int diff;

    lhs = (struct sixel_pca_sort_entry const *)arg1;
    rhs = (struct sixel_pca_sort_entry const *)arg2;
    if (lhs->key < rhs->key) {
        return -1;
    }
    if (lhs->key > rhs->key) {
        return 1;
    }
    diff = compareplane_lexicographic(lhs->entry,
                                      rhs->entry,
                                      compareplaneTieDepth);
    if (diff != 0) {
        return diff;
    }

    return 0;
}

static int
sumcompare(const void *b1, const void *b2)
{
    struct box const *lhs;
    struct box const *rhs;

    lhs = (struct box const *)b1;
    rhs = (struct box const *)b2;
    if (lhs->priority < rhs->priority) {
        return 1;
    }
    if (lhs->priority > rhs->priority) {
        return -1;
    }
    if (lhs->ind < rhs->ind) {
        return -1;
    }
    if (lhs->ind > rhs->ind) {
        return 1;
    }
    if (lhs->colors < rhs->colors) {
        return -1;
    }
    if (lhs->colors > rhs->colors) {
        return 1;
    }

    return 0;
}

/*
 * Reposition one updated box while preserving sumcompare order without
 * re-sorting the full vector.
 */
static void
sixel_palette_heckbert_reposition_box(boxVector bv,
                                      unsigned int count,
                                      unsigned int index)
{
    unsigned int low;
    unsigned int high;
    unsigned int middle;
    struct box moved;

    if (bv == NULL || count < 2U || index >= count) {
        return;
    }
    /* The other boxes are in sumcompare order, and box indices differ, so
     * the order is total: binary search finds the slot the former stepwise
     * swaps reached, and one memmove shifts the boxes in between. */
    moved = bv[index];
    if (index > 0U && sumcompare(&bv[index - 1U], &moved) > 0) {
        low = 0U;
        high = index - 1U;
        while (low < high) {
            middle = low + (high - low) / 2U;
            if (sumcompare(&bv[middle], &moved) > 0) {
                high = middle;
            } else {
                low = middle + 1U;
            }
        }
        memmove(&bv[low + 1U], &bv[low], (index - low) * sizeof(*bv));
        bv[low] = moved;
        return;
    }
    if (index + 1U < count && sumcompare(&moved, &bv[index + 1U]) > 0) {
        low = index + 1U;
        high = count - 1U;
        while (low < high) {
            middle = low + (high - low + 1U) / 2U;
            if (sumcompare(&moved, &bv[middle]) > 0) {
                low = middle;
            } else {
                high = middle - 1U;
            }
        }
        memmove(&bv[index], &bv[index + 1U], (low - index) * sizeof(*bv));
        bv[low] = moved;
    }
}

static tupletable2
newColorMap(unsigned int newcolors,
            unsigned int depth,
            sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    tupletable2 colormap;
    unsigned int i;

    status = SIXEL_FALSE;
    colormap.preserve_extended = 0;
    colormap.size = 0U;
    status = sixel_heckbert_alloc_table(&colormap.table, depth, newcolors,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        return colormap;
    }
    if (colormap.table != NULL) {
        for (i = 0U; i < newcolors; ++i) {
            unsigned int plane;

            for (plane = 0U; plane < depth; ++plane) {
                colormap.table[i]->components[plane] = 0;
            }
        }
        colormap.size = newcolors;
    }

    return colormap;
}

static boxVector
newBoxVector(unsigned int colors,
             unsigned int sum,
             unsigned int newcolors,
             sixel_allocator_t *allocator)
{
    boxVector bv;

    bv = (boxVector)sixel_allocator_malloc(allocator,
                                           sizeof(struct box)
                                               * (size_t)newcolors);
    if (bv == NULL) {
        sixel_helper_set_additional_message(
            "out of memory allocating box vector table");
        return NULL;
    }

    bv[0].ind = 0U;
    bv[0].colors = colors;
    bv[0].sum = sum;
    bv[0].priority = (double)sum;

    return bv;
}

static unsigned int
sixel_palette_heckbert_entry_weight(sixel_heckbert_tuple_t const *entry)
{
    unsigned long weight;

    if (entry == NULL) {
        return 1U;
    }
    weight = entry->value;
    if (weight == 0UL) {
        return 1U;
    }
    if (weight > (unsigned long)UINT_MAX) {
        return UINT_MAX;
    }

    return (unsigned int)weight;
}

#if SIXEL_SIMD_NEON_A64
/* Three-plane form of findBoxBoundariesAndWeight. Planes 0 and 1 share one
 * vector; the bounds stay in registers and move only on a strict compare,
 * as in the scalar loop, so equal values keep the earlier bound. */
/* Accel: HECKBERT-BOX-BOUNDS neon */
static void
findBoxBoundariesAndWeightNeon3(tupletable2 const colorfreqtable,
                                unsigned int boxStart,
                                unsigned int boxSize,
                                sample minval[],
                                sample maxval[],
                                unsigned long *total_weight,
                                unsigned int axis_count
                                    [SIXEL_HECKBERT_MAX_CHANNELS]
                                    [sixel_palette_heckbert_axis_bins])
{
    sixel_heckbert_tuple_t const *entry;
    float64x2_t low;
    float64x2_t low_min;
    float64x2_t low_max;
    uint64x2_t bins;
    uint64x2_t last_bin;
    sample value;
    sample high_min;
    sample high_max;
    unsigned long weight;
    unsigned int high_bin;
    unsigned int i;

    entry = colorfreqtable.table[boxStart];
    low_min = low_max = vld1q_f64(entry->components);
    high_min = high_max = entry->components[2];
    weight = 0UL;
    if (axis_count != NULL) {
        memset(axis_count, 0, 3U * sizeof(axis_count[0]));
    }
    last_bin = vdupq_n_u64(sixel_palette_heckbert_axis_bins - 1U);
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        weight += (unsigned long)sixel_palette_heckbert_entry_weight(entry);
        low = vld1q_f64(entry->components);
        value = entry->components[2];
        low_min = vbslq_f64(vcltq_f64(low, low_min), low, low_min);
        low_max = vbslq_f64(vcgtq_f64(low, low_max), low, low_max);
        high_min = value < high_min ? value : high_min;
        high_max = value > high_max ? value : high_max;
        if (axis_count != NULL) {
            bins = vcvtq_u64_f64(low);
            bins = vbslq_u64(vcgtq_u64(bins, last_bin), last_bin, bins);
            high_bin = (unsigned int)value;
            if (high_bin >= sixel_palette_heckbert_axis_bins) {
                high_bin = sixel_palette_heckbert_axis_bins - 1U;
            }
            axis_count[0][vgetq_lane_u64(bins, 0)] += 1U;
            axis_count[1][vgetq_lane_u64(bins, 1)] += 1U;
            axis_count[2][high_bin] += 1U;
        }
    }
    vst1q_f64(minval, low_min);
    vst1q_f64(maxval, low_max);
    minval[2] = high_min;
    maxval[2] = high_max;
    if (total_weight != NULL) {
        *total_weight = weight;
    }
}
#endif

#if SIXEL_SIMD_AVX2
/* RGB bounds share a vector, but strict comparisons keep the first equal
 * value, including signed zero, just as the scalar loop does. Masked loads
 * and stores never touch a fourth component of a three-channel tuple.
 * Histogram coordinates are nonnegative: clamp before signed conversion
 * so every representable unsigned coordinate maps to the scalar bin. */
/* Accel: HECKBERT-BOX-BOUNDS avx2 */
static SIXEL_TARGET_AVX2 void
findBoxBoundariesAndWeightAvx2(tupletable2 const colorfreqtable,
                              unsigned int boxStart,
                              unsigned int boxSize,
                              sample minval[], sample maxval[],
                              unsigned long *total_weight,
                              unsigned int axis_count
                                  [SIXEL_HECKBERT_MAX_CHANNELS]
                                  [sixel_palette_heckbert_axis_bins])
{
    __m256i mask;
    __m256d value;
    __m256d low;
    __m256d high;
    __m256d last_bin;
    __m128i bins;
    sixel_heckbert_tuple_t const *entry;
    unsigned long weight;
    unsigned int i;

    mask = _mm256_set_epi64x(0, -1, -1, -1);
    entry = colorfreqtable.table[boxStart];
    low = high = _mm256_maskload_pd(entry->components, mask);
    last_bin = _mm256_set1_pd(sixel_palette_heckbert_axis_bins - 1U);
    weight = 0UL;
    if (axis_count != NULL) {
        memset(axis_count, 0, 3U * sizeof(axis_count[0]));
    }
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        weight += (unsigned long)sixel_palette_heckbert_entry_weight(entry);
        value = _mm256_maskload_pd(entry->components, mask);
        low = _mm256_blendv_pd(low, value,
            _mm256_cmp_pd(value, low, _CMP_LT_OQ));
        high = _mm256_blendv_pd(high, value,
            _mm256_cmp_pd(value, high, _CMP_GT_OQ));
        if (axis_count != NULL) {
            bins = _mm256_cvttpd_epi32(_mm256_min_pd(value, last_bin));
            axis_count[0][_mm_cvtsi128_si32(bins)] += 1U;
            axis_count[1][_mm_extract_epi32(bins, 1)] += 1U;
            axis_count[2][_mm_extract_epi32(bins, 2)] += 1U;
        }
    }
    _mm256_maskstore_pd(minval, mask, low);
    _mm256_maskstore_pd(maxval, mask, high);
    if (total_weight != NULL) {
        *total_weight = weight;
    }
}
#endif

/*
 * Collect min/max bounds and total tuple weight in one pass so splitBox can
 * reuse the same scan for axis selection and weighted median splitting.
 */
static void
findBoxBoundariesAndWeight(tupletable2 const colorfreqtable,
                           unsigned int depth,
                           unsigned int boxStart,
                           unsigned int boxSize,
                           sample minval[],
                           sample maxval[],
                           unsigned long *total_weight,
                           unsigned int axis_count
                               [SIXEL_HECKBERT_MAX_CHANNELS]
                               [sixel_palette_heckbert_axis_bins])
{
    unsigned int plane;
    unsigned int i;
    sixel_heckbert_tuple_t const *entry;
    unsigned int weight;
    unsigned int axis_value;
    sample value;

#if SIXEL_SIMD_AVX2
    if (depth == 3U
            && sixel_cpu_simd_level() == SIXEL_SIMD_LEVEL_AVX2) {
        findBoxBoundariesAndWeightAvx2(colorfreqtable, boxStart, boxSize,
            minval, maxval, total_weight, axis_count);
        return;
    }
#endif
#if SIXEL_SIMD_NEON_A64
    if (depth == 3U) {
        findBoxBoundariesAndWeightNeon3(colorfreqtable, boxStart, boxSize,
                                        minval, maxval, total_weight,
                                        axis_count);
        return;
    }
#endif
    for (plane = 0U; plane < depth; ++plane) {
        minval[plane] = colorfreqtable.table[boxStart]->components[plane];
        maxval[plane] = minval[plane];
    }
    if (total_weight != NULL) {
        *total_weight = 0UL;
    }
    if (axis_count != NULL) {
        for (plane = 0U; plane < depth; ++plane) {
            for (i = 0U; i < sixel_palette_heckbert_axis_bins; ++i) {
                axis_count[plane][i] = 0U;
            }
        }
    }

    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        if (total_weight != NULL) {
            weight = sixel_palette_heckbert_entry_weight(entry);
            *total_weight += (unsigned long)weight;
        }
        for (plane = 0U; plane < depth; ++plane) {
            value = entry->components[plane];
            if (value < minval[plane]) {
                minval[plane] = value;
            }
            if (value > maxval[plane]) {
                maxval[plane] = value;
            }
            if (axis_count != NULL) {
                axis_value = (unsigned int)value;
                if (axis_value >= sixel_palette_heckbert_axis_bins) {
                    axis_value = sixel_palette_heckbert_axis_bins - 1U;
                }
                axis_count[plane][axis_value] += 1U;
            }
        }
    }
}

/* Restore full numeric order inside an integer-axis bucket. A heap avoids
 * quadratic work for heavily occupied buckets and needs no extra workspace
 * or global comparator state. Buckets containing one coordinate retain their
 * existing permutation, including the integral lookup-policy=none path. */
static void
sortAxisFractions(tupletable entries, unsigned int count, unsigned int axis)
{
    unsigned int i;
    unsigned int root;
    unsigned int child;
    unsigned int end;
    unsigned int start;
    sixel_heckbert_tuple_t *entry;
    int differs;

    if (count < 2U) {
        return;
    }
    differs = 0;
    for (i = 1U; i < count; ++i) {
        if (entries[i]->components[axis] != entries[0]->components[axis]) {
            differs = 1;
            break;
        }
    }
    if (!differs) {
        return;
    }
    start = count / 2U;
    end = count;
    while (end > 1U) {
        if (start > 0U) {
            --start;
            entry = entries[start];
        } else {
            --end;
            entry = entries[end];
            entries[end] = entries[0];
        }
        root = start;
        while (root < end / 2U) {
            child = root * 2U + 1U;
            if (child + 1U < end && entries[child]->components[axis]
                    < entries[child + 1U]->components[axis]) {
                ++child;
            }
            if (entry->components[axis] >= entries[child]->components[axis]) {
                break;
            }
            entries[root] = entries[child];
            root = child;
        }
        entries[root] = entry;
    }
}

/*
 * Reorder tuples by one 8-bit axis using an in-place bucket permutation.
 * The coarse permutation takes O(n + 256) work; fractional means sharing
 * an integer coordinate then receive a bounded in-place comparison sort.
 */
static SIXELSTATUS
sortBoxByAxisBuckets(tupletable2 const colorfreqtable,
                     unsigned int boxStart,
                     unsigned int boxSize,
                     unsigned int axis,
                     /*
                      * Keep this non-const for C99 compatibility: passing a
                      * pointer-to-array with added qualifiers is only relaxed
                      * in newer standards, but we build with -std=c99.
                      */
                     unsigned int axis_count
                         [SIXEL_HECKBERT_MAX_CHANNELS]
                         [sixel_palette_heckbert_axis_bins],
                     sample const minval[],
                     sample const maxval[],
                     unsigned char *keys)
{
    unsigned int bucket_count[sixel_palette_heckbert_axis_bins];
    unsigned int bucket_next[sixel_palette_heckbert_axis_bins];
    unsigned int bucket_end[sixel_palette_heckbert_axis_bins];
    unsigned int min_bucket;
    unsigned int max_bucket;
    unsigned int bucket;
    unsigned int index;
    unsigned int cursor;
    unsigned int axis_value;
    unsigned int target;
    unsigned char key;
    sixel_heckbert_tuple_t *entry;

    if (axis >= SIXEL_HECKBERT_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "Internal error: invalid split axis in sortBoxByAxisBuckets.");
        return SIXEL_BAD_ARGUMENT;
    }
    for (bucket = 0U; bucket < sixel_palette_heckbert_axis_bins; ++bucket) {
        bucket_count[bucket] = 0U;
        bucket_next[bucket] = 0U;
        bucket_end[bucket] = 0U;
    }
    /* Read each bucket once. The permutation below then moves the keys with
     * the entries instead of loading every entry again. */
    for (index = 0U; index < boxSize; ++index) {
        entry = colorfreqtable.table[boxStart + index];
        axis_value = (unsigned int)entry->components[axis];
        if (axis_value >= sixel_palette_heckbert_axis_bins) {
            axis_value = sixel_palette_heckbert_axis_bins - 1U;
        }
        keys[index] = (unsigned char)axis_value;
    }

    if (axis_count != NULL && minval != NULL && maxval != NULL) {
        min_bucket = (unsigned int)minval[axis];
        max_bucket = (unsigned int)maxval[axis];
        if (min_bucket >= sixel_palette_heckbert_axis_bins) {
            min_bucket = sixel_palette_heckbert_axis_bins - 1U;
        }
        if (max_bucket >= sixel_palette_heckbert_axis_bins) {
            max_bucket = sixel_palette_heckbert_axis_bins - 1U;
        }
        for (bucket = min_bucket; bucket <= max_bucket; ++bucket) {
            bucket_count[bucket] = axis_count[axis][bucket];
        }
    } else {
        min_bucket = sixel_palette_heckbert_axis_bins - 1U;
        max_bucket = 0U;
        for (index = 0U; index < boxSize; ++index) {
            axis_value = keys[index];
            bucket_count[axis_value] += 1U;
            if (axis_value < min_bucket) {
                min_bucket = axis_value;
            }
            if (axis_value > max_bucket) {
                max_bucket = axis_value;
            }
        }
    }
    if (boxSize < 2U) {
        return SIXEL_OK;
    }

    cursor = boxStart;
    for (bucket = min_bucket; bucket <= max_bucket; ++bucket) {
        bucket_next[bucket] = cursor;
        bucket_end[bucket] = cursor + bucket_count[bucket];
        cursor = bucket_end[bucket];
    }

    for (bucket = min_bucket; bucket <= max_bucket; ++bucket) {
        while (bucket_next[bucket] < bucket_end[bucket]) {
            index = bucket_next[bucket];
            key = keys[index - boxStart];
            if (key == bucket) {
                bucket_next[bucket] += 1U;
                continue;
            }
            target = bucket_next[key];
            entry = colorfreqtable.table[target];
            colorfreqtable.table[target] = colorfreqtable.table[index];
            colorfreqtable.table[index] = entry;
            keys[index - boxStart] = keys[target - boxStart];
            keys[target - boxStart] = key;
            bucket_next[key] += 1U;
        }
    }

    for (bucket = min_bucket; bucket <= max_bucket; ++bucket) {
        sortAxisFractions(colorfreqtable.table
            + bucket_end[bucket] - bucket_count[bucket],
            bucket_count[bucket], axis);
    }

    return SIXEL_OK;
}

/* Define ratios in native units, then compensate the histogram's nominal
 * byte scaling. CIELAB emphasizes lightness by two in native coordinates.
 * A common ratio multiplier cancels before any squared-error accumulation. */
static void
sixel_heckbert_axis_weights(int pixelformat, int method,
                            sixel_heckbert_options_t const *options,
                            double weights[4])
{
    double ratios[3];
    double ranges[3];
    double maximum;
    unsigned int plane;
    int perceptual;

    perceptual = pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32 ||
        pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32 ||
        pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    ratios[0] = pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32 ? 2.0 :
        pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32 ? 2.0 : 1.0;
    ratios[1] = ratios[2] = 1.0;
    if (method == SIXEL_LARGE_LUM) {
        if (perceptual) {
            ratios[0] *= 2.0;
        } else {
            ratios[0] = 0.2989;
            ratios[1] = 0.5866;
            ratios[2] = 0.1145;
        }
    }
    maximum = 0.0;
    for (plane = 0U; plane < 3U; ++plane) {
        ranges[plane] = SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat)
            ? sixel_pixelformat_float_channel_range(pixelformat, (int)plane)
            : 1.0;
        if (options != NULL && options->weights[0] > 0.0) {
            ratios[plane] = options->weights[plane];
        }
        if (ratios[plane] > maximum) {
            maximum = ratios[plane];
        }
    }
    for (plane = 0U; plane < 3U; ++plane) {
        weights[plane] = ratios[plane] / maximum * ranges[plane];
    }
    /* Keep RGB luminosity multiplication bit-identical to its legacy form. */
    if (method == SIXEL_LARGE_LUM && !perceptual &&
            (options == NULL || options->weights[0] == 0.0)) {
        memcpy(weights, ratios, sizeof(ratios));
    }
    weights[3] = 1.0;
}

static unsigned int
largestByWeights(sample minval[], sample maxval[], unsigned int depth,
                  double const weights[4])
{
    unsigned int largest_dimension;
    unsigned int plane;
    double largest;
    double spread;

    largest_dimension = 0U;
    largest = 0.0;
    for (plane = 0U; plane < depth; ++plane) {
        spread = (maxval[plane] - minval[plane]) * weights[plane];
        if (spread > largest) {
            largest_dimension = plane;
            largest = spread;
        }
    }
    return largest_dimension;
}

/*
 * Estimate the first principal component of the colors inside the target box
 * using a lightweight power iteration over the weighted covariance matrix.
 * The routine uses up to three channels and skips PCA entirely if the variance
 * collapses to zero so the caller can fall back to legacy strategies.
 */
static int
computePcaAxis(tupletable2 const colorfreqtable,
               unsigned int depth,
               unsigned int boxStart,
               unsigned int boxSize,
               double axis[3],
               double *explained)
{
    unsigned int dims;
    unsigned int i;
    unsigned int plane;
    unsigned int other;
    unsigned int iter;
    double weight_sum;
    double mean[3];
    double cov[3][3];
    double second_moment[3][3];
    double tuple_value[3];
    double vec[3];
    double next[3];
    double norm;
    double alignment;
    double variance_total;
    double lambda;
    double normalized_second;
    double cov_value;
    double projected;
    sixel_heckbert_tuple_t *entry;
    double weight;

    dims = depth < 3U ? depth : 3U;
    weight_sum = 0.0;
    alignment = 0.0;
    variance_total = 0.0;
    lambda = 0.0;
    normalized_second = 0.0;
    cov_value = 0.0;
    projected = 0.0;
    for (plane = 0U; plane < 3U; ++plane) {
        mean[plane] = 0.0;
        tuple_value[plane] = 0.0;
        vec[plane] = (plane < dims) ? 1.0 : 0.0;
        axis[plane] = 0.0;
        for (other = 0U; other < 3U; ++other) {
            cov[plane][other] = 0.0;
            second_moment[plane][other] = 0.0;
        }
    }
    if (dims == 0U || boxSize < 2U) {
        return 0;
    }

    /*
     * Legacy integral tuples use first and second moments from one scan.
     * Native tuples add a centered covariance pass below: subtracting large,
     * nearly equal moments can erase a small but meaningful spread.
     */
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        weight = (double)entry->value;
        if (weight == 0.0) {
            weight = 1.0;
        }
        weight_sum += weight;
        for (plane = 0U; plane < dims; ++plane) {
            tuple_value[plane] = (double)entry->components[plane];
            mean[plane] += weight * tuple_value[plane];
        }
        for (plane = 0U; plane < dims; ++plane) {
            for (other = plane; other < dims; ++other) {
                second_moment[plane][other]
                    += weight * tuple_value[plane] * tuple_value[other];
            }
        }
    }
    if (weight_sum <= 0.0) {
        return 0;
    }
    for (plane = 0U; plane < dims; ++plane) {
        mean[plane] /= weight_sum;
    }
    if (colorfreqtable.preserve_extended) {
        memset(second_moment, 0, sizeof(second_moment));
        for (i = 0U; i < boxSize; ++i) {
            entry = colorfreqtable.table[boxStart + i];
            weight = (double)entry->value;
            for (plane = 0U; plane < dims; ++plane) {
                tuple_value[plane] = entry->components[plane] - mean[plane];
            }
            for (plane = 0U; plane < dims; ++plane) {
                for (other = plane; other < dims; ++other) {
                    second_moment[plane][other] += weight
                        * tuple_value[plane] * tuple_value[other];
                }
            }
        }
    }
    for (plane = 0U; plane < dims; ++plane) {
        for (other = plane; other < dims; ++other) {
            normalized_second = second_moment[plane][other] / weight_sum;
            cov_value = colorfreqtable.preserve_extended ? normalized_second
                : normalized_second - mean[plane] * mean[other];
            if (plane == other && cov_value < 0.0 && cov_value > -1.0e-9) {
                cov_value = 0.0;
            }
            cov[plane][other] = cov_value;
            cov[other][plane] = cov_value;
        }
        variance_total += cov[plane][plane];
    }
    if (variance_total <= 0.0) {
        return 0;
    }

    for (iter = 0U; iter < 12U; ++iter) {
        for (plane = 0U; plane < 3U; ++plane) {
            next[plane] = 0.0;
        }
        for (plane = 0U; plane < dims; ++plane) {
            for (other = 0U; other < dims; ++other) {
                next[plane] += cov[plane][other] * vec[other];
            }
        }
        norm = 0.0;
        for (plane = 0U; plane < dims; ++plane) {
            norm += next[plane] * next[plane];
        }
        norm = sqrt(norm);
        if (norm <= 0.0) {
            return 0;
        }
        alignment = 0.0;
        for (plane = 0U; plane < dims; ++plane) {
            next[plane] /= norm;
            alignment += next[plane] * vec[plane];
        }
        if (alignment < 0.0) {
            alignment = -alignment;
        }
        for (plane = 0U; plane < 3U; ++plane) {
            vec[plane] = (plane < dims) ? next[plane] : 0.0;
        }
        if (1.0 - alignment <= 1.0e-6) {
            break;
        }
    }
    lambda = 0.0;
    for (plane = 0U; plane < dims; ++plane) {
        projected = 0.0;
        for (other = 0U; other < dims; ++other) {
            projected += cov[plane][other] * vec[other];
        }
        lambda += vec[plane] * projected;
    }
    if (!(lambda > 0.0)) {
        return 0;
    }
    norm = 0.0;
    for (plane = 0U; plane < dims; ++plane) {
        norm += vec[plane] * vec[plane];
    }
    norm = sqrt(norm);
    if (norm <= 0.0) {
        return 0;
    }
    for (plane = 0U; plane < 3U; ++plane) {
        axis[plane] = vec[plane] / norm;
    }
    if (explained != NULL && variance_total > 0.0) {
        *explained = lambda / variance_total;
    }

    return 1;
}

/*
 * Sort the target box by precomputed PCA projection keys.  Allocation failure
 * is reported so callers can fall back to the legacy compareplanePca path.
 */
static SIXELSTATUS
sortBoxByPcaProjection(tupletable2 const colorfreqtable,
                       unsigned int boxStart,
                       unsigned int boxSize,
                       unsigned int depth,
                       unsigned int dimensions,
                       double axis[3],
                       struct sixel_pca_sort_entry *workspace,
                       unsigned int workspace_capacity)
{
    struct sixel_pca_sort_entry *cache;
    struct sixel_pca_sort_entry *owned_cache;
    unsigned int i;
    unsigned int plane;
    sixel_heckbert_tuple_t *entry;
    double key;

    cache = NULL;
    owned_cache = NULL;
    i = 0U;
    plane = 0U;
    entry = NULL;
    key = 0.0;
    if (workspace != NULL && workspace_capacity >= boxSize) {
        cache = workspace;
    } else {
        owned_cache = (struct sixel_pca_sort_entry *)malloc(
            boxSize * sizeof(struct sixel_pca_sort_entry));
        if (owned_cache == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        cache = owned_cache;
    }

    compareplaneTieDepth = depth;
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        key = 0.0;
        for (plane = 0U; plane < dimensions; ++plane) {
            key += (double)entry->components[plane] * axis[plane];
        }
        cache[i].entry = entry;
        cache[i].weight = sixel_palette_heckbert_entry_weight(entry);
        cache[i].key = key;
    }
    qsort((char *)cache,
          boxSize,
          sizeof(struct sixel_pca_sort_entry),
          compareplanePcaCached);
    for (i = 0U; i < boxSize; ++i) {
        colorfreqtable.table[boxStart + i] = cache[i].entry;
    }
    free(owned_cache);

    return SIXEL_OK;
}

/*
 * Partition one PCA box around its weighted median without globally sorting
 * the range.  The selected lower side stays in the front region expected by
 * splitBox, while the upper side remains in the back region.
 */
static SIXELSTATUS
partitionBoxByPcaMedian(tupletable2 const colorfreqtable,
                        unsigned int boxStart,
                        unsigned int boxSize,
                        unsigned int depth,
                        unsigned int dimensions,
                        double axis[3],
                        unsigned int sum,
                        struct sixel_pca_sort_entry *workspace,
                        unsigned int workspace_capacity,
                        unsigned int *medianIndex,
                        unsigned int *lowerSum)
{
    struct sixel_pca_sort_entry *cache;
    struct sixel_pca_sort_entry *owned_cache;
    unsigned int i;
    unsigned int plane;
    sixel_heckbert_tuple_t *entry;
    double key;
    unsigned int index;
    unsigned int lower;

    cache = NULL;
    owned_cache = NULL;
    i = 0U;
    plane = 0U;
    entry = NULL;
    key = 0.0;
    index = 0U;
    lower = 0U;
    if (medianIndex == NULL || lowerSum == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (boxSize == 0U) {
        *medianIndex = 0U;
        *lowerSum = 0U;
        return SIXEL_OK;
    }
    if (boxSize == 1U) {
        *medianIndex = 1U;
        *lowerSum = sixel_palette_heckbert_entry_weight(
            colorfreqtable.table[boxStart]);
        return SIXEL_OK;
    }
    if (workspace != NULL && workspace_capacity >= boxSize) {
        cache = workspace;
    } else {
        owned_cache = (struct sixel_pca_sort_entry *)malloc(
            boxSize * sizeof(struct sixel_pca_sort_entry));
        if (owned_cache == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        cache = owned_cache;
    }

    compareplaneTieDepth = depth;
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        key = 0.0;
        for (plane = 0U; plane < dimensions; ++plane) {
            key += (double)entry->components[plane] * axis[plane];
        }
        cache[i].entry = entry;
        cache[i].weight = sixel_palette_heckbert_entry_weight(entry);
        cache[i].key = key;
    }
    sixel_pca_select_weighted_median(cache,
                                     boxSize,
                                     sum / 2U,
                                     medianIndex,
                                     lowerSum);
    index = *medianIndex;
    if (index == 0U) {
        index = 1U;
    }
    if (index >= boxSize) {
        index = boxSize - 1U;
    }
    lower = 0U;
    for (i = 0U; i < index; ++i) {
        lower += cache[i].weight;
    }
    for (i = 0U; i < boxSize; ++i) {
        colorfreqtable.table[boxStart + i] = cache[i].entry;
    }
    if (owned_cache != NULL) {
        free(owned_cache);
    }
    *medianIndex = index;
    *lowerSum = lower;

    return SIXEL_OK;
}

/* Bucket means remain fractional through partitioning and representative
 * selection. Round only the resulting byte representative, with exact halves
 * upward; native float tuples retain their fractions and extended range. */
static void
centerBox(unsigned int boxStart,
          unsigned int boxSize,
          tupletable2 const colorfreqtable,
          unsigned int depth,
          tuple newTuple)
{
    unsigned int plane;
    sample minval;
    sample maxval;
    sample v;
    unsigned int i;

    for (plane = 0U; plane < depth; ++plane) {
        minval = colorfreqtable.table[boxStart]->components[plane];
        maxval = minval;
        for (i = 1U; i < boxSize; ++i) {
            v = colorfreqtable.table[boxStart + i]->components[plane];
            if (v < minval) {
                minval = v;
            }
            if (v > maxval) {
                maxval = v;
            }
        }
        newTuple[plane] = (minval + maxval) / 2;
        if (!colorfreqtable.preserve_extended) {
            newTuple[plane] = floor(newTuple[plane] + 0.5);
        }
    }
}

static void
averageColors(unsigned int boxStart,
              unsigned int boxSize,
              tupletable2 const colorfreqtable,
              unsigned int depth,
              tuple newTuple)
{
    unsigned int plane;
    sample sum;
    unsigned int i;

    if (boxSize == 0U) {
        for (plane = 0U; plane < depth; ++plane) {
            newTuple[plane] = 0;
        }
        return;
    }

    for (plane = 0U; plane < depth; ++plane) {
        sum = 0;
        for (i = 0U; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->components[plane];
        }
        newTuple[plane] = sum / boxSize;
        if (!colorfreqtable.preserve_extended) {
            newTuple[plane] = floor(newTuple[plane] + 0.5);
        }
    }
}

static void
averagePixels(unsigned int boxStart,
              unsigned int boxSize,
              tupletable2 const colorfreqtable,
              unsigned int depth,
              tuple newTuple)
{
    unsigned int n;
    unsigned int plane;
    unsigned int i;
    sample sum;

    n = 0U;
    for (i = 0U; i < boxSize; ++i) {
        n += (unsigned int)colorfreqtable.table[boxStart + i]->value;
    }

    for (plane = 0U; plane < depth; ++plane) {
        sum = 0;
        for (i = 0U; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->components[plane]
                * (unsigned int)colorfreqtable.table[boxStart + i]->value;
        }
        if (n != 0U) {
            newTuple[plane] = sum / n;
            if (!colorfreqtable.preserve_extended) {
                newTuple[plane] = floor(newTuple[plane] + 0.5);
            }
        } else {
            newTuple[plane] = 0;
        }
    }
}

static tupletable2
colormapFromBv(unsigned int newcolors,
               boxVector bv,
               unsigned int boxes,
               tupletable2 const colorfreqtable,
               unsigned int depth,
               int methodForRep,
               int use_reversible,
                int pixelformat,
                sixel_allocator_t *allocator)
{
    tupletable2 colormap;
    unsigned int bi;

    colormap = newColorMap(newcolors, depth, allocator);
    if (colormap.size == 0U) {
        return colormap;
    }

    for (bi = 0U; bi < boxes; ++bi) {
        switch (methodForRep) {
        case SIXEL_REP_AUTO:
        case SIXEL_REP_CENTER_BOX:
            centerBox(bv[bi].ind,
                      bv[bi].colors,
                      colorfreqtable,
                      depth,
                      colormap.table[bi]->components);
            break;
        case SIXEL_REP_AVERAGE_COLORS:
            averageColors(bv[bi].ind,
                          bv[bi].colors,
                          colorfreqtable,
                          depth,
                          colormap.table[bi]->components);
            break;
        case SIXEL_REP_AVERAGE_PIXELS:
            averagePixels(bv[bi].ind,
                          bv[bi].colors,
                          colorfreqtable,
                          depth,
                          colormap.table[bi]->components);
            break;
        default:
#if HAVE_ASSERT
            assert(!"invalid representative selection method");
#endif
            break;
        }
        if (use_reversible && !sixel_palette_snap_destination_active()) {
            sixel_palette_reversible_tuple(colormap.table[bi]->components,
                                          depth,
                                          pixelformat);
        }
    }

    return colormap;
}

static int
force_palette_compare(const void *lhs, const void *rhs)
{
    unsigned int left;
    unsigned int right;
    unsigned int left_value;
    unsigned int right_value;

    left = *(const unsigned int *)lhs;
    right = *(const unsigned int *)rhs;
    left_value = force_palette_source->table[left]->value;
    right_value = force_palette_source->table[right]->value;
    if (left_value > right_value) {
        return -1;
    }
    if (left_value < right_value) {
        return 1;
    }
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }

    return 0;
}

static SIXELSTATUS
force_palette_completion(tupletable2 *colormapP,
                         unsigned int depth,
                         unsigned int reqColors,
                         tupletable2 const colorfreqtable,
                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    tupletable new_table;
    unsigned int *order;
    unsigned int current;
    unsigned int fill;
    unsigned int candidate;
    unsigned int plane;
    unsigned int source;

    status = SIXEL_FALSE;
    new_table = NULL;
    order = NULL;
    current = colormapP->size;
    if (current >= reqColors) {
        return SIXEL_OK;
    }

    status = sixel_heckbert_alloc_table(&new_table, depth, reqColors,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (colorfreqtable.size > 0U) {
        order = (unsigned int *)sixel_allocator_malloc(
            allocator,
            colorfreqtable.size * sizeof(unsigned int));
        if (order == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (candidate = 0U; candidate < colorfreqtable.size; ++candidate) {
            order[candidate] = candidate;
        }
        force_palette_source = &colorfreqtable;
        qsort(order,
              colorfreqtable.size,
              sizeof(unsigned int),
              force_palette_compare);
        force_palette_source = NULL;
    }

    for (fill = 0U; fill < current; ++fill) {
        new_table[fill]->value = colormapP->table[fill]->value;
        for (plane = 0U; plane < depth; ++plane) {
            new_table[fill]->components[plane]
                = colormapP->table[fill]->components[plane];
        }
    }

    candidate = 0U;
    fill = current;
    if (order != NULL) {
        while (fill < reqColors && candidate < colorfreqtable.size) {
            unsigned int index;

            index = order[candidate];
            new_table[fill]->value = colorfreqtable.table[index]->value;
            for (plane = 0U; plane < depth; ++plane) {
                new_table[fill]->components[plane]
                    = colorfreqtable.table[index]->components[plane];
            }
            ++fill;
            ++candidate;
        }
    }

    if (fill < reqColors && fill == 0U) {
        new_table[fill]->value = 0U;
        for (plane = 0U; plane < depth; ++plane) {
            new_table[fill]->components[plane] = 0U;
        }
        ++fill;
    }

    source = 0U;
    while (fill < reqColors) {
        new_table[fill]->value = new_table[source]->value;
        for (plane = 0U; plane < depth; ++plane) {
            new_table[fill]->components[plane] =
                new_table[source]->components[plane];
        }
        ++fill;
        ++source;
        if (source >= fill) {
            source = 0U;
        }
    }

    sixel_allocator_free(allocator, colormapP->table);
    colormapP->table = new_table;
    colormapP->size = reqColors;
    status = SIXEL_OK;

end:
    if (status != SIXEL_OK && new_table != NULL) {
        sixel_allocator_free(allocator, new_table);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }

    return status;
}

/* Translate merged cluster statistics into a tupletable palette. */
/*
 * Reassign histogram entries to the merged palette so the final centroids stay
 * aligned with their nearest tuples.  The loop iterates through assignment,
 * accumulation, and centroid update until convergence or the iteration limit.
 */
/* Rebuild these statistics from assignments, never from rounded centers. */
static void
sixel_heckbert_rep_add(sixel_palette_rep_stats_t *stats,
                       sixel_heckbert_tuple_t const *entry, unsigned int depth)
{
    unsigned int plane;
    double value;

    if (stats == NULL) {
        return;
    }
    for (plane = 0U; plane < depth; ++plane) {
        value = entry->components[plane];
        stats->sums[plane] += value;
        if (stats->colors == 0U || value < stats->min[plane]) {
            stats->min[plane] = value;
        }
        if (stats->colors == 0U || value > stats->max[plane]) {
            stats->max[plane] = value;
        }
    }
    ++stats->colors;
}

static void
sixel_final_merge_lloyd_histogram(tupletable2 const colorfreqtable,
                                  unsigned int depth,
                                  unsigned int cluster_count,
                                  unsigned long *cluster_weight,
                                  double *cluster_sums,
                                  unsigned int iterations,
                                  unsigned int *executed_iterations,
                                  int pixelformat,
                                  sixel_palette_rep_stats_t *stats)
{
    double centers[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned long cluster_weight_local[SIXEL_PALETTE_MAX];
    double cluster_sums_local[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    double distance;
    double best_distance;
    double diff;
    double channel;
    double entry0;
    double entry1;
    double entry2;
    double entry3;
    double weighted_value;
    unsigned int iteration;
    unsigned int cluster_index;
    unsigned int component;
    unsigned int entry_index;
    unsigned int best_cluster;
    unsigned long weight;
    unsigned long value;
    size_t offset;
    size_t local_offset;
    size_t output_offset;
    unsigned int *assignment;
    unsigned int previous_cluster;
    int has_assignment_history;
    int assignment_changed;
    int center_changed;
    double previous_center;
    unsigned int executed;
    sixel_heckbert_tuple_t *entry;

    distance = 0.0;
    best_distance = 0.0;
    diff = 0.0;
    channel = 0.0;
    entry0 = 0.0;
    entry1 = 0.0;
    entry2 = 0.0;
    entry3 = 0.0;
    weighted_value = 0.0;
    iteration = 0U;
    cluster_index = 0U;
    component = 0U;
    entry_index = 0U;
    best_cluster = 0U;
    weight = 0UL;
    value = 0UL;
    offset = 0U;
    local_offset = 0U;
    output_offset = 0U;
    assignment = NULL;
    previous_cluster = 0U;
    has_assignment_history = 0;
    assignment_changed = 0;
    center_changed = 0;
    previous_center = 0.0;
    executed = 0U;
    entry = NULL;
    if (executed_iterations != NULL) {
        *executed_iterations = 0U;
    }
    if (iterations == 0U || cluster_count == 0U || depth == 0U
        || cluster_weight == NULL || cluster_sums == NULL
        || colorfreqtable.table == NULL || colorfreqtable.size == 0U) {
        return;
    }
    /*
     * Heckbert merge paths only handle palette-sized cluster counts and
     * channel depths that fit the fixed tuple representation.
     */
    if (depth > SIXEL_MAX_CHANNELS || cluster_count > SIXEL_PALETTE_MAX) {
        return;
    }
    /*
     * The caller builds clusters from the histogram table, so cluster_count
     * must never exceed the tuple count.  Abort refinement if metadata is
     * inconsistent to avoid out-of-bounds reads on analyzer builds.
     */
    if (cluster_count > colorfreqtable.size) {
        return;
    }
#if SIZE_MAX <= UINT_MAX
    /*
     * On narrow-size_t targets keep explicit overflow guards for buffer
     * sizing.  Wider targets make these comparisons tautologically false.
     */
    if ((size_t)cluster_count > SIZE_MAX / sizeof(unsigned long)) {
        return;
    }
    if ((size_t)cluster_count > SIZE_MAX / (size_t)depth) {
        return;
    }
#endif
#if SIZE_MAX <= UINT_MAX
    if ((size_t)colorfreqtable.size > SIZE_MAX / sizeof(unsigned int)) {
        return;
    }
#endif
    assignment = (unsigned int *)malloc((size_t)colorfreqtable.size
                                        * sizeof(unsigned int));
    if (assignment != NULL) {
        for (entry_index = 0U; entry_index < colorfreqtable.size;
                ++entry_index) {
            assignment[entry_index] = UINT_MAX;
        }
    }
    memset(cluster_weight_local, 0, sizeof(cluster_weight_local));
    memset(cluster_sums_local, 0, sizeof(cluster_sums_local));
    memset(centers, 0, sizeof(centers));
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        local_offset = (size_t)cluster_index * (size_t)SIXEL_MAX_CHANNELS;
        output_offset = (size_t)cluster_index * (size_t)depth;
        weight = cluster_weight[cluster_index];
        cluster_weight_local[cluster_index] = weight;
        for (component = 0U; component < depth; ++component) {
            cluster_sums_local[local_offset + (size_t)component]
                = cluster_sums[output_offset + (size_t)component];
            if (weight > 0UL) {
                centers[local_offset + (size_t)component]
                    = cluster_sums_local[local_offset + (size_t)component]
                    / (double)weight;
            }
        }
    }
    for (iteration = 0U; iteration < iterations; ++iteration) {
        /* Return revoked loans before the next solver job. */
        sixel_thread_work_checkpoint();
        if (stats != NULL) {
            memset(stats, 0, cluster_count * sizeof(*stats));
        }
        assignment_changed = 0;
        center_changed = 0;
        /* Reset cluster accumulators in one contiguous pass. */
        memset(cluster_weight_local, 0, sizeof(cluster_weight_local));
        memset(cluster_sums_local, 0, sizeof(cluster_sums_local));
        for (entry_index = 0U; entry_index < colorfreqtable.size;
                ++entry_index) {
            entry = colorfreqtable.table[entry_index];
            if (entry == NULL) {
                continue;
            }
            value = (unsigned long)entry->value;
            if (value == 0UL) {
                continue;
            }
            best_cluster = 0U;
            previous_cluster = UINT_MAX;
            if (assignment != NULL) {
                previous_cluster = assignment[entry_index];
                if (previous_cluster < cluster_count) {
                    best_cluster = previous_cluster;
                }
            }
            /*
             * Static analyzers may conservatively keep UINT_MAX from the
             * assignment history path. Normalize before center indexing so
             * every offset uses a cluster id within [0, cluster_count).
             */
            if (best_cluster >= cluster_count) {
                best_cluster = 0U;
            }
            /*
             * Most inputs are RGB/RGBA. Keep dedicated hot paths to avoid
             * the per-component loop overhead in the nearest-center search.
             */
            if (depth == 3U) {
                entry0 = (double)entry->components[0U];
                entry1 = (double)entry->components[1U];
                entry2 = (double)entry->components[2U];
                offset = (size_t)best_cluster * (size_t)SIXEL_MAX_CHANNELS;
                diff = entry0 - centers[offset + 0U];
                best_distance = diff * diff;
                diff = entry1 - centers[offset + 1U];
                best_distance += diff * diff;
                diff = entry2 - centers[offset + 2U];
                best_distance += diff * diff;
                for (cluster_index = 0U; cluster_index < cluster_count;
                        ++cluster_index) {
                    if (cluster_index == best_cluster) {
                        continue;
                    }
                    offset = (size_t)cluster_index
                        * (size_t)SIXEL_MAX_CHANNELS;
                    diff = entry0 - centers[offset + 0U];
                    distance = diff * diff;
                    if (distance <= best_distance) {
                        diff = entry1 - centers[offset + 1U];
                        distance += diff * diff;
                        if (distance <= best_distance) {
                            diff = entry2 - centers[offset + 2U];
                            distance += diff * diff;
                        }
                    }
                    if (distance < best_distance
                        || (distance == best_distance
                            && cluster_index < best_cluster)) {
                        best_distance = distance;
                        best_cluster = cluster_index;
                    }
                }
            } else if (depth == 4U) {
                entry0 = (double)entry->components[0U];
                entry1 = (double)entry->components[1U];
                entry2 = (double)entry->components[2U];
                entry3 = (double)entry->components[3U];
                offset = (size_t)best_cluster * (size_t)SIXEL_MAX_CHANNELS;
                diff = entry0 - centers[offset + 0U];
                best_distance = diff * diff;
                diff = entry1 - centers[offset + 1U];
                best_distance += diff * diff;
                diff = entry2 - centers[offset + 2U];
                best_distance += diff * diff;
                diff = entry3 - centers[offset + 3U];
                best_distance += diff * diff;
                for (cluster_index = 0U; cluster_index < cluster_count;
                        ++cluster_index) {
                    if (cluster_index == best_cluster) {
                        continue;
                    }
                    offset = (size_t)cluster_index
                        * (size_t)SIXEL_MAX_CHANNELS;
                    diff = entry0 - centers[offset + 0U];
                    distance = diff * diff;
                    if (distance <= best_distance) {
                        diff = entry1 - centers[offset + 1U];
                        distance += diff * diff;
                        if (distance <= best_distance) {
                            diff = entry2 - centers[offset + 2U];
                            distance += diff * diff;
                            if (distance <= best_distance) {
                                diff = entry3 - centers[offset + 3U];
                                distance += diff * diff;
                            }
                        }
                    }
                    if (distance < best_distance
                        || (distance == best_distance
                            && cluster_index < best_cluster)) {
                        best_distance = distance;
                        best_cluster = cluster_index;
                    }
                }
            } else {
                best_distance = 0.0;
                offset = (size_t)best_cluster * (size_t)SIXEL_MAX_CHANNELS;
                for (component = 0U; component < depth; ++component) {
                    diff = (double)entry->components[component]
                        - centers[offset + (size_t)component];
                    best_distance += diff * diff;
                }
                for (cluster_index = 0U; cluster_index < cluster_count;
                        ++cluster_index) {
                    if (cluster_index == best_cluster) {
                        continue;
                    }
                    distance = 0.0;
                    offset = (size_t)cluster_index
                        * (size_t)SIXEL_MAX_CHANNELS;
                    for (component = 0U; component < depth; ++component) {
                        diff = (double)entry->components[component]
                            - centers[offset + (size_t)component];
                        distance += diff * diff;
                        if (distance > best_distance) {
                            break;
                        }
                    }
                    if (distance < best_distance
                        || (distance == best_distance
                            && cluster_index < best_cluster)) {
                        best_distance = distance;
                        best_cluster = cluster_index;
                    }
                }
            }
            if (assignment != NULL) {
                assignment[entry_index] = best_cluster;
                if (previous_cluster != best_cluster) {
                    assignment_changed = 1;
                }
            }
            offset = (size_t)best_cluster * (size_t)SIXEL_MAX_CHANNELS;
            if (stats != NULL) {
                sixel_heckbert_rep_add(stats + best_cluster, entry, depth);
            }
            cluster_weight_local[best_cluster] += value;
            weighted_value = (double)value;
            /* Mirror the hot-path specialization for weighted sum updates. */
            if (depth == 3U) {
                cluster_sums_local[offset + 0U] += entry0 * weighted_value;
                cluster_sums_local[offset + 1U] += entry1 * weighted_value;
                cluster_sums_local[offset + 2U] += entry2 * weighted_value;
            } else if (depth == 4U) {
                cluster_sums_local[offset + 0U] += entry0 * weighted_value;
                cluster_sums_local[offset + 1U] += entry1 * weighted_value;
                cluster_sums_local[offset + 2U] += entry2 * weighted_value;
                cluster_sums_local[offset + 3U] += entry3 * weighted_value;
            } else {
                for (component = 0U; component < depth; ++component) {
                    cluster_sums_local[offset + (size_t)component]
                        += entry->components[component] * weighted_value;
                }
            }
        }
        if (assignment != NULL) {
            if (has_assignment_history && !assignment_changed) {
                executed = iteration + 1U;
                break;
            }
            has_assignment_history = 1;
        }
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            weight = cluster_weight_local[cluster_index];
            if (weight == 0UL) {
                continue;
            }
            offset = (size_t)cluster_index * (size_t)SIXEL_MAX_CHANNELS;
            for (component = 0U; component < depth; ++component) {
                previous_center = centers[offset + (size_t)component];
                channel = cluster_sums_local[offset + (size_t)component]
                    / (double)weight;
                if (channel != previous_center) {
                    center_changed = 1;
                }
                centers[offset + (size_t)component] = channel;
            }
            if (depth == 3U && colorfreqtable.preserve_extended) {
                sixel_palette_snap_scaled_triple(centers + offset,
                    pixelformat, SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_ITER);
            }
        }
        executed = iteration + 1U;
        if (!center_changed) {
            break;
        }
    }
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        weight = cluster_weight_local[cluster_index];
        offset = (size_t)cluster_index * (size_t)SIXEL_MAX_CHANNELS;
        if (weight == 0UL) {
            for (component = 0U; component < depth; ++component) {
                channel = centers[offset + (size_t)component];
                if (!colorfreqtable.preserve_extended && channel < 0.0) {
                    channel = 0.0;
                }
                if (!colorfreqtable.preserve_extended && channel > 255.0) {
                    channel = 255.0;
                }
                cluster_sums_local[offset + (size_t)component] = channel;
            }
            cluster_weight_local[cluster_index] = 1UL;
        }
    }
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        local_offset = (size_t)cluster_index * (size_t)SIXEL_MAX_CHANNELS;
        output_offset = (size_t)cluster_index * (size_t)depth;
        cluster_weight[cluster_index] = cluster_weight_local[cluster_index];
        for (component = 0U; component < depth; ++component) {
            cluster_sums[output_offset + (size_t)component]
                = cluster_sums_local[local_offset + (size_t)component];
            /* A projected representative is no longer the unconstrained
             * mean; preserve its position when exporting weighted sums. */
            if (colorfreqtable.preserve_extended &&
                    sixel_palette_snap_destination_active() &&
                    sixel_palette_should_snap(
                        SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_ITER)) {
                cluster_sums[output_offset + (size_t)component] =
                    centers[local_offset + (size_t)component] *
                    (double)cluster_weight[cluster_index];
            }
        }
    }
    if (assignment != NULL) {
        free(assignment);
    }
    if (executed_iterations != NULL) {
        *executed_iterations = executed;
    }
}

static SIXELSTATUS
sixel_palette_clusters_to_colormap(unsigned long *weights,
                                   double *sums,
                                   unsigned int depth,
                                   unsigned int cluster_count,
                                   int use_reversible,
                                   int pixelformat,
                                   tupletable2 *colormapP,
                                   int preserve_extended,
                                   int method_for_rep,
                                   sixel_palette_rep_stats_t const *stats,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    tupletable2 colormap;
    unsigned int index;
    unsigned int plane;
    double component;
    unsigned long weight;

    status = SIXEL_BAD_ARGUMENT;
    if (colormapP == NULL) {
        return status;
    }
    colormap = newColorMap(cluster_count, depth, allocator);
    if (colormap.size == 0U) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0U; index < cluster_count; ++index) {
        weight = weights[index];
        if (weight == 0UL) {
            weight = 1UL;
        }
        colormap.table[index]->value
            = (unsigned int)((weight > (unsigned long)UINT_MAX)
                                 ? UINT_MAX
                                 : weight);
        for (plane = 0U; plane < depth; ++plane) {
            component = sums[(size_t)index * (size_t)depth + plane];
            component /= (double)weight;
            /* Empty Lloyd clusters retain the existing centroid fallback. */
            if (stats != NULL && stats[index].colors != 0U) {
                if (method_for_rep == SIXEL_REP_CENTER_BOX) {
                    component = (stats[index].min[plane]
                        + stats[index].max[plane]) / 2.0;
                } else if (method_for_rep == SIXEL_REP_AVERAGE_COLORS) {
                    component = stats[index].sums[plane]
                        / (double)stats[index].colors;
                }
            }
            if (!preserve_extended && component < 0.0) {
                component = 0.0;
            }
            if (!preserve_extended && component > 255.0) {
                component = 255.0;
            }
            colormap.table[index]->components[plane]
                = preserve_extended ? component : floor(component + 0.5);
        }
        if (use_reversible && !sixel_palette_snap_destination_active()) {
            sixel_palette_reversible_tuple(colormap.table[index]->components,
                                          depth,
                                          pixelformat);
        }
    }
    *colormapP = colormap;
    status = SIXEL_OK;

    return status;
}

static void
sixel_palette_heckbert_compute_box_median(tupletable2 const colorfreqtable,
                                          unsigned int boxStart,
                                          unsigned int boxSize,
                                          unsigned int sum,
                                          unsigned int *median_index,
                                          unsigned int *lower_sum)
{
    unsigned int index;
    unsigned int lowersum;
    unsigned int weight;
    unsigned int half;

    index = 1U;
    lowersum = 0U;
    weight = 0U;
    half = sum / 2U;
    if (boxSize == 0U) {
        *median_index = 0U;
        *lower_sum = 0U;
        return;
    }

    weight = sixel_palette_heckbert_entry_weight(
        colorfreqtable.table[boxStart]);
    lowersum = weight;
    while (index < boxSize - 1U && lowersum < half) {
        weight = sixel_palette_heckbert_entry_weight(
            colorfreqtable.table[boxStart + index]);
        lowersum += weight;
        index += 1U;
    }

    if (index == 0U) {
        index = 1U;
    }
    if (index >= boxSize) {
        index = boxSize - 1U;
    }
    *median_index = index;
    *lower_sum = lowersum;
}

/* Evaluate all nonempty cuts in a fully sorted range in linear time.
 * Centering on the first tuple avoids subtracting large common offsets.
 * Ties keep the earliest cut; accumulation order is serial and fixed. */
static double
sixel_heckbert_sse(double const sums[4], double const squares[4],
                   double population, unsigned int depth)
{
    unsigned int plane;
    double error;
    double component;

    error = 0.0;
    if (population <= 0.0) {
        return 0.0;
    }
    for (plane = 0U; plane < depth; ++plane) {
        component = squares[plane] - sums[plane] * sums[plane] / population;
        error += component > 0.0 ? component : 0.0;
    }
    return error;
}

#if SIXEL_SIMD_NEON_A64
/* Three-plane form of sixel_heckbert_box_sse. Each plane holds its sum and
 * square sum in the two lanes of one vector, and the cut search scores both
 * children in the lanes of one vector. Every lane takes the operations of the
 * scalar loop in the same order, fusing the same multiply-adds, so the cut
 * and the errors are the same values. */
/* Accel: HECKBERT-BOX-SCORE neon */
static double
sixel_heckbert_box_sse_neon3(tupletable2 table, unsigned int start,
                             unsigned int size, double const weights[4],
                             unsigned int *cut, unsigned int *lower,
                             double children[2])
{
    float64x2_t total[3];
    float64x2_t left[3];
    float64x2_t zero;
    float64x2_t factor;
    float64x2_t right;
    float64x2_t sums;
    float64x2_t squares;
    float64x2_t populations;
    float64x2_t errors;
    float64x2_t component;
    double origin[3];
    double sum_list[4];
    double square_list[4];
    double population;
    double left_population;
    double weight;
    double value;
    double best;
    double score;
    sixel_heckbert_tuple_t const *entry;
    unsigned int i;
    unsigned int plane;

    zero = vdupq_n_f64(0.0);
    for (plane = 0U; plane < 3U; ++plane) {
        origin[plane] = table.table[start]->components[plane];
        total[plane] = zero;
        left[plane] = zero;
    }
    population = 0.0;
    for (i = 0U; i < size; ++i) {
        entry = table.table[start + i];
        weight = (double)sixel_palette_heckbert_entry_weight(entry);
        population += weight;
        for (plane = 0U; plane < 3U; ++plane) {
            value = (entry->components[plane] - origin[plane]) * weights[plane];
            factor = vsetq_lane_f64(weight * value, vdupq_n_f64(weight), 1);
            total[plane] = vfmaq_f64(total[plane], factor,
                vdupq_n_f64(value));
        }
    }
    for (plane = 0U; plane < 3U; ++plane) {
        sum_list[plane] = vgetq_lane_f64(total[plane], 0);
        square_list[plane] = vgetq_lane_f64(total[plane], 1);
    }
    sum_list[3] = square_list[3] = 0.0;
    if (cut == NULL) {
        return sixel_heckbert_sse(sum_list, square_list, population, 3U);
    }
    *cut = 0U;
    *lower = 0U;
    children[0] = children[1] = 0.0;
    left_population = 0.0;
    best = -1.0;
    /* Both children hold at least one entry of weight one or more, so
     * neither population is zero here. */
    for (i = 0U; i + 1U < size; ++i) {
        entry = table.table[start + i];
        weight = (double)sixel_palette_heckbert_entry_weight(entry);
        left_population += weight;
        populations = vsetq_lane_f64(population - left_population,
            vdupq_n_f64(left_population), 1);
        errors = zero;
        for (plane = 0U; plane < 3U; ++plane) {
            value = (entry->components[plane] - origin[plane]) * weights[plane];
            factor = vsetq_lane_f64(weight * value, vdupq_n_f64(weight), 1);
            left[plane] = vfmaq_f64(left[plane], factor, vdupq_n_f64(value));
            right = vsubq_f64(total[plane], left[plane]);
            sums = vzip1q_f64(left[plane], right);
            squares = vzip2q_f64(left[plane], right);
            component = vsubq_f64(squares,
                vdivq_f64(vmulq_f64(sums, sums), populations));
            errors = vaddq_f64(errors, vmaxnmq_f64(component, zero));
        }
        score = vgetq_lane_f64(errors, 0) + vgetq_lane_f64(errors, 1);
        if (i == 0U || score < best) {
            best = score;
            *cut = i + 1U;
            *lower = (unsigned int)left_population;
            children[0] = vgetq_lane_f64(errors, 0);
            children[1] = vgetq_lane_f64(errors, 1);
        }
    }
    return sixel_heckbert_sse(sum_list, square_list, population, 3U);
}
#endif

#if SIXEL_SIMD_AVX2 && HAVE_DOUBLE_EVAL_NO_EXCESS
/* AVX2 rounds each operation to binary64. A scalar compiler that keeps
 * intermediate results wider can choose a different cut. */
/* Keep RGB accumulation serial within each lane. The fourth lane is zero,
 * and the final reduction follows scalar plane order (including +0). No
 * fused multiply-add is used: AVX2 does not imply FMA support.
 * Vectors arrive by address: 32-bit x86 and Cygwin/MSYS x86-64 GCC pass a
 * by-value __m256d through a stack slot without 32-byte alignment, then
 * load it with an aligned move that faults. */
/* Accel: HECKBERT-BOX-SCORE avx2 */
static SIXEL_TARGET_AVX2 double
sixel_heckbert_error_avx2(__m256d const *sums, __m256d const *squares,
                          double population)
{
    __m256d component;
    double values[4];
    double error;

    if (population <= 0.0) {
        return 0.0;
    }
    component = _mm256_sub_pd(*squares, _mm256_div_pd(
        _mm256_mul_pd(*sums, *sums), _mm256_set1_pd(population)));
    component = _mm256_and_pd(component, _mm256_cmp_pd(component,
        _mm256_setzero_pd(), _CMP_GT_OQ));
    _mm256_storeu_pd(values, component);
    error = 0.0;
    error += values[0];
    error += values[1];
    error += values[2];
    return error;
}

/* A masked load reads exactly three doubles even at the last tuple. Each
 * RGB lane performs the scalar products and additions in the same order;
 * strict score comparison retains the earliest cut on ties. */
/* Accel: HECKBERT-BOX-SCORE avx2 */
static SIXEL_TARGET_AVX2 double
sixel_heckbert_box_sse_avx2(tupletable2 table, unsigned int start,
                           unsigned int size, double const weights[4],
                           unsigned int *cut, unsigned int *lower,
                           double children[2])
{
    __m256i mask;
    __m256d origin;
    __m256d factors;
    __m256d sums;
    __m256d squares;
    __m256d left_sums;
    __m256d left_squares;
    __m256d right_sums;
    __m256d right_squares;
    __m256d value;
    __m256d product;
    double population;
    double left_population;
    double weight;
    double best;
    double errors[2];
    sixel_heckbert_tuple_t const *entry;
    unsigned int i;

    /* Empty boxes have no origin tuple to load. */
    if (size == 0U) {
        if (cut != NULL) {
            *cut = 0U;
            *lower = 0U;
            children[0] = children[1] = 0.0;
        }
        return 0.0;
    }
    mask = _mm256_set_epi64x(0, -1, -1, -1);
    origin = _mm256_maskload_pd(table.table[start]->components, mask);
    factors = _mm256_maskload_pd(weights, mask);
    sums = squares = _mm256_setzero_pd();
    left_sums = left_squares = _mm256_setzero_pd();
    population = 0.0;
    for (i = 0U; i < size; ++i) {
        entry = table.table[start + i];
        weight = (double)sixel_palette_heckbert_entry_weight(entry);
        population += weight;
        value = _mm256_mul_pd(_mm256_sub_pd(
            _mm256_maskload_pd(entry->components, mask), origin), factors);
        product = _mm256_mul_pd(_mm256_set1_pd(weight), value);
        sums = _mm256_add_pd(sums, product);
        squares = _mm256_add_pd(squares, _mm256_mul_pd(product, value));
    }
    if (cut == NULL) {
        return sixel_heckbert_error_avx2(&sums, &squares, population);
    }
    *cut = 0U;
    *lower = 0U;
    children[0] = children[1] = 0.0;
    left_population = 0.0;
    best = -1.0;
    for (i = 0U; i + 1U < size; ++i) {
        entry = table.table[start + i];
        weight = (double)sixel_palette_heckbert_entry_weight(entry);
        left_population += weight;
        value = _mm256_mul_pd(_mm256_sub_pd(
            _mm256_maskload_pd(entry->components, mask), origin), factors);
        product = _mm256_mul_pd(_mm256_set1_pd(weight), value);
        left_sums = _mm256_add_pd(left_sums, product);
        left_squares = _mm256_add_pd(left_squares,
            _mm256_mul_pd(product, value));
        right_sums = _mm256_sub_pd(sums, left_sums);
        right_squares = _mm256_sub_pd(squares, left_squares);
        errors[0] = sixel_heckbert_error_avx2(&left_sums, &left_squares,
            left_population);
        errors[1] = sixel_heckbert_error_avx2(&right_sums, &right_squares,
            population - left_population);
        if (i == 0U || errors[0] + errors[1] < best) {
            best = errors[0] + errors[1];
            *cut = i + 1U;
            *lower = (unsigned int)left_population;
            children[0] = errors[0];
            children[1] = errors[1];
        }
    }
    return sixel_heckbert_error_avx2(&sums, &squares, population);
}
#endif

static double
sixel_heckbert_box_sse(tupletable2 table, unsigned int start,
                       unsigned int size, unsigned int depth,
                       double const weights[4], unsigned int *cut,
                       unsigned int *lower, double children[2])
{
    double sums[4];
    double squares[4];
    double left_sums[4];
    double left_squares[4];
    double right_sums[4];
    double right_squares[4];
    double population;
    double left_population;
    double weight;
    double value;
    double best;
    double errors[2];
    unsigned int i;
    unsigned int plane;

#if SIXEL_SIMD_AVX2 && HAVE_DOUBLE_EVAL_NO_EXCESS
    if (depth == 3U
            && sixel_cpu_simd_level() == SIXEL_SIMD_LEVEL_AVX2) {
        return sixel_heckbert_box_sse_avx2(table, start, size, weights,
            cut, lower, children);
    }
#endif
#if SIXEL_SIMD_NEON_A64
    if (depth == 3U) {
        return sixel_heckbert_box_sse_neon3(table, start, size, weights,
                                            cut, lower, children);
    }
#endif
    memset(sums, 0, sizeof(sums));
    memset(squares, 0, sizeof(squares));
    memset(left_sums, 0, sizeof(left_sums));
    memset(left_squares, 0, sizeof(left_squares));
    population = 0.0;
    left_population = 0.0;
    best = -1.0;
    for (i = 0U; i < size; ++i) {
        weight = (double)sixel_palette_heckbert_entry_weight(
            table.table[start + i]);
        population += weight;
        for (plane = 0U; plane < depth; ++plane) {
            value = (table.table[start + i]->components[plane]
                - table.table[start]->components[plane]) * weights[plane];
            sums[plane] += weight * value;
            squares[plane] += weight * value * value;
        }
    }
    if (cut == NULL) {
        return sixel_heckbert_sse(sums, squares, population, depth);
    }
    /* An unsplittable box reports an empty cut to the caller. */
    *cut = 0U;
    *lower = 0U;
    children[0] = children[1] = 0.0;
    for (i = 0U; i + 1U < size; ++i) {
        weight = (double)sixel_palette_heckbert_entry_weight(
            table.table[start + i]);
        left_population += weight;
        for (plane = 0U; plane < depth; ++plane) {
            value = (table.table[start + i]->components[plane]
                - table.table[start]->components[plane]) * weights[plane];
            left_sums[plane] += weight * value;
            left_squares[plane] += weight * value * value;
            right_sums[plane] = sums[plane] - left_sums[plane];
            right_squares[plane] = squares[plane] - left_squares[plane];
        }
        errors[0] = sixel_heckbert_sse(left_sums, left_squares,
            left_population, depth);
        errors[1] = sixel_heckbert_sse(right_sums, right_squares,
            population - left_population, depth);
        if (i == 0U || errors[0] + errors[1] < best) {
            best = errors[0] + errors[1];
            *cut = i + 1U;
            *lower = (unsigned int)left_population;
            children[0] = errors[0];
            children[1] = errors[1];
        }
    }
    return sixel_heckbert_sse(sums, squares, population, depth);
}

/*
 * Detect partitions that would produce an empty or nearly-empty child box.
 * splitBox retries with fallback axes when this returns true.
 */
static int
sixel_palette_heckbert_split_is_degenerate(unsigned int boxSize,
                                           unsigned int medianIndex,
                                           unsigned int lowersum,
                                           unsigned int sum)
{
    if (boxSize < 2U) {
        return 1;
    }
    if (medianIndex == 0U || medianIndex >= boxSize) {
        return 1;
    }
    if (lowersum == 0U || lowersum >= sum) {
        return 1;
    }
    if (boxSize >= 4U
            && (medianIndex <= 1U || medianIndex >= boxSize - 1U)) {
        return 1;
    }

    return 0;
}

/*
 * Attempt one split strategy (PCA or axis bucket sort), then compute the
 * weighted median split point on the reordered tuple range.
 */
static SIXELSTATUS
sixel_palette_heckbert_split_attempt(
    tupletable2 const colorfreqtable,
    unsigned int depth,
    unsigned int boxStart,
    unsigned int boxSize,
    sample minval[],
    sample maxval[],
    unsigned int axis_count[SIXEL_HECKBERT_MAX_CHANNELS]
                           [sixel_palette_heckbert_axis_bins],
    int methodForLargest,
    unsigned int sum,
    unsigned int dimensions,
    struct sixel_pca_sort_entry *pca_workspace,
    unsigned int pca_workspace_capacity,
    unsigned char *axis_keys,
    unsigned int *medianIndex,
    unsigned int *lowersum,
    unsigned int *axis_used,
    double *pca_ratio,
    int cut_sse,
    double const weights[4],
    double children[2])
{
    SIXELSTATUS status;
    unsigned int axis;
    unsigned int i;
    double pca_axis[3];
    int use_pca;

    status = SIXEL_FALSE;
    axis = 0U;
    i = 0U;
    use_pca = 0;
    if (pca_ratio != NULL) {
        *pca_ratio = 0.0;
    }

    if (methodForLargest == SIXEL_LARGE_PCA) {
        use_pca = computePcaAxis(colorfreqtable,
                                 dimensions,
                                 boxStart,
                                 boxSize,
                                 pca_axis,
                                 pca_ratio);
        if (!use_pca) {
            return SIXEL_FALSE;
        }
        compareplanePcaDimensions = dimensions;
        compareplaneTieDepth = depth;
        for (i = 0U; i < 3U; ++i) {
            compareplanePcaAxis[i] = pca_axis[i];
        }
        if (cut_sse) {
            status = sortBoxByPcaProjection(colorfreqtable, boxStart,
                boxSize, depth, dimensions, pca_axis,
                pca_workspace, pca_workspace_capacity);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            sixel_heckbert_box_sse(colorfreqtable, boxStart, boxSize,
                depth, weights, medianIndex, lowersum, children);
            return SIXEL_OK;
        }
        status = partitionBoxByPcaMedian(colorfreqtable,
                                         boxStart,
                                         boxSize,
                                         depth,
                                         dimensions,
                                         pca_axis,
                                         sum,
                                         pca_workspace,
                                         pca_workspace_capacity,
                                         medianIndex,
                                         lowersum);
        if (status == SIXEL_BAD_ALLOCATION) {
            status = sortBoxByPcaProjection(colorfreqtable,
                                            boxStart,
                                            boxSize,
                                            depth,
                                            dimensions,
                                            pca_axis,
                                            pca_workspace,
                                            pca_workspace_capacity);
        }
        if (status == SIXEL_BAD_ALLOCATION) {
            qsort((char *)&colorfreqtable.table[boxStart],
                  boxSize,
                  sizeof(colorfreqtable.table[boxStart]),
                  compareplanePca);
            sixel_palette_heckbert_compute_box_median(colorfreqtable,
                                                      boxStart,
                                                      boxSize,
                                                      sum,
                                                      medianIndex,
                                                      lowersum);
            status = SIXEL_OK;
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (axis_used != NULL) {
            *axis_used = 0U;
        }
        return SIXEL_OK;
    } else {
        axis = largestByWeights(minval, maxval, depth, weights);
        if (colorfreqtable.preserve_extended) {
            for (i = 0U; i < 3U; ++i) {
                pca_axis[i] = i == axis ? 1.0 : 0.0;
            }
            if (axis_used != NULL) {
                *axis_used = axis;
            }
            if (cut_sse) {
                status = sortBoxByPcaProjection(colorfreqtable, boxStart,
                    boxSize, depth, dimensions, pca_axis,
                    pca_workspace, pca_workspace_capacity);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                sixel_heckbert_box_sse(colorfreqtable, boxStart, boxSize,
                    depth, weights, medianIndex, lowersum, children);
                return SIXEL_OK;
            }
            return partitionBoxByPcaMedian(colorfreqtable,
                boxStart, boxSize, depth, dimensions, pca_axis, sum,
                pca_workspace, pca_workspace_capacity,
                medianIndex, lowersum);
        }
        status = sortBoxByAxisBuckets(colorfreqtable,
                                      boxStart,
                                      boxSize,
                                      axis,
                                      axis_count,
                                      minval,
                                      maxval,
                                      axis_keys);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (axis_used != NULL) {
            *axis_used = axis;
        }
    }

    if (cut_sse) {
        sixel_heckbert_box_sse(colorfreqtable, boxStart, boxSize,
            depth, weights, medianIndex, lowersum, children);
        return SIXEL_OK;
    }
    sixel_palette_heckbert_compute_box_median(colorfreqtable,
                                              boxStart,
                                              boxSize,
                                              sum,
                                              medianIndex,
                                              lowersum);

    return SIXEL_OK;
}

static SIXELSTATUS
splitBox(boxVector bv,
         unsigned int *boxesP,
         unsigned int bi,
         tupletable2 const colorfreqtable,
         unsigned int depth,
         int methodForLargest,
         struct sixel_pca_sort_entry *pca_workspace,
         unsigned int pca_workspace_capacity,
         unsigned char *axis_keys,
         sixel_heckbert_options_t const *options,
         int pixelformat)
{
    SIXELSTATUS status;
    unsigned int boxStart;
    unsigned int boxSize;
    unsigned int sm;
    enum { max_depth = 16 };
    sample minval[max_depth];
    sample maxval[max_depth];
    unsigned int medianIndex;
    unsigned int lowersum;
    unsigned int dimensions;
    unsigned long total_weight;
    unsigned int axis_used;
    double pca_ratio;
    int split_ok;
    unsigned int split_methods[3];
    unsigned int split_method_count;
    unsigned int split_attempt;
    unsigned int old_boxes;
    unsigned int axis_count[SIXEL_HECKBERT_MAX_CHANNELS]
                           [sixel_palette_heckbert_axis_bins];
    unsigned int (*axis_count_ptr)[sixel_palette_heckbert_axis_bins];
    int split_method;
    int degenerate;
    double weights[4];
    double children[2];

    status = SIXEL_FALSE;
    boxStart = bv[bi].ind;
    boxSize = bv[bi].colors;
    sm = bv[bi].sum;
    dimensions = (depth < 3U) ? depth : 3U;
    total_weight = 0UL;
    axis_used = 0U;
    pca_ratio = 0.0;
    split_ok = 0;
    split_method_count = 0U;
    split_attempt = 0U;
    old_boxes = 0U;
    axis_count_ptr = NULL;
    split_method = methodForLargest;
    degenerate = 0;
    if (methodForLargest != SIXEL_LARGE_NORM
            && methodForLargest != SIXEL_LARGE_LUM
            && methodForLargest != SIXEL_LARGE_PCA) {
        sixel_helper_set_additional_message(
            "Internal error: invalid value of methodForLargest.");
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    if (!colorfreqtable.preserve_extended &&
            methodForLargest != SIXEL_LARGE_PCA) {
        axis_count_ptr = axis_count;
    }
    findBoxBoundariesAndWeight(colorfreqtable,
                               depth,
                               boxStart,
                               boxSize,
                               minval,
                               maxval,
                               &total_weight,
                               axis_count_ptr);
    if (total_weight == 0UL) {
        sixel_helper_set_additional_message(
            "Internal error: empty histogram in splitBox.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    sm = (total_weight > (unsigned long)UINT_MAX)
             ? UINT_MAX
             : (unsigned int)total_weight;
    if (methodForLargest == SIXEL_LARGE_PCA) {
        split_methods[split_method_count++] = SIXEL_LARGE_PCA;
        split_methods[split_method_count++] = SIXEL_LARGE_NORM;
        split_methods[split_method_count++] = SIXEL_LARGE_LUM;
    } else if (methodForLargest == SIXEL_LARGE_LUM) {
        split_methods[split_method_count++] = SIXEL_LARGE_LUM;
        split_methods[split_method_count++] = SIXEL_LARGE_NORM;
    } else {
        split_methods[split_method_count++] = SIXEL_LARGE_NORM;
        split_methods[split_method_count++] = SIXEL_LARGE_LUM;
    }

    medianIndex = 1U;
    lowersum = 1U;
    /*
     * Try preferred strategy first, then fallback strategies when the split is
     * too imbalanced.  This avoids pathological one-color-vs-rest partitions.
     */
    for (split_attempt = 0U; split_attempt < split_method_count;
            ++split_attempt) {
        split_method = (int)split_methods[split_attempt];
        if (!colorfreqtable.preserve_extended &&
                split_method != SIXEL_LARGE_PCA && axis_count_ptr == NULL) {
            findBoxBoundariesAndWeight(colorfreqtable,
                                       depth,
                                       boxStart,
                                       boxSize,
                                       minval,
                                       maxval,
                                       NULL,
                                       axis_count);
            axis_count_ptr = axis_count;
        }
        sixel_heckbert_axis_weights(pixelformat, split_method,
            options, weights);
        /* A fallback from norm/PCA is part of their compatibility contract,
         * not an explicit request for perceptual luminosity weighting. */
        if (split_method == SIXEL_LARGE_LUM &&
                methodForLargest != SIXEL_LARGE_LUM) {
            sixel_heckbert_axis_weights(pixelformat, SIXEL_LARGE_NORM,
                options, weights);
            weights[0] *= 0.2989;
            weights[1] *= 0.5866;
            weights[2] *= 0.1145;
        }
        status = sixel_palette_heckbert_split_attempt(colorfreqtable,
                                                      depth,
                                                      boxStart,
                                                      boxSize,
                                                      minval,
                                                      maxval,
                                                      axis_count_ptr,
                                                      split_method,
                                                      sm,
                                                      dimensions,
                                                      pca_workspace,
                                                      pca_workspace_capacity,
                                                      axis_keys,
                                                      &medianIndex,
                                                      &lowersum,
                                                      &axis_used,
                                                      &pca_ratio,
                                                      options->cut_sse,
                                                      weights, children);
        if (SIXEL_FAILED(status)) {
            if (split_method == SIXEL_LARGE_PCA) {
                sixel_debugf("PCA fallback to range axis on box %u", bi);
                status = SIXEL_FALSE;
                continue;
            }
            goto end;
        }
        split_ok = 1;
        if (split_method == SIXEL_LARGE_PCA) {
            sixel_debugf("box %u: PCA split (PC1 ratio %.3f)",
                         bi,
                         pca_ratio);
        } else {
            sixel_debugf("box %u: axis split method=%d axis=%u",
                         bi,
                         split_method,
                         axis_used);
        }
        degenerate = sixel_palette_heckbert_split_is_degenerate(boxSize,
                                                                medianIndex,
                                                                lowersum,
                                                                sm);
        /* A singleton outlier can be the SSE optimum; reject only empty
         * children, not an intentionally unbalanced minimum-error cut. */
        if (options->cut_sse) {
            degenerate = medianIndex == 0U || medianIndex >= boxSize ||
                lowersum == 0U || lowersum >= sm;
        }
        if (!degenerate) {
            break;
        }
        if (split_attempt + 1U < split_method_count) {
            sixel_debugf("box %u: split imbalance on method=%d; fallback",
                         bi,
                         split_method);
        }
    }
    if (!split_ok) {
        sixel_helper_set_additional_message(
            "Internal error: splitBox failed to select an axis.");
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    old_boxes = *boxesP;
    bv[bi].colors = medianIndex;
    bv[bi].sum = lowersum;
    bv[old_boxes].ind = boxStart + medianIndex;
    bv[old_boxes].colors = boxSize - medianIndex;
    bv[old_boxes].sum = sm - lowersum;
    bv[bi].priority = (double)bv[bi].sum;
    bv[old_boxes].priority = (double)bv[old_boxes].sum;
    if (options->order_sse) {
        /* Reuse the cut's moments when available. Median cuts need one
         * statistics pass per child, but never recompute untouched boxes. */
        if (!options->cut_sse) {
            /* Box ordering uses one metric even when a median split falls
             * back to a different axis-selection heuristic. */
            sixel_heckbert_axis_weights(pixelformat, methodForLargest,
                options, weights);
            children[0] = sixel_heckbert_box_sse(colorfreqtable, boxStart,
                medianIndex, depth, weights, NULL, NULL, NULL);
            children[1] = sixel_heckbert_box_sse(colorfreqtable,
                boxStart + medianIndex, boxSize - medianIndex,
                depth, weights, NULL, NULL, NULL);
        }
        bv[bi].priority = children[0];
        bv[old_boxes].priority = children[1];
    }
    *boxesP = old_boxes + 1U;
    sixel_palette_heckbert_reposition_box(bv, old_boxes, bi);
    sixel_palette_heckbert_reposition_box(bv, *boxesP, old_boxes);
    status = SIXEL_OK;

end:
    return status;
}

/* PCC debug-info generation can fail on long stack parameter lists.
 * Share this request across colormap construction and median-cut telemetry. */
typedef struct sixel_palette_heckbert_colormap_request {
    sixel_palette_t *palette;
    sixel_heckbert_histogram_t const *histogram;
    unsigned int depth;
    int use_reversible;
    tupletable2 *colormap;
    unsigned int *origcolors;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_timeline_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    double *iterate_ms;
    unsigned int *iterate_count;
    double *merge_ms;
    unsigned int *merge_iterate_count;
    int *merge_mode;
} sixel_palette_heckbert_colormap_request_t;

#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
/* Solve every split with the quant/heckbert.gpu component. On success the
 * table holds the boxes' entries in box order and bv the boxes in sumcompare
 * order, as the CPU loop leaves them. FEATURE_ERROR leaves both untouched so
 * the caller can solve on the CPU. */
static SIXELSTATUS
sixel_palette_heckbert_solve_gpu(tupletable2 const colorfreqtable,
                                 unsigned int working_colors,
                                 int methodForLargest,
                                 int pixelformat,
                                 sixel_heckbert_options_t const *options,
                                 boxVector bv,
                                 unsigned int *boxes,
                                 sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    sixel_heckbert_solver_interface_t *solver;
    sixel_heckbert_solve_request_t request;
    void *service;
    double *tuples;
    unsigned int *weights;
    unsigned int *order;
    unsigned int *box_fields;
    double *box_priority;
    sixel_heckbert_tuple_t **entries;
    double axis_weights[4];
    unsigned int count;
    unsigned int box_count;
    unsigned int i;

    count = colorfreqtable.size;
    solver = NULL;
    service = NULL;
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;
    status = factory->vtbl->create(factory, "quant/heckbert.gpu", allocator,
        (void **)&solver);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    tuples = sixel_allocator_malloc(allocator,
        (size_t)count * 3u * sizeof(double));
    weights = sixel_allocator_malloc(allocator,
        (size_t)count * sizeof(unsigned int));
    order = sixel_allocator_malloc(allocator,
        (size_t)count * sizeof(unsigned int));
    entries = sixel_allocator_malloc(allocator,
        (size_t)count * sizeof(*entries));
    box_fields = sixel_allocator_malloc(allocator,
        (size_t)working_colors * 3u * sizeof(unsigned int));
    box_priority = sixel_allocator_malloc(allocator,
        (size_t)working_colors * sizeof(double));
    if (tuples == NULL || weights == NULL || order == NULL ||
            entries == NULL || box_fields == NULL || box_priority == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (i = 0U; i < count; ++i) {
        tuples[i * 3U + 0U] = colorfreqtable.table[i]->components[0];
        tuples[i * 3U + 1U] = colorfreqtable.table[i]->components[1];
        tuples[i * 3U + 2U] = colorfreqtable.table[i]->components[2];
        weights[i] = sixel_palette_heckbert_entry_weight(
            colorfreqtable.table[i]);
    }
    sixel_heckbert_axis_weights(pixelformat, methodForLargest, options,
        axis_weights);
    memset(&request, 0, sizeof(request));
    request.tuples = tuples;
    request.weights = weights;
    request.axis_weights = axis_weights;
    request.count = count;
    request.colors = working_colors;
    request.method_for_largest = methodForLargest;
    request.cut_sse = options->cut_sse;
    request.order_sse = options->order_sse;
    request.order = order;
    request.box_start = box_fields;
    request.box_size = box_fields + working_colors;
    request.box_sum = box_fields + 2U * working_colors;
    request.box_priority = box_priority;
    request.box_count = &box_count;
    status = solver->vtbl->solve(solver, &request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (i = 0U; i < count; ++i) {
        entries[i] = colorfreqtable.table[order[i]];
    }
    memcpy(colorfreqtable.table, entries, (size_t)count * sizeof(*entries));
    for (i = 0U; i < box_count; ++i) {
        bv[i].ind = request.box_start[i];
        bv[i].colors = request.box_size[i];
        bv[i].sum = request.box_sum[i];
        bv[i].priority = box_priority[i];
    }
    qsort(bv, box_count, sizeof(*bv), sumcompare);
    *boxes = box_count;
end:
    sixel_allocator_free(allocator, tuples);
    sixel_allocator_free(allocator, weights);
    sixel_allocator_free(allocator, order);
    sixel_allocator_free(allocator, entries);
    sixel_allocator_free(allocator, box_fields);
    sixel_allocator_free(allocator, box_priority);
    solver->vtbl->unref(solver);
    return status;
}

#endif

static SIXELSTATUS
mediancut(tupletable2 const colorfreqtable,
          unsigned int depth,
          unsigned int newcolors,
          int methodForLargest,
          int methodForRep,
          int use_reversible,
          int final_merge_mode,
          int pixelformat,
          tupletable2 *colormapP,
          sixel_allocator_t *allocator,
          sixel_palette_heckbert_colormap_request_t const *request,
          sixel_heckbert_options_t const *options)
{
    SIXELSTATUS status;
    boxVector bv;
    unsigned int bi;
    unsigned int boxes;
    int multicolorBoxesExist;
    unsigned int i;
    unsigned int sum;
    unsigned int working_colors;
    int apply_merge;
    int resolved_merge;
    unsigned long *cluster_weight;
    double *cluster_sums;
    sixel_palette_rep_stats_t *rep_stats;
    int cluster_total;
    unsigned int plane;
    unsigned int offset;
    unsigned int size;
    unsigned long value;
    sixel_heckbert_tuple_t *entry;
    SIXELSTATUS merge_status;
    unsigned int iteration_limit;
    unsigned int iteration_actual;
    unsigned int boxes_before;
    unsigned int boxes_after;
    double iteration_wall_start;
    double iteration_wall_stop;
    int job_iteration;
    int job_merge;
    double merge_wall_start;
    double merge_wall_stop;
    char log_detail[128];
    struct sixel_pca_sort_entry *pca_workspace;
    unsigned int pca_workspace_capacity;
    unsigned char *axis_keys;
    int gpu_policy;

    status = SIXEL_FALSE;
    bv = NULL;
    cluster_weight = NULL;
    cluster_sums = NULL;
    rep_stats = NULL;
    cluster_total = 0;
    working_colors = newcolors;
    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    iteration_limit = 0U;
    iteration_actual = 0U;
    boxes_before = 0U;
    boxes_after = 0U;
    iteration_wall_start = 0.0;
    iteration_wall_stop = 0.0;
    job_iteration = -1;
    job_merge = -1;
    merge_wall_start = 0.0;
    merge_wall_stop = 0.0;
    log_detail[0] = '\0';
    pca_workspace = NULL;
    pca_workspace_capacity = 0U;
    axis_keys = NULL;
    sum = 0U;
    for (i = 0U; i < colorfreqtable.size; ++i) {
        sum += colorfreqtable.table[i]->value;
    }

    if (apply_merge) {
        working_colors = sixel_final_merge_target(newcolors,
                                                  final_merge_mode);
        if (working_colors > colorfreqtable.size) {
            working_colors = colorfreqtable.size;
        }
        sixel_debugf("overshoot: %u", working_colors);
    }
    if (working_colors == 0U) {
        working_colors = 1U;
    }

    bv = newBoxVector(colorfreqtable.size, sum, working_colors, allocator);
    if (bv == NULL) {
        goto end;
    }
    if ((methodForLargest == SIXEL_LARGE_PCA ||
            colorfreqtable.preserve_extended) && colorfreqtable.size > 1U) {
        pca_workspace = (struct sixel_pca_sort_entry *)sixel_allocator_malloc(
            allocator,
            colorfreqtable.size * sizeof(struct sixel_pca_sort_entry));
        if (pca_workspace != NULL) {
            pca_workspace_capacity = colorfreqtable.size;
        }
    }
    axis_keys = (unsigned char *)sixel_allocator_malloc(allocator,
        colorfreqtable.size);
    if (axis_keys == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate heckbert axis keys.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    boxes = 1U;
    multicolorBoxesExist = (colorfreqtable.size > 1U);
    /* Policy tests drive mediancut without a palette object. */
    gpu_policy = request->palette != NULL
        ? SIXEL_PALETTE_CONTEXT(request->palette)->gpu_policy
        : SIXEL_GPU_POLICY_OFF;
    /* The solver is optional even under the force policy: FEATURE_ERROR
     * means no device or a histogram outside the solver's domain (Vulkan
     * stops at 4096 entries), and median cut then stays on the CPU without
     * disabling the later GPU palette stages. Force skips only the Vulkan
     * headroom choice, so a supported histogram always reaches the GPU;
     * offload's headroom step always chooses the device. */
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    if (gpu_policy != SIXEL_GPU_POLICY_OFF && multicolorBoxesExist &&
            depth == 3U && !colorfreqtable.preserve_extended &&
            !apply_merge && !use_reversible &&
            methodForLargest == SIXEL_LARGE_NORM &&
            options->cut_sse && options->order_sse &&
            working_colors <= 256U
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
            && (gpu_policy == SIXEL_GPU_POLICY_FORCE ||
                sixel_palette_heckbert_gpu_vulkan_choose(gpu_policy))
#endif
            ) {
        status = sixel_palette_heckbert_solve_gpu(colorfreqtable,
            working_colors, methodForLargest, pixelformat, options, bv,
            &boxes, allocator);
        if (SIXEL_SUCCEEDED(status)) {
            multicolorBoxesExist = 0;
        } else if (status != SIXEL_FEATURE_ERROR) {
            goto end;
        }
    }
#endif
    while (boxes < working_colors && multicolorBoxesExist) {
        /* Return revoked loans before the next solver job. */
        sixel_thread_work_checkpoint();
        iteration_wall_start = sixel_timer_now();
        boxes_before = boxes;
        job_iteration = sixel_palette_heckbert_log_start(request->logger,
                                                         request->job_seq,
                                                         request->engine_name,
                                                         "palette/iterate",
                                                         "iterate");
        for (bi = 0U; bi < boxes && bv[bi].colors < 2U; ++bi) {
            ;
        }
        if (bi >= boxes) {
            multicolorBoxesExist = 0;
        } else {
            status = splitBox(bv,
                              &boxes,
                              bi,
                              colorfreqtable,
                              depth,
                              methodForLargest,
                              pca_workspace,
                              pca_workspace_capacity, axis_keys, options,
                              pixelformat);
            if (SIXEL_FAILED(status)) {
                iteration_wall_stop = sixel_timer_now();
                if (request->iterate_ms != NULL) {
                    *request->iterate_ms +=
                        (iteration_wall_stop - iteration_wall_start) * 1000.0;
                }
                if (request->iterate_count != NULL) {
                    *request->iterate_count += 1U;
                }
                (void)snprintf(log_detail,
                               sizeof(log_detail),
                               "boxes=%u->%u split=failed",
                               boxes_before,
                               boxes);
                sixel_palette_heckbert_log_finish(request->logger,
                                                  job_iteration,
                                                  request->engine_name,
                                                  "palette/iterate",
                                                  "iterate",
                                                  log_detail);
                goto end;
            }
        }
        iteration_wall_stop = sixel_timer_now();
        boxes_after = boxes;
        if (request->iterate_ms != NULL) {
            *request->iterate_ms += (iteration_wall_stop - iteration_wall_start)
                           * 1000.0;
        }
        if (request->iterate_count != NULL) {
            *request->iterate_count += 1U;
        }
        (void)snprintf(log_detail,
                       sizeof(log_detail),
                       "boxes=%u->%u multicolor=%d",
                       boxes_before,
                       boxes_after,
                       multicolorBoxesExist);
        sixel_palette_heckbert_log_finish(request->logger,
                                          job_iteration,
                                          request->engine_name,
                                          "palette/iterate",
                                          "iterate",
                                          log_detail);
    }

    if (apply_merge && boxes > newcolors) {
        cluster_weight = (unsigned long *)sixel_allocator_malloc(
            allocator,
            (size_t)boxes * sizeof(unsigned long));
        /*
         * Track the accumulated channel totals in double precision so the
         * Ward stage can preserve sub-8bit accuracy before the palette is
         * snapped back onto the SIXEL tone grid.
         */
            cluster_sums = (double *)sixel_allocator_malloc(
            allocator,
            (size_t)boxes * (size_t)depth * sizeof(double));
        if (cluster_weight == NULL || cluster_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (methodForRep == SIXEL_REP_CENTER_BOX ||
                methodForRep == SIXEL_REP_AVERAGE_COLORS) {
            rep_stats = sixel_allocator_calloc(allocator, boxes,
                sizeof(*rep_stats));
            if (rep_stats == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            for (bi = 0U; bi < boxes; ++bi) {
                for (i = 0U; i < bv[bi].colors; ++i) {
                    sixel_heckbert_rep_add(rep_stats + bi,
                        colorfreqtable.table[bv[bi].ind + i], depth);
                }
            }
        }
        for (bi = 0U; bi < boxes; ++bi) {
            offset = bv[bi].ind;
            size = bv[bi].colors;
            if (size == 0U) {
                cluster_weight[bi] = 0UL;
                for (plane = 0U; plane < depth; ++plane) {
                    cluster_sums[(size_t)bi * (size_t)depth + plane]
                        = 0.0;
                }
                continue;
            }
            if (size == 1U) {
                entry = colorfreqtable.table[offset];
                value = (unsigned long)entry->value;
                cluster_weight[bi] = value;
                for (plane = 0U; plane < depth; ++plane) {
                    cluster_sums[(size_t)bi * (size_t)depth + plane]
                        = (double)entry->components[plane] * (double)value;
                }
                continue;
            }
            cluster_weight[bi] = 0UL;
            for (plane = 0U; plane < depth; ++plane) {
                cluster_sums[(size_t)bi * (size_t)depth + plane] = 0.0;
            }
            for (i = 0U; i < size; ++i) {
                entry = colorfreqtable.table[offset + i];
                value = (unsigned long)entry->value;
                cluster_weight[bi] += value;
                for (plane = 0U; plane < depth; ++plane) {
                    cluster_sums[(size_t)bi * (size_t)depth + plane]
                        += (double)entry->components[plane] * (double)value;
                }
            }
        }
        merge_wall_start = sixel_timer_now();
        job_merge = sixel_palette_heckbert_log_start(request->logger,
                                                     request->job_seq,
                                                     request->engine_name,
                                                     "palette/merge",
                                                     "merge");
        cluster_total = sixel_palette_merge_representatives(cluster_weight,
                                                  cluster_sums,
                                                  depth,
                                                  gpu_policy,
                                                  (int)boxes,
                                                  (int)newcolors,
                                                  resolved_merge,
                                                  use_reversible,
                                                  pixelformat,
                                                  colorfreqtable
                                                      .preserve_extended,
                                                  rep_stats,
                                                  allocator);
        if (colorfreqtable.preserve_extended && cluster_total < 1) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > newcolors) {
            cluster_total = (int)newcolors;
        }
        /*
         * cluster_weight/cluster_sums are allocated for `boxes` entries.
         * Clamp merged cluster_count so downstream refinement and conversion
         * never index beyond those buffers.
         */
        if ((unsigned int)cluster_total > boxes) {
            cluster_total = (int)boxes;
        }
        if (cluster_total > 0) {
            sixel_final_merge_load_env();
            iteration_limit
                = sixel_final_merge_lloyd_iterations(resolved_merge);
            if (iteration_limit > 0U) {
                sixel_final_merge_lloyd_histogram(colorfreqtable,
                                                  depth,
                                                  (unsigned int)cluster_total,
                                                  cluster_weight,
                                                  cluster_sums,
                                                  iteration_limit,
                                                  &iteration_actual,
                                                  pixelformat,
                                                  rep_stats);
            }
        }
        merge_status = sixel_palette_clusters_to_colormap(
            cluster_weight, cluster_sums, depth,
            (unsigned int)cluster_total, use_reversible, pixelformat,
            colormapP, colorfreqtable.preserve_extended, methodForRep,
            rep_stats, allocator);
        merge_wall_stop = sixel_timer_now();
        if (request->merge_ms != NULL) {
            *request->merge_ms += (merge_wall_stop - merge_wall_start) * 1000.0;
        }
        if (request->merge_iterate_count != NULL) {
            *request->merge_iterate_count += iteration_actual;
        }
        if (request->merge_mode != NULL) {
            *request->merge_mode = resolved_merge;
        }
        (void)snprintf(log_detail,
                       sizeof(log_detail),
                       "boxes=%u->%d merge=%d refine=%u",
                       boxes,
                       cluster_total,
                       resolved_merge,
                       iteration_actual);
        sixel_palette_heckbert_log_finish(request->logger,
                                          job_merge,
                                          request->engine_name,
                                          "palette/merge",
                                          "merge",
                                          log_detail);
        if (SIXEL_FAILED(merge_status)) {
            status = merge_status;
            goto end;
        }
    } else {
        *colormapP = colormapFromBv(newcolors,
                                    bv,
                                    boxes,
                                    colorfreqtable,
                                    depth,
                                    methodForRep,
                                    use_reversible,
                                    pixelformat,
                                    allocator);
        if (colorfreqtable.preserve_extended && colormapP->size == 0U) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, rep_stats);
    if (bv != NULL) {
        sixel_allocator_free(allocator, bv);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weight != NULL) {
        sixel_allocator_free(allocator, cluster_weight);
    }
    if (pca_workspace != NULL) {
        sixel_allocator_free(allocator, pca_workspace);
    }
    sixel_allocator_free(allocator, axis_keys);

    return status;
}

static SIXELSTATUS
sixel_palette_heckbert_colormap(
    sixel_palette_heckbert_colormap_request_t const *request)
{
    sixel_heckbert_options_t defaults = {1, 1, {0.0, 0.0, 0.0}};
    sixel_heckbert_options_t const *options;
    SIXELSTATUS status;
    sixel_palette_t *palette;
    unsigned int depth;
    int use_reversible;
    tupletable2 *colormapP;
    unsigned int *origcolors;
    int pixelformat;
    sixel_allocator_t *allocator;
    tupletable2 colorfreqtable;
    unsigned int i;
    unsigned int n;
    int representative;

    status = SIXEL_FALSE;
    if (request == NULL || request->colormap == NULL
            || request->allocator == NULL || request->palette == NULL
            || request->histogram == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    palette = request->palette;
    options = SIXEL_PALETTE_CONTEXT(palette)->attempt != NULL
        ? &SIXEL_PALETTE_CONTEXT(palette)->attempt->heckbert : &defaults;
    /* Direct palette callers have no diffusion state. Use the dithered
     * default; dither initialization resolves NONE before reaching here. */
    representative = SIXEL_PALETTE_CONTEXT(palette)->method_for_rep;
    if (representative == SIXEL_REP_AUTO) {
        representative = SIXEL_REP_AVERAGE_COLORS;
    }
    sixel_trace_topic_message("palette_contract",
        "LSXHBC1|rep=%d|cut=%s|order=%s|weights=%.17g/%.17g/%.17g",
        representative, options->cut_sse ? "sse" : "median",
        options->order_sse ? "sse" : "population",
        options->weights[0], options->weights[1], options->weights[2]);
    depth = request->depth;
    use_reversible = request->use_reversible;
    colormapP = request->colormap;
    origcolors = request->origcolors;
    pixelformat = request->pixelformat;
    allocator = request->allocator;

    colormapP->size = 0U;
    colormapP->table = NULL;
    colorfreqtable = *request->histogram;
    if (origcolors != NULL) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size
            <= SIXEL_PALETTE_STORAGE(palette)->requested_colors) {
        sixel_debugf("Image already has few enough colors (<=%u). Keeping "
                     "same colors.",
                     SIXEL_PALETTE_STORAGE(palette)->requested_colors);
        colormapP->size = colorfreqtable.size;
        status = sixel_heckbert_alloc_table(&colormapP->table, depth,
                                           colorfreqtable.size, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        for (i = 0U; i < colorfreqtable.size; ++i) {
            colormapP->table[i]->value = colorfreqtable.table[i]->value;
            for (n = 0U; n < depth; ++n) {
                colormapP->table[i]->components[n]
                    = colorfreqtable.table[i]->components[n];
            }
            if (use_reversible && !sixel_palette_snap_destination_active()) {
                sixel_palette_reversible_tuple(
                    colormapP->table[i]->components,
                    depth,
                    pixelformat);
            }
        }
    } else {
        sixel_debugf("choosing %u colors...",
                     SIXEL_PALETTE_STORAGE(palette)->requested_colors);
        status = mediancut(colorfreqtable,
                           depth,
                           SIXEL_PALETTE_STORAGE(palette)
                               ->requested_colors,
                           SIXEL_PALETTE_CONTEXT(palette)->method_for_largest,
                           representative,
                           use_reversible,
                           SIXEL_PALETTE_CONTEXT(palette)->final_merge_mode,
                           pixelformat,
                           colormapP,
                           allocator,
                           request,
                           options);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_debugf("%u colors are chosen.", colorfreqtable.size);
    }
    if (SIXEL_PALETTE_CONTEXT(palette)->force_palette) {
        status = force_palette_completion(colormapP,
                                          depth,
                                          SIXEL_PALETTE_STORAGE(palette)
                                              ->requested_colors,
                                          colorfreqtable,
                                          allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    return status;
}

SIXELSTATUS
sixel_palette_build_heckbert(sixel_palette_t *palette,
                             sixel_heckbert_histogram_t const *histogram,
                             unsigned int depth,
                             int pixelformat,
                             sixel_allocator_t *allocator,
                             sixel_timeline_logger_t *logger,
                             int *job_seq,
                             char const *engine_name,
                             sixel_palette_telemetry_t *telemetry)
{
    SIXELSTATUS status;
    SIXELSTATUS drop_status;
    sixel_allocator_t *work_allocator;
    tupletable2 colormap;
    unsigned int origcolors;
    size_t payload_size;
    unsigned int index;
    unsigned int plane;
    int input_is_float32;
    float *float_entries;
    int float_stride;
    size_t float_entries_size;
    double component;
    int reversible_for_quantizer;
    sixel_palette_heckbert_colormap_request_t colormap_request;
    double wall_start;
    double init_stop;
    double export_start;
    double export_stop;
    double iterate_ms;
    double merge_ms;
    unsigned int iterate_count;
    unsigned int merge_iterate_count;
    int merge_mode;
    int job_init;
    int job_export;
    char log_detail[128];

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = SIXEL_PALETTE_STORAGE(palette)->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    wall_start = sixel_timer_now();
    init_stop = wall_start;
    export_start = wall_start;
    export_stop = wall_start;
    iterate_ms = 0.0;
    merge_ms = 0.0;
    iterate_count = 0U;
    merge_iterate_count = 0U;
    merge_mode = SIXEL_FINAL_MERGE_NONE;
    job_init = -1;
    job_export = -1;
    log_detail[0] = '\0';

    job_init = sixel_palette_heckbert_log_start(logger,
                                                job_seq,
                                                engine_name,
                                                "palette/init",
                                                "init");

    colormap.preserve_extended = 0;
    colormap.size = 0U;
    colormap.table = NULL;
    origcolors = 0U;
    input_is_float32 = SIXEL_WORKFORMAT_IS_FLOAT32(pixelformat);
    float_entries = NULL;
    float_stride = 0;
    reversible_for_quantizer = SIXEL_PALETTE_CONTEXT(palette)->use_reversible;
    colormap_request.palette = palette;
    colormap_request.histogram = histogram;
    colormap_request.depth = depth;
    colormap_request.use_reversible = reversible_for_quantizer;
    colormap_request.colormap = &colormap;
    colormap_request.origcolors = &origcolors;
    colormap_request.pixelformat = pixelformat;
    colormap_request.allocator = work_allocator;
    colormap_request.logger = logger;
    colormap_request.job_seq = job_seq;
    colormap_request.engine_name = engine_name;
    colormap_request.iterate_ms = &iterate_ms;
    colormap_request.iterate_count = &iterate_count;
    colormap_request.merge_ms = &merge_ms;
    colormap_request.merge_iterate_count = &merge_iterate_count;
    colormap_request.merge_mode = &merge_mode;
    status = sixel_palette_heckbert_colormap(&colormap_request);
    init_stop = sixel_timer_now();
    export_start = init_stop;
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    (void)snprintf(log_detail,
                   sizeof(log_detail),
                   "colors=%u source=%u",
                   colormap.size,
                   origcolors);
    sixel_palette_heckbert_log_finish(logger,
                                      job_init,
                                      engine_name,
                                      "palette/init",
                                      "init",
                                      log_detail);
    job_export = sixel_palette_heckbert_log_start(logger,
                                                 job_seq,
                                                 engine_name,
                                                 "palette/export",
                                                 "export");

    status = sixel_palette_resize(palette,
                                  colormap.size,
                                  (int)depth,
                                  work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (input_is_float32 && colormap.size > 0U && depth > 0U) {
        float_stride = (int)((size_t)depth * (size_t)sizeof(float));
        if (float_stride <= 0) {
            sixel_helper_set_additional_message(
                "sixel_palette_build_heckbert: invalid float stride.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (colormap.size > SIZE_MAX / depth / sizeof(float)) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        float_entries_size = (size_t)colormap.size * depth * sizeof(float);
        float_entries = (float *)sixel_allocator_malloc(
            work_allocator, float_entries_size);
        if (float_entries == NULL) {
            sixel_helper_set_additional_message(
                "sixel_palette_build_heckbert: float palette alloc failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    payload_size = (size_t)colormap.size * (size_t)depth;
    if (payload_size > 0U && SIXEL_PALETTE_STORAGE(palette)->entries != NULL
        && colormap.table != NULL) {
        for (index = 0U; index < colormap.size; ++index) {
            for (plane = 0U; plane < depth; ++plane) {
                if (input_is_float32) {
                    component = colormap.table[index]->components[plane]
                        / 255.0 * sixel_pixelformat_float_channel_range(
                            pixelformat, (int)plane);
                    float_entries[index * depth + plane] = (float)component;
                    SIXEL_PALETTE_STORAGE(palette)
                        ->entries[index * depth + plane] =
                        sixel_pixelformat_float_channel_to_byte(pixelformat,
                            (int)plane, (float)component);
                    continue;
                }
                /* The few-colors shortcut also exports bucket means. */
                SIXEL_PALETTE_STORAGE(palette)->entries[index * depth + plane]
                    = (unsigned char)floor(
                        colormap.table[index]->components[plane] + 0.5);
            }
        }
    }

    SIXEL_PALETTE_STORAGE(palette)->original_colors = origcolors;
    if (float_entries != NULL && float_stride > 0) {
        drop_status = sixel_palette_set_entries_float32(palette,
                                                        float_entries,
                                                        colormap.size,
                                                        float_stride,
                                                        work_allocator);
    } else {
        drop_status = sixel_palette_set_entries_float32(palette,
                                                        NULL,
                                                        0U,
                                                        0,
                                                        work_allocator);
    }
    if (SIXEL_FAILED(drop_status)) {
        status = drop_status;
        goto end;
    }
    export_stop = sixel_timer_now();
    (void)snprintf(log_detail,
                   sizeof(log_detail),
                   "colors=%u depth=%u",
                   SIXEL_PALETTE_STORAGE(palette)->entry_count,
                   depth);
    sixel_palette_heckbert_log_finish(logger,
                                      job_export,
                                      engine_name,
                                      "palette/export",
                                      "export",
                                      log_detail);
    status = SIXEL_OK;

end:
    if (telemetry != NULL) {
        double now;
        double init_span;
        double export_span;

        now = sixel_timer_now();
        if (init_stop < wall_start) {
            init_stop = now;
        }
        if (export_stop < export_start) {
            export_stop = now;
        }

        init_span = init_stop - wall_start;
        if (init_span < 0.0) {
            init_span = 0.0;
        }
        export_span = export_stop - export_start;
        if (export_span < 0.0) {
            export_span = 0.0;
        }

        telemetry->init_ms = init_span * 1000.0;
        telemetry->iterate_ms = iterate_ms;
        telemetry->merge_ms = merge_ms;
        telemetry->export_ms = export_span * 1000.0;
        telemetry->iterate_count = iterate_count;
        telemetry->merge_iterate_count = merge_iterate_count;
        telemetry->merge_mode = merge_mode;
    }

    if (float_entries != NULL) {
        sixel_allocator_free(work_allocator, float_entries);
    }
    if (colormap.table != NULL) {
        sixel_allocator_free(work_allocator, colormap.table);
    }

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
