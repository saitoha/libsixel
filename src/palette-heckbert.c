/*
 * SPDX-License-Identifier: MIT
 *
 * mediancut algorithm implementation is imported from pnmcolormap.c
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

#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_ASSERT
# include <assert.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "lut.h"
#include "palette-internal.h"
#include "palette.h"
#include "status.h"

/*
 * Allocate a tuple table with enough space to store "size" tuples of the
 * specified depth.  The helper is now private to the Heckbert implementation
 * because the k-means path keeps its samples in a separate reservoir.
 */
static SIXELSTATUS
alloctupletable(tupletable *result,
                unsigned int depth,
                unsigned int size,
                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    enum { message_buffer_size = 256 };
    char message[message_buffer_size];
    int nwrite;
    unsigned int mainTableSize;
    unsigned int tupleIntSize;
    unsigned int allocSize;
    void *pool;
    tupletable tbl;
    unsigned int i;

    status = SIXEL_FALSE;
    if (result == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (UINT_MAX / sizeof(struct tupleint) < size) {
        nwrite = sixel_compat_snprintf(message,
                                       sizeof(message),
                                       "size %u is too big for arithmetic",
                                       size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    mainTableSize = size * sizeof(struct tupleint *);
    if ((UINT_MAX - mainTableSize) / sizeof(struct tupleint) < size) {
        nwrite = sixel_compat_snprintf(message,
                                       sizeof(message),
                                       "size %u is too big for arithmetic",
                                       size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    tupleIntSize = sizeof(struct tupleint) + (depth - 1U) * sizeof(sample);
    if ((UINT_MAX - mainTableSize) / tupleIntSize < size) {
        nwrite = sixel_compat_snprintf(message,
                                       sizeof(message),
                                       "size %u is too big for arithmetic",
                                       size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    allocSize = mainTableSize + size * tupleIntSize;

    pool = sixel_allocator_malloc(allocator, allocSize);
    if (pool == NULL) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "unable to allocate %u bytes for a %u-entry "
                              "tuple table",
                              allocSize,
                              size);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    tbl = (tupletable)pool;

    for (i = 0U; i < size; ++i) {
        tbl[i] = (struct tupleint *)((char *)pool
            + mainTableSize + i * tupleIntSize);
    }

    *result = tbl;

    status = SIXEL_OK;

end:
    if (status != SIXEL_OK) {
        *result = NULL;
    }

    return status;
}

/*
 * Determine the dense histogram size for a given depth and quantization
 * policy.  The ladder illustrates the exponent calculation:
 *
 *   slots = 2^(depth * channel_bits)
 *
 * The guard against SIZE_MAX keeps the allocator from wrapping on 32-bit
 * hosts where the histogram would grow beyond addressable memory.
 */
size_t
histogram_dense_size(unsigned int depth,
                     struct histogram_control const *control)
{
    size_t size;
    unsigned int exponent;
    unsigned int i;

    size = 1U;
    exponent = depth * control->channel_bits;
    for (i = 0U; i < exponent; ++i) {
        if (size > SIZE_MAX / 2U) {
            size = SIZE_MAX;
            break;
        }
        size <<= 1U;
    }

    return size;
}

/*
 * Configure histogram binning for the requested LUT policy.  Median-cut
 * inherits the original pnmcolormap heuristics: 5bit keeps wider buckets while
 * 6bit retains finer resolution for shallow depths.
 */
struct histogram_control
histogram_control_make_for_policy(unsigned int depth, int lut_policy)
{
    struct histogram_control control;

    control.reversible_rounding = 0;
    control.channel_shift = 2U;
    if (depth > 3U) {
        control.channel_shift = 3U;
    }
    if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        control.channel_shift = 3U;
    } else if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        control.channel_shift = 2U;
        if (depth > 3U) {
            control.channel_shift = 3U;
        }
    } else if (lut_policy == SIXEL_LUT_POLICY_NONE) {
        control.channel_shift = 0U;
    } else if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        control.channel_shift = 2U;
    }
    control.channel_bits = 8U - control.channel_shift;
    control.channel_mask = (1U << control.channel_bits) - 1U;

    return control;
}

/*
 * Reconstruct an 8-bit channel value from its quantized representation.  The
 * behaviour mirrors Netpbm's midpoint rounding so that palette synthesis and
 * lookup tables agree on the representative colour for each bucket.
 */
static unsigned int
histogram_reconstruct(unsigned int quantized,
                      struct histogram_control const *control)
{
    unsigned int value;

    value = quantized << control->channel_shift;
    if (quantized == control->channel_mask) {
        value = 255U;
    } else {
        if (control->channel_shift > 0U) {
            value |= (1U << (control->channel_shift - 1U));
        }
    }
    if (value > 255U) {
        value = 255U;
    }

    return value;
}

static unsigned int
histogram_quantize(unsigned int sample8,
                   struct histogram_control const *control)
{
    unsigned int quantized;
    unsigned int shift;
    unsigned int mask;
    unsigned int rounding;

    shift = control->channel_shift;
    mask = control->channel_mask;
    if (shift == 0U) {
        quantized = sample8;
    } else {
        if (control->reversible_rounding) {
            rounding = 1U << shift;
        } else {
            rounding = 1U << (shift - 1U);
        }
        quantized = (sample8 + rounding) >> shift;
        if (quantized > mask) {
            quantized = mask;
        }
    }

    return quantized;
}

/*
 * Pack a pixel into a dense histogram index.  Colour channels are processed in
 * reverse order so the least significant bits store the first component.
 */
unsigned int
histogram_pack_color(unsigned char const *data,
                     unsigned int depth,
                     struct histogram_control const *control)
{
    uint32_t packed;
    unsigned int n;
    unsigned int sample8;
    unsigned int bits;

    packed = 0U;
    bits = control->channel_bits;
    if (control->channel_shift == 0U) {
        for (n = 0U; n < depth; ++n) {
            packed |= (uint32_t)data[depth - 1U - n] << (n * bits);
        }
        return packed;
    }

    for (n = 0U; n < depth; ++n) {
        sample8 = (unsigned int)data[depth - 1U - n];
        packed |= histogram_quantize(sample8, control) << (n * bits);
    }

    return packed;
}

/*
 * Construct the histogram consumed by the median-cut splitter.  The control
 * flow is intentionally verbose:
 *
 *   1. Choose sampling density based on quality mode.
 *   2. Quantize each pixel into the dense histogram space.
 *   3. Build a sparse reference map and reconstruct representative tuples.
 */
SIXELSTATUS
sixel_lut_build_histogram(unsigned char const *data,
                          unsigned int length,
                          unsigned long depth,
                          int quality_mode,
                          int use_reversible,
                          int policy,
                          tupletable2 *colorfreqtable,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    typedef uint32_t unit_t;
    unit_t *histogram;
    unit_t *refmap;
    unit_t *ref;
    unsigned int bucket_index;
    unsigned int step;
    unsigned int max_sample;
    size_t hist_size;
    unit_t bucket_value;
    unsigned int component;
    unsigned int reconstructed;
    struct histogram_control control;
    unsigned int depth_u;
    unsigned char reversible_pixel[4];
    unsigned int i;
    unsigned int n;
    unsigned int plane;

    status = SIXEL_FALSE;
    histogram = NULL;
    refmap = NULL;
    if (colorfreqtable == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    colorfreqtable->size = 0U;
    colorfreqtable->table = NULL;

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        max_sample = 18383U;
        break;
    case SIXEL_QUALITY_HIGH:
        max_sample = 1118383U;
        break;
    case SIXEL_QUALITY_FULL:
    default:
        max_sample = 4003079U;
        break;
    }

    if (depth == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    step = (unsigned int)(length / depth / max_sample * depth);
    if (step == 0U) {
        step = (unsigned int)depth;
    }

    sixel_debugf("making histogram...");

    depth_u = (unsigned int)depth;
    control = histogram_control_make_for_policy(depth_u, policy);
    if (use_reversible) {
        control.reversible_rounding = 1;
    }
    hist_size = histogram_dense_size(depth_u, &control);
    histogram = (unit_t *)sixel_allocator_calloc(allocator,
                                                 hist_size,
                                                 sizeof(unit_t));
    if (histogram == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for histogram.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    ref = refmap = (unit_t *)sixel_allocator_malloc(allocator,
                                                    hist_size
                                                    * sizeof(unit_t));
    if (refmap == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for lookup table.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (i = 0U; i < length; i += step) {
        if (use_reversible) {
            for (plane = 0U; plane < depth_u; ++plane) {
                reversible_pixel[plane]
                    = sixel_palette_reversible_value(data[i + plane]);
            }
            bucket_index = histogram_pack_color(reversible_pixel,
                                                depth_u,
                                                &control);
        } else {
            bucket_index = histogram_pack_color(data + i,
                                                depth_u,
                                                &control);
        }
        if (histogram[bucket_index] == 0U) {
            *ref++ = bucket_index;
        }
        if (histogram[bucket_index] < UINT32_MAX) {
            histogram[bucket_index]++;
        }
    }

    colorfreqtable->size = (unsigned int)(ref - refmap);
    status = alloctupletable(&colorfreqtable->table,
                             depth,
                             (unsigned int)(ref - refmap),
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (i = 0U; i < colorfreqtable->size; ++i) {
        bucket_value = refmap[i];
        if (histogram[bucket_value] > 0U) {
            colorfreqtable->table[i]->value = histogram[bucket_value];
            for (n = 0U; n < depth; ++n) {
                component = (unsigned int)
                    ((bucket_value >> (n * control.channel_bits))
                     & control.channel_mask);
                reconstructed = histogram_reconstruct(component,
                                                      &control);
                if (use_reversible) {
                    reconstructed =
                        (unsigned int)sixel_palette_reversible_value(
                            reconstructed);
                }
                colorfreqtable->table[i]->tuple[depth - 1U - n]
                    = (sample)reconstructed;
            }
        }
    }

    sixel_debugf("%u colors found", colorfreqtable->size);
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, refmap);
    sixel_allocator_free(allocator, histogram);

    return status;
}

typedef struct box *boxVector;
struct box {
    unsigned int ind;
    unsigned int colors;
    unsigned int sum;
};

static unsigned int compareplanePlane;
static tupletable2 const *force_palette_source;

/*
 * qsort callback used to order tuples by the component selected for the
 * current split.  The helper mirrors the original Netpbm implementation but
 * keeps the state in a local static so the algorithm can remain thread-safe at
 * the call boundary.
 */
static int
compareplane(const void *arg1, const void *arg2)
{
    int lhs;
    int rhs;
    typedef const struct tupleint *const *const sortarg;
    sortarg comparandPP = (sortarg)arg1;
    sortarg comparatorPP = (sortarg)arg2;
    lhs = (int)(*comparandPP)->tuple[compareplanePlane];
    rhs = (int)(*comparatorPP)->tuple[compareplanePlane];

    return lhs - rhs;
}

static int
sumcompare(const void *b1, const void *b2)
{
    return (int)((boxVector)b2)->sum - (int)((boxVector)b1)->sum;
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
    colormap.size = 0U;
    status = alloctupletable(&colormap.table, depth, newcolors, allocator);
    if (SIXEL_FAILED(status)) {
        return colormap;
    }
    if (colormap.table != NULL) {
        for (i = 0U; i < newcolors; ++i) {
            unsigned int plane;

            for (plane = 0U; plane < depth; ++plane) {
                colormap.table[i]->tuple[plane] = 0;
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

    return bv;
}

static void
findBoxBoundaries(tupletable2 const colorfreqtable,
                  unsigned int depth,
                  unsigned int boxStart,
                  unsigned int boxSize,
                  sample minval[],
                  sample maxval[])
{
    unsigned int plane;
    unsigned int i;

    for (plane = 0U; plane < depth; ++plane) {
        minval[plane] = colorfreqtable.table[boxStart]->tuple[plane];
        maxval[plane] = minval[plane];
    }

    for (i = 1U; i < boxSize; ++i) {
        for (plane = 0U; plane < depth; ++plane) {
            sample v;

            v = colorfreqtable.table[boxStart + i]->tuple[plane];
            if (v < minval[plane]) {
                minval[plane] = v;
            }
            if (v > maxval[plane]) {
                maxval[plane] = v;
            }
        }
    }
}

static unsigned int
largestByNorm(sample minval[], sample maxval[], unsigned int depth)
{
    unsigned int largestDimension;
    unsigned int plane;
    sample largestSpreadSoFar;

    largestSpreadSoFar = 0;
    largestDimension = 0U;
    for (plane = 0U; plane < depth; ++plane) {
        sample spread;

        spread = maxval[plane] - minval[plane];
        if (spread > largestSpreadSoFar) {
            largestDimension = plane;
            largestSpreadSoFar = spread;
        }
    }

    return largestDimension;
}

static unsigned int
largestByLuminosity(sample minval[], sample maxval[], unsigned int depth)
{
    double luminosity[3];
    double spread;
    double largest;
    unsigned int largestDimension;
    unsigned int plane;

    luminosity[0] = 0.2989;
    luminosity[1] = 0.5866;
    luminosity[2] = 0.1145;
    largest = 0.0;
    largestDimension = 0U;
    for (plane = 0U; plane < depth; ++plane) {
        spread = (double)(maxval[plane] - minval[plane]);
        if (plane < 3U) {
            spread *= luminosity[plane];
        }
        if (spread > largest) {
            largest = spread;
            largestDimension = plane;
        }
    }

    return largestDimension;
}

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
    unsigned int i;

    for (plane = 0U; plane < depth; ++plane) {
        minval = colorfreqtable.table[boxStart]->tuple[plane];
        maxval = minval;
        for (i = 1U; i < boxSize; ++i) {
            sample v;

            v = colorfreqtable.table[boxStart + i]->tuple[plane];
            if (v < minval) {
                minval = v;
            }
            if (v > maxval) {
                maxval = v;
            }
        }
        newTuple[plane] = (minval + maxval) / 2;
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

    for (plane = 0U; plane < depth; ++plane) {
        sum = 0;
        for (i = 0U; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->tuple[plane];
        }
        newTuple[plane] = sum / boxSize;
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

    n = 0U;
    for (i = 0U; i < boxSize; ++i) {
        n += (unsigned int)colorfreqtable.table[boxStart + i]->value;
    }

    for (plane = 0U; plane < depth; ++plane) {
        sample sum;

        sum = 0;
        for (i = 0U; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->tuple[plane]
                * (unsigned int)colorfreqtable.table[boxStart + i]->value;
        }
        if (n != 0U) {
            newTuple[plane] = sum / n;
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
        case SIXEL_REP_CENTER_BOX:
            centerBox(bv[bi].ind,
                      bv[bi].colors,
                      colorfreqtable,
                      depth,
                      colormap.table[bi]->tuple);
            break;
        case SIXEL_REP_AVERAGE_COLORS:
            averageColors(bv[bi].ind,
                          bv[bi].colors,
                          colorfreqtable,
                          depth,
                          colormap.table[bi]->tuple);
            break;
        case SIXEL_REP_AVERAGE_PIXELS:
            averagePixels(bv[bi].ind,
                          bv[bi].colors,
                          colorfreqtable,
                          depth,
                          colormap.table[bi]->tuple);
            break;
        default:
#if HAVE_ASSERT
            assert(!"invalid representative selection method");
#endif
            break;
        }
        if (use_reversible) {
            sixel_palette_reversible_tuple(colormap.table[bi]->tuple, depth);
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

    status = alloctupletable(&new_table, depth, reqColors, allocator);
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
            new_table[fill]->tuple[plane]
                = colormapP->table[fill]->tuple[plane];
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
                new_table[fill]->tuple[plane]
                    = colorfreqtable.table[index]->tuple[plane];
            }
            ++fill;
            ++candidate;
        }
    }

    if (fill < reqColors && fill == 0U) {
        new_table[fill]->value = 0U;
        for (plane = 0U; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] = 0U;
        }
        ++fill;
    }

    source = 0U;
    while (fill < reqColors) {
        new_table[fill]->value = new_table[source]->value;
        for (plane = 0U; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] = new_table[source]->tuple[plane];
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
static SIXELSTATUS
sixel_palette_clusters_to_colormap(unsigned long *weights,
                                   unsigned long *sums,
                                   unsigned int depth,
                                   unsigned int cluster_count,
                                   int use_reversible,
                                   tupletable2 *colormapP,
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
            component = (double)sums[(size_t)index * (size_t)depth + plane];
            component /= (double)weight;
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            colormap.table[index]->tuple[plane]
                = (sample)(component + 0.5);
        }
        if (use_reversible) {
            sixel_palette_reversible_tuple(colormap.table[index]->tuple,
                                          depth);
        }
    }
    *colormapP = colormap;
    status = SIXEL_OK;

    return status;
}

static SIXELSTATUS
splitBox(boxVector bv,
         unsigned int *boxesP,
         unsigned int bi,
         tupletable2 const colorfreqtable,
         unsigned int depth,
         int methodForLargest)
{
    SIXELSTATUS status;
    unsigned int boxStart;
    unsigned int boxSize;
    unsigned int sm;
    enum { max_depth = 16 };
    sample minval[max_depth];
    sample maxval[max_depth];
    unsigned int largestDimension;
    unsigned int medianIndex;
    unsigned int lowersum;
    unsigned int i;

    status = SIXEL_FALSE;
    boxStart = bv[bi].ind;
    boxSize = bv[bi].colors;
    sm = bv[bi].sum;
    findBoxBoundaries(colorfreqtable,
                      depth,
                      boxStart,
                      boxSize,
                      minval,
                      maxval);
    switch (methodForLargest) {
    case SIXEL_LARGE_NORM:
        largestDimension = largestByNorm(minval, maxval, depth);
        break;
    case SIXEL_LARGE_LUM:
        largestDimension = largestByLuminosity(minval, maxval, depth);
        break;
    default:
        sixel_helper_set_additional_message(
            "Internal error: invalid value of methodForLargest.");
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    compareplanePlane = largestDimension;
    qsort((char *)&colorfreqtable.table[boxStart],
          boxSize,
          sizeof(colorfreqtable.table[boxStart]),
          compareplane);

    lowersum = colorfreqtable.table[boxStart]->value;
    for (i = 1U; i < boxSize - 1U && lowersum < sm / 2U; ++i) {
        lowersum += colorfreqtable.table[boxStart + i]->value;
    }
    medianIndex = i;

    bv[bi].colors = medianIndex;
    bv[bi].sum = lowersum;
    bv[*boxesP].ind = boxStart + medianIndex;
    bv[*boxesP].colors = boxSize - medianIndex;
    bv[*boxesP].sum = sm - lowersum;
    ++(*boxesP);
    qsort((char *)bv, *boxesP, sizeof(struct box), sumcompare);
    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
mediancut(tupletable2 const colorfreqtable,
          unsigned int depth,
          unsigned int newcolors,
          int methodForLargest,
          int methodForRep,
          int use_reversible,
          int final_merge_mode,
          tupletable2 *colormapP,
          sixel_allocator_t *allocator)
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
    unsigned long *cluster_sums;
    int cluster_total;
    unsigned int plane;
    unsigned int offset;
    unsigned int size;
    unsigned long value;
    struct tupleint *entry;
    SIXELSTATUS merge_status;
    unsigned int iteration_limit;

    status = SIXEL_FALSE;
    bv = NULL;
    cluster_weight = NULL;
    cluster_sums = NULL;
    cluster_total = 0;
    working_colors = newcolors;
    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD
                   || resolved_merge == SIXEL_FINAL_MERGE_HKMEANS);
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
    boxes = 1U;
    multicolorBoxesExist = (colorfreqtable.size > 1U);
    while (boxes < working_colors && multicolorBoxesExist) {
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
                              methodForLargest);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    }

    if (apply_merge && boxes > newcolors) {
        cluster_weight = (unsigned long *)sixel_allocator_malloc(
            allocator,
            (size_t)boxes * sizeof(unsigned long));
        cluster_sums = (unsigned long *)sixel_allocator_malloc(
            allocator,
            (size_t)boxes * (size_t)depth * sizeof(unsigned long));
        if (cluster_weight == NULL || cluster_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (bi = 0U; bi < boxes; ++bi) {
            offset = bv[bi].ind;
            size = bv[bi].colors;
            cluster_weight[bi] = 0UL;
            for (plane = 0U; plane < depth; ++plane) {
                cluster_sums[(size_t)bi * (size_t)depth + plane] = 0UL;
            }
            for (i = 0U; i < size; ++i) {
                entry = colorfreqtable.table[offset + i];
                value = (unsigned long)entry->value;
                cluster_weight[bi] += value;
                for (plane = 0U; plane < depth; ++plane) {
                    cluster_sums[(size_t)bi * (size_t)depth + plane]
                        += (unsigned long)entry->tuple[plane] * value;
                }
            }
        }
        cluster_total = sixel_palette_apply_merge(cluster_weight,
                                                  cluster_sums,
                                                  depth,
                                                  (int)boxes,
                                                  (int)newcolors,
                                                  resolved_merge,
                                                  use_reversible,
                                                  allocator);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > newcolors) {
            cluster_total = (int)newcolors;
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
                                                  iteration_limit);
            }
        }
        merge_status = sixel_palette_clusters_to_colormap(cluster_weight,
                                                          cluster_sums,
                                                          depth,
                                                          (unsigned int)
                                                              cluster_total,
                                                          use_reversible,
                                                          colormapP,
                                                          allocator);
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
                                    allocator);
    }

    status = SIXEL_OK;

end:
    if (bv != NULL) {
        sixel_allocator_free(allocator, bv);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weight != NULL) {
        sixel_allocator_free(allocator, cluster_weight);
    }

    return status;
}

static SIXELSTATUS
sixel_palette_heckbert_colormap(unsigned char const *data,
                                unsigned int length,
                                unsigned int depth,
                                unsigned int reqColors,
                                int methodForLargest,
                                int methodForRep,
                                int qualityMode,
                                int force_palette,
                                int use_reversible,
                                int final_merge_mode,
                                int lut_policy,
                                tupletable2 *colormapP,
                                unsigned int *origcolors,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    tupletable2 colorfreqtable;
    unsigned int i;
    unsigned int n;

    status = SIXEL_FALSE;
    colorfreqtable.size = 0U;
    colorfreqtable.table = NULL;
    if (colormapP == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    colormapP->size = 0U;
    colormapP->table = NULL;
    status = sixel_lut_build_histogram(data,
                                       length,
                                       depth,
                                       qualityMode,
                                       use_reversible,
                                       lut_policy,
                                       &colorfreqtable,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors != NULL) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size <= reqColors) {
        sixel_debugf("Image already has few enough colors (<=%u). Keeping "
                     "same colors.",
                     reqColors);
        colormapP->size = colorfreqtable.size;
        status = alloctupletable(&colormapP->table,
                                 depth,
                                 colorfreqtable.size,
                                 allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        for (i = 0U; i < colorfreqtable.size; ++i) {
            colormapP->table[i]->value = colorfreqtable.table[i]->value;
            for (n = 0U; n < depth; ++n) {
                colormapP->table[i]->tuple[n]
                    = colorfreqtable.table[i]->tuple[n];
            }
            if (use_reversible) {
                sixel_palette_reversible_tuple(
                    colormapP->table[i]->tuple,
                    depth);
            }
        }
    } else {
        sixel_debugf("choosing %u colors...", reqColors);
        status = mediancut(colorfreqtable,
                           depth,
                           reqColors,
                           methodForLargest,
                           methodForRep,
                           use_reversible,
                           final_merge_mode,
                           colormapP,
                           allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_debugf("%u colors are chosen.", colorfreqtable.size);
    }
    if (force_palette) {
        status = force_palette_completion(colormapP,
                                          depth,
                                          reqColors,
                                          colorfreqtable,
                                          allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    if (colorfreqtable.table != NULL) {
        sixel_allocator_free(allocator, colorfreqtable.table);
    }

    return status;
}

SIXELSTATUS
sixel_palette_build_heckbert(sixel_palette_t *palette,
                             unsigned char const *data,
                             unsigned int length,
                             int pixelformat,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_allocator_t *work_allocator;
    tupletable2 colormap;
    unsigned int origcolors;
    int depth_result;
    unsigned int depth;
    size_t payload_size;
    unsigned int index;
    unsigned int plane;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    depth_result = sixel_helper_compute_depth(pixelformat);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_heckbert: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = (unsigned int)depth_result;

    colormap.size = 0U;
    colormap.table = NULL;
    origcolors = 0U;
    status = sixel_palette_heckbert_colormap(data,
                                             length,
                                             depth,
                                             palette->requested_colors,
                                             palette->method_for_largest,
                                             palette->method_for_rep,
                                             palette->quality_mode,
                                             palette->force_palette,
                                             palette->use_reversible,
                                             palette->final_merge_mode,
                                             palette->lut_policy,
                                             &colormap,
                                             &origcolors,
                                             work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_palette_resize(palette,
                                  colormap.size,
                                  (int)depth,
                                  work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    payload_size = (size_t)colormap.size * (size_t)depth;
    if (payload_size > 0U && palette->entries != NULL
        && colormap.table != NULL) {
        for (index = 0U; index < colormap.size; ++index) {
            for (plane = 0U; plane < depth; ++plane) {
                palette->entries[index * depth + plane]
                    = (unsigned char)colormap.table[index]->tuple[plane];
            }
        }
    }

    palette->original_colors = origcolors;
    status = SIXEL_OK;

end:
    if (colormap.table != NULL) {
        sixel_allocator_free(work_allocator, colormap.table);
    }

    return status;
}

#if HAVE_TESTS
static int
palette_test_luminosity(void)
{
    int nret = EXIT_FAILURE;
    sample minval[1] = { 1 };
    sample maxval[1] = { 2 };
    unsigned int retval;

    retval = largestByLuminosity(minval, maxval, 1);
    if (retval != 0) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}

SIXELAPI int
sixel_palette_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (*palette_testcase)(void);

    static palette_testcase const testcases[] = {
        palette_test_luminosity,
    };

    for (i = 0U; i < sizeof(testcases) / sizeof(palette_testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
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
