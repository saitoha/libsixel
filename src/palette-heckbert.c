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
#include "logger.h"
#include "lookup-common.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-heckbert.h"
#include "palette.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"

/*
 * K-means testcases live in palette-kmeans.c; declare them here so the
 * unified test driver can exercise both engines without importing the
 * entire implementation header.
 */
int palette_test_kmeans_float32_two_colors(void);
int palette_test_kmeans_float32_merge_scaling(void);

/*
 * Tuple table primitives live exclusively inside the Heckbert implementation.
 * Exposing them here avoids leaking histogram internals into palette.c.
 */
typedef unsigned long sample;
typedef sample *tuple;

enum {
    sixel_palette_heckbert_max_channels = 4
};

struct tupleint {
    unsigned int value;
    sample tuple[1];
};

typedef struct tupleint **tupletable;

typedef struct {
    unsigned int size;
    tupletable table;
} tupletable2;

/*
 * Median-cut histogram configuration is private to this module.  Keeping the
 * structure here prevents unrelated lookup code from depending on Heckbert
 * internals.
 */
struct histogram_control {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
    int reversible_rounding;
};

static int
sixel_palette_heckbert_log_start(sixel_logger_t *logger,
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
sixel_palette_heckbert_log_finish(sixel_logger_t *logger,
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

/*
 * Normalize a float32 channel according to the current pixelformat and convert
 * it into an 8-bit bucket sample for the histogram quantizer.  This keeps
 * signed OKLab components from collapsing to zero when float32 inputs drive
 * the palette solver.
 */
static unsigned char
sixel_palette_heckbert_float32_to_u8(float value,
                                     int pixelformat,
                                     int channel)
{
    return sixel_pixelformat_float_channel_to_byte(pixelformat,
                                                   channel,
                                                   value);
}

/*
 * Convert tuple samples into normalized float entries so the palette builder
 * can preserve float32 precision when RGBFLOAT32 inputs reach the median-cut
 * implementation.
 */
static float
sixel_palette_heckbert_sample_to_float(sample value)
{
    sample clamped;
    float normalized;

    clamped = value;
    if (clamped > 255UL) {
        clamped = 255UL;
    }
    normalized = (float)clamped / 255.0f;

    return normalized;
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
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
            components[plane]
                = (double)sixel_pixelformat_byte_to_float(
                      pixelformat,
                      (int)plane,
                      (unsigned char)tuple_values[plane]);
            continue;
        }
        components[plane] = (double)tuple_values[plane];
    }
    sixel_palette_snap_triple(components,
                              1,
                              pixelformat,
                              SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
    for (plane = 0U; plane < depth && plane < 3U; ++plane) {
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
            tuple_values[plane]
                = (sample)sixel_pixelformat_float_channel_to_byte(
                      pixelformat,
                      (int)plane,
                      (float)components[plane]);
            continue;
        }
        tuple_values[plane] = (sample)components[plane];
    }
}

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
static size_t
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
static struct histogram_control
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
static unsigned int
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
static SIXELSTATUS
sixel_lut_build_histogram(unsigned char const *data,
                          unsigned int length,
                          unsigned int depth,
                          unsigned int pixel_stride,
                          int pixelformat,
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
    size_t hist_size;
    unit_t bucket_value;
    unsigned int component;
    unsigned int reconstructed;
    struct histogram_control control;
    unsigned int depth_u;
    unsigned char reversible_pixel[sixel_palette_heckbert_max_channels];
    unsigned char quantized_pixel[sixel_palette_heckbert_max_channels];
    unsigned int i;
    unsigned int n;
    unsigned int plane;
    unsigned int channel_stride;
    int input_is_float32;

    status = SIXEL_FALSE;
    histogram = NULL;
    refmap = NULL;
    if (colorfreqtable == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    colorfreqtable->size = 0U;
    colorfreqtable->table = NULL;

    if (depth == 0U || data == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixel_stride == 0U || pixel_stride % depth != 0U) {
        sixel_helper_set_additional_message(
            "sixel_lut_build_histogram: invalid pixel stride.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth > sixel_palette_heckbert_max_channels) {
        sixel_helper_set_additional_message(
            "sixel_lut_build_histogram: unsupported channel count.");
        return SIXEL_BAD_ARGUMENT;
    }

    channel_stride = pixel_stride / depth;
    input_is_float32 = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat);
    if (input_is_float32) {
        if (channel_stride != sizeof(float)) {
            sixel_helper_set_additional_message(
                "sixel_lut_build_histogram: unexpected float stride.");
            return SIXEL_BAD_ARGUMENT;
        }
    } else if (channel_stride != 1U) {
        sixel_helper_set_additional_message(
            "sixel_lut_build_histogram: only 8bit or "
            "RGBFLOAT32 inputs are supported.");
        return SIXEL_BAD_ARGUMENT;
    }

    (void)quality_mode;

    /*
     * The encoder now owns sampling, so the histogram walks every supplied
     * pixel.  The stride stays aligned to the caller's layout to avoid losing
     * information after the upstream down-sampler trimmed the dataset.
     */
    step = pixel_stride;

    sixel_debugf("making histogram...");

    depth_u = depth;
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

    for (i = 0U; i + pixel_stride <= length; i += step) {
        unsigned char const *bucket_input;

        bucket_input = NULL;
        if (input_is_float32) {
            float const *float_pixel;

            float_pixel = (float const *)(void const *)(data + i);
            for (plane = 0U; plane < depth_u; ++plane) {
                quantized_pixel[plane]
                    = sixel_palette_heckbert_float32_to_u8(
                        float_pixel[plane],
                        pixelformat,
                        (int)plane);
            }
            bucket_input = quantized_pixel;
        } else {
            bucket_input = data + i;
        }
        if (use_reversible) {
            for (plane = 0U; plane < depth_u; ++plane) {
                reversible_pixel[plane]
                    = sixel_palette_reversible_value(bucket_input[plane]);
            }
            bucket_input = reversible_pixel;
        }
        bucket_index = histogram_pack_color(bucket_input,
                                            depth_u,
                                            &control);
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
static double compareplanePcaAxis[3];
static unsigned int compareplanePcaDimensions;

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

/*
 * qsort callback used when the split axis is determined by PCA.  The projection
 * is evaluated on demand to avoid storing an additional array alongside the
 * histogram entries.
 */
static int
compareplanePca(const void *arg1, const void *arg2)
{
    double lhs;
    double rhs;
    unsigned int plane;
    typedef const struct tupleint *const *const sortarg;
    sortarg comparandPP = (sortarg)arg1;
    sortarg comparatorPP = (sortarg)arg2;

    lhs = 0.0;
    rhs = 0.0;
    for (plane = 0U; plane < compareplanePcaDimensions; ++plane) {
        lhs += (double)(*comparandPP)->tuple[plane]
               * compareplanePcaAxis[plane];
        rhs += (double)(*comparatorPP)->tuple[plane]
               * compareplanePcaAxis[plane];
    }

    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }

    return 0;
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
    double vec[3];
    double next[3];
    double norm;
    double variance_total;
    double lambda;
    struct tupleint *entry;
    double weight;
    double diff;

    dims = depth < 3U ? depth : 3U;
    weight_sum = 0.0;
    variance_total = 0.0;
    lambda = 0.0;
    for (plane = 0U; plane < 3U; ++plane) {
        mean[plane] = 0.0;
        vec[plane] = (plane < dims) ? 1.0 : 0.0;
        axis[plane] = 0.0;
        for (other = 0U; other < 3U; ++other) {
            cov[plane][other] = 0.0;
        }
    }
    if (dims == 0U || boxSize < 2U) {
        return 0;
    }

    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        weight = (double)entry->value;
        if (weight == 0.0) {
            weight = 1.0;
        }
        weight_sum += weight;
        for (plane = 0U; plane < dims; ++plane) {
            mean[plane] += weight * (double)entry->tuple[plane];
        }
    }
    if (weight_sum <= 0.0) {
        return 0;
    }
    for (plane = 0U; plane < dims; ++plane) {
        mean[plane] /= weight_sum;
    }

    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        weight = (double)entry->value;
        if (weight == 0.0) {
            weight = 1.0;
        }
        for (plane = 0U; plane < dims; ++plane) {
            diff = (double)entry->tuple[plane] - mean[plane];
            cov[plane][plane] += weight * diff * diff;
            for (other = plane + 1U; other < dims; ++other) {
                cov[plane][other]
                    += weight * diff
                       * ((double)entry->tuple[other] - mean[other]);
            }
        }
    }
    for (plane = 0U; plane < dims; ++plane) {
        for (other = plane; other < dims; ++other) {
            cov[plane][other] /= weight_sum;
            cov[other][plane] = cov[plane][other];
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
        for (plane = 0U; plane < 3U; ++plane) {
            vec[plane] = (plane < dims) ? next[plane] / norm : 0.0;
        }
    }
    lambda = 0.0;
    for (plane = 0U; plane < dims; ++plane) {
        double projected = 0.0;
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
            sixel_palette_reversible_tuple(colormap.table[bi]->tuple,
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
/*
 * Reassign histogram entries to the merged palette so the final centroids stay
 * aligned with their nearest tuples.  The loop iterates through assignment,
 * accumulation, and centroid update until convergence or the iteration limit.
 */
static void
sixel_final_merge_lloyd_histogram(tupletable2 const colorfreqtable,
                                  unsigned int depth,
                                  unsigned int cluster_count,
                                  unsigned long *cluster_weight,
                                  double *cluster_sums,
                                  unsigned int iterations)
{
    double *centers;
    double distance;
    double best_distance;
    double diff;
    double channel;
    unsigned int iteration;
    unsigned int cluster_index;
    unsigned int component;
    unsigned int entry_index;
    unsigned int best_cluster;
    unsigned long weight;
    unsigned long value;
    size_t offset;
    size_t total;
    struct tupleint *entry;

    centers = NULL;
    distance = 0.0;
    best_distance = 0.0;
    diff = 0.0;
    channel = 0.0;
    iteration = 0U;
    cluster_index = 0U;
    component = 0U;
    entry_index = 0U;
    best_cluster = 0U;
    weight = 0UL;
    value = 0UL;
    offset = 0U;
    total = 0U;
    entry = NULL;
    if (iterations == 0U || cluster_count == 0U || depth == 0U
        || cluster_weight == NULL || cluster_sums == NULL
        || colorfreqtable.table == NULL || colorfreqtable.size == 0U) {
        return;
    }
    total = (size_t)cluster_count * (size_t)depth;
    centers = (double *)malloc(total * sizeof(double));
    if (centers == NULL) {
        return;
    }
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        offset = (size_t)cluster_index * (size_t)depth;
        weight = cluster_weight[cluster_index];
        for (component = 0U; component < depth; ++component) {
            if (weight > 0UL) {
                centers[offset + (size_t)component]
                    = cluster_sums[offset + (size_t)component]
                    / (double)weight;
            } else {
                centers[offset + (size_t)component] = 0.0;
            }
        }
    }
    for (iteration = 0U; iteration < iterations; ++iteration) {
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            cluster_weight[cluster_index] = 0UL;
        }
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            offset = (size_t)cluster_index * (size_t)depth;
            for (component = 0U; component < depth; ++component) {
            cluster_sums[offset + (size_t)component] = 0.0;
            }
        }
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
            best_distance = 0.0;
            offset = 0U;
            for (component = 0U; component < depth; ++component) {
                diff = (double)entry->tuple[component]
                    - centers[offset + (size_t)component];
                best_distance += diff * diff;
            }
            for (cluster_index = 1U; cluster_index < cluster_count;
                    ++cluster_index) {
                distance = 0.0;
                offset = (size_t)cluster_index * (size_t)depth;
                for (component = 0U; component < depth; ++component) {
                    diff = (double)entry->tuple[component]
                        - centers[offset + (size_t)component];
                    distance += diff * diff;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_cluster = cluster_index;
                }
            }
            offset = (size_t)best_cluster * (size_t)depth;
            cluster_weight[best_cluster] += value;
            for (component = 0U; component < depth; ++component) {
                cluster_sums[offset + (size_t)component]
                    += (double)entry->tuple[component] * (double)value;
            }
        }
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            weight = cluster_weight[cluster_index];
            if (weight == 0UL) {
                continue;
            }
            offset = (size_t)cluster_index * (size_t)depth;
            for (component = 0U; component < depth; ++component) {
                centers[offset + (size_t)component]
                    = cluster_sums[offset + (size_t)component]
                    / (double)weight;
            }
        }
    }
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        weight = cluster_weight[cluster_index];
        offset = (size_t)cluster_index * (size_t)depth;
        if (weight == 0UL) {
            for (component = 0U; component < depth; ++component) {
                channel = centers[offset + (size_t)component];
                if (channel < 0.0) {
                    channel = 0.0;
                }
                if (channel > 255.0) {
                    channel = 255.0;
                }
                cluster_sums[offset + (size_t)component] = channel;
            }
            cluster_weight[cluster_index] = 1UL;
        }
    }
    free(centers);
}

static SIXELSTATUS
sixel_palette_clusters_to_colormap(unsigned long *weights,
                                   double *sums,
                                   unsigned int depth,
                                   unsigned int cluster_count,
                                   int use_reversible,
                                   int pixelformat,
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
            component = sums[(size_t)index * (size_t)depth + plane];
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
                                          depth,
                                          pixelformat);
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
    unsigned int dimensions;
    unsigned long total_weight;
    unsigned long weight_value;
    unsigned int weight_clamped;
    double pca_axis[3];
    double pca_ratio;
    int use_pca;
    int effective_method;

    status = SIXEL_FALSE;
    boxStart = bv[bi].ind;
    boxSize = bv[bi].colors;
    sm = bv[bi].sum;
    dimensions = (depth < 3U) ? depth : 3U;
    total_weight = 0UL;
    pca_ratio = 0.0;
    use_pca = (methodForLargest == SIXEL_LARGE_PCA);
    effective_method = methodForLargest;
    findBoxBoundaries(colorfreqtable,
                      depth,
                      boxStart,
                      boxSize,
                      minval,
                      maxval);
    if (use_pca != 0) {
        if (!computePcaAxis(colorfreqtable,
                            dimensions,
                            boxStart,
                            boxSize,
                            pca_axis,
                            &pca_ratio)) {
            sixel_debugf("PCA fallback to range axis on box %u", bi);
            use_pca = 0;
            effective_method = SIXEL_LARGE_NORM;
        } else {
            sixel_debugf("box %u: PCA split (PC1 ratio %.3f)",
                         bi,
                         pca_ratio);
        }
    }
    if (use_pca == 0) {
        switch (effective_method) {
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
    } else {
        compareplanePcaDimensions = dimensions;
        for (i = 0U; i < 3U; ++i) {
            compareplanePcaAxis[i] = pca_axis[i];
        }
        qsort((char *)&colorfreqtable.table[boxStart],
              boxSize,
              sizeof(colorfreqtable.table[boxStart]),
              compareplanePca);
    }

    for (i = 0U; i < boxSize; ++i) {
        weight_value = colorfreqtable.table[boxStart + i]->value;
        if (weight_value == 0UL) {
            weight_value = 1UL;
        }
        total_weight += weight_value;
    }
    if (total_weight == 0UL) {
        sixel_helper_set_additional_message(
            "Internal error: empty histogram in splitBox.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    sm = (total_weight > (unsigned long)UINT_MAX)
             ? UINT_MAX
             : (unsigned int)total_weight;

    weight_value = colorfreqtable.table[boxStart]->value;
    if (weight_value == 0UL) {
        weight_value = 1UL;
    }
    weight_clamped = (weight_value > (unsigned long)UINT_MAX)
                         ? UINT_MAX
                         : (unsigned int)weight_value;
    lowersum = weight_clamped;
    for (i = 1U; i < boxSize - 1U && lowersum < sm / 2U; ++i) {
        weight_value = colorfreqtable.table[boxStart + i]->value;
        if (weight_value == 0UL) {
            weight_value = 1UL;
        }
        weight_clamped = (weight_value > (unsigned long)UINT_MAX)
                             ? UINT_MAX
                             : (unsigned int)weight_value;
        lowersum += weight_clamped;
    }
    medianIndex = i;
    if (medianIndex == 0U) {
        medianIndex = 1U;
    }
    if (medianIndex >= boxSize) {
        medianIndex = boxSize - 1U;
    }

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
          int pixelformat,
          tupletable2 *colormapP,
          sixel_allocator_t *allocator,
          sixel_logger_t *logger,
          int *job_seq,
          char const *engine_name,
          double *iterate_ms,
          unsigned int *iterate_count,
          double *merge_ms,
          unsigned int *merge_iterate_count,
          int *merge_mode)
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
    int cluster_total;
    unsigned int plane;
    unsigned int offset;
    unsigned int size;
    unsigned long value;
    struct tupleint *entry;
    SIXELSTATUS merge_status;
    unsigned int iteration_limit;
    unsigned int boxes_before;
    unsigned int boxes_after;
    double iteration_wall_start;
    double iteration_wall_stop;
    int job_iteration;
    int job_merge;
    double merge_wall_start;
    double merge_wall_stop;
    char log_detail[128];

    status = SIXEL_FALSE;
    bv = NULL;
    cluster_weight = NULL;
    cluster_sums = NULL;
    cluster_total = 0;
    working_colors = newcolors;
    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    iteration_limit = 0U;
    boxes_before = 0U;
    boxes_after = 0U;
    iteration_wall_start = 0.0;
    iteration_wall_stop = 0.0;
    job_iteration = -1;
    job_merge = -1;
    merge_wall_start = 0.0;
    merge_wall_stop = 0.0;
    log_detail[0] = '\0';
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
        iteration_wall_start = sixel_timer_now();
        boxes_before = boxes;
        job_iteration = sixel_palette_heckbert_log_start(logger,
                                                         job_seq,
                                                         engine_name,
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
                              methodForLargest);
            if (SIXEL_FAILED(status)) {
                iteration_wall_stop = sixel_timer_now();
                if (iterate_ms != NULL) {
                    *iterate_ms += (iteration_wall_stop - iteration_wall_start)
                                   * 1000.0;
                }
                if (iterate_count != NULL) {
                    *iterate_count += 1U;
                }
                (void)snprintf(log_detail,
                               sizeof(log_detail),
                               "boxes=%u->%u split=failed",
                               boxes_before,
                               boxes);
                sixel_palette_heckbert_log_finish(logger,
                                                  job_iteration,
                                                  engine_name,
                                                  "palette/iterate",
                                                  "iterate",
                                                  log_detail);
                goto end;
            }
        }
        iteration_wall_stop = sixel_timer_now();
        boxes_after = boxes;
        if (iterate_ms != NULL) {
            *iterate_ms += (iteration_wall_stop - iteration_wall_start)
                           * 1000.0;
        }
        if (iterate_count != NULL) {
            *iterate_count += 1U;
        }
        (void)snprintf(log_detail,
                       sizeof(log_detail),
                       "boxes=%u->%u multicolor=%d",
                       boxes_before,
                       boxes_after,
                       multicolorBoxesExist);
        sixel_palette_heckbert_log_finish(logger,
                                          job_iteration,
                                          engine_name,
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
        for (bi = 0U; bi < boxes; ++bi) {
            offset = bv[bi].ind;
            size = bv[bi].colors;
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
                        += (double)entry->tuple[plane] * (double)value;
                }
            }
        }
        merge_wall_start = sixel_timer_now();
        job_merge = sixel_palette_heckbert_log_start(logger,
                                                     job_seq,
                                                     engine_name,
                                                     "palette/merge",
                                                     "merge");
        cluster_total = sixel_palette_apply_merge(cluster_weight,
                                                  cluster_sums,
                                                  depth,
                                                  (int)boxes,
                                                  (int)newcolors,
                                                  resolved_merge,
                                                  use_reversible,
                                                  pixelformat,
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
                                                          pixelformat,
                                                          colormapP,
                                                          allocator);
        merge_wall_stop = sixel_timer_now();
        if (merge_ms != NULL) {
            *merge_ms += (merge_wall_stop - merge_wall_start) * 1000.0;
        }
        if (merge_iterate_count != NULL && iteration_limit > 0U) {
            *merge_iterate_count += iteration_limit;
        }
        if (merge_mode != NULL) {
            *merge_mode = resolved_merge;
        }
        (void)snprintf(log_detail,
                       sizeof(log_detail),
                       "boxes=%u->%d merge=%d refine=%u",
                       boxes,
                       cluster_total,
                       resolved_merge,
                       iteration_limit);
        sixel_palette_heckbert_log_finish(logger,
                                          job_merge,
                                          engine_name,
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
sixel_palette_heckbert_colormap(sixel_palette_t *palette,
                                unsigned char const *data,
                                unsigned int length,
                                unsigned int depth,
                                int use_reversible,
                                tupletable2 *colormapP,
                                unsigned int *origcolors,
                                unsigned int pixel_stride,
                                int pixelformat,
                                sixel_allocator_t *allocator,
                                sixel_logger_t *logger,
                                int *job_seq,
                                char const *engine_name,
                                double *iterate_ms,
                                unsigned int *iterate_count,
                                double *merge_ms,
                                unsigned int *merge_iterate_count,
                                int *merge_mode)
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
                                       pixel_stride,
                                       pixelformat,
                                       palette->quality_mode,
                                       use_reversible,
                                       palette->lut_policy,
                                       &colorfreqtable,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors != NULL) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size <= palette->requested_colors) {
        sixel_debugf("Image already has few enough colors (<=%u). Keeping "
                     "same colors.",
                     palette->requested_colors);
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
                    depth,
                    pixelformat);
            }
        }
    } else {
        sixel_debugf("choosing %u colors...", palette->requested_colors);
        status = mediancut(colorfreqtable,
                           depth,
                           palette->requested_colors,
                           palette->method_for_largest,
                           palette->method_for_rep,
                           use_reversible,
                           palette->final_merge_mode,
                           pixelformat,
                           colormapP,
                           allocator,
                           logger,
                           job_seq,
                           engine_name,
                           iterate_ms,
                           iterate_count,
                           merge_ms,
                           merge_iterate_count,
                           merge_mode);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_debugf("%u colors are chosen.", colorfreqtable.size);
    }
    if (palette->force_palette) {
        status = force_palette_completion(colormapP,
                                          depth,
                                          palette->requested_colors,
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
                             sixel_allocator_t *allocator,
                             sixel_logger_t *logger,
                             int *job_seq,
                             char const *engine_name,
                             sixel_palette_telemetry_t *telemetry)
{
    SIXELSTATUS status;
    SIXELSTATUS drop_status;
    sixel_allocator_t *work_allocator;
    tupletable2 colormap;
    unsigned int origcolors;
    int depth_result;
    unsigned int depth;
    unsigned int pixel_stride;
    unsigned int bytes_per_channel;
    size_t payload_size;
    unsigned int index;
    unsigned int plane;
    int input_is_float32;
    float *float_entries;
    int float_stride;
    int reversible_for_quantizer;
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
        work_allocator = palette->allocator;
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

    depth_result = sixel_helper_compute_depth(pixelformat);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_heckbert: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    pixel_stride = (unsigned int)depth_result;
    bytes_per_channel = 1U;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        bytes_per_channel = (unsigned int)sizeof(float);
    }
    if (pixel_stride == 0U || bytes_per_channel == 0U
        || pixel_stride % bytes_per_channel != 0U) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_heckbert: unsupported pixel stride.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = pixel_stride / bytes_per_channel;
    if (depth == 0U
        || depth > (unsigned int)sixel_palette_heckbert_max_channels) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_heckbert: invalid channel count.");
        return SIXEL_BAD_ARGUMENT;
    }

    colormap.size = 0U;
    colormap.table = NULL;
    origcolors = 0U;
    input_is_float32 = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat);
    float_entries = NULL;
    float_stride = 0;
    reversible_for_quantizer = palette->use_reversible;
    status = sixel_palette_heckbert_colormap(palette,
                                             data,
                                             length,
                                             depth,
                                             reversible_for_quantizer,
                                             &colormap,
                                             &origcolors,
                                             pixel_stride,
                                             pixelformat,
                                             work_allocator,
                                             logger,
                                             job_seq,
                                             engine_name,
                                             &iterate_ms,
                                             &iterate_count,
                                             &merge_ms,
                                             &merge_iterate_count,
                                             &merge_mode);
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
        size_t float_entries_size;

        float_stride = (int)((size_t)depth * (size_t)sizeof(float));
        float_entries_size = (size_t)colormap.size
                              * (size_t)depth
                              * (size_t)sizeof(float);
        if (float_stride <= 0) {
            sixel_helper_set_additional_message(
                "sixel_palette_build_heckbert: invalid float stride.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (float_entries_size > 0U) {
            float_entries = (float *)sixel_allocator_malloc(
                work_allocator,
                float_entries_size);
            if (float_entries == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_palette_build_heckbert: float palette alloc"
                    " failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
    }

    payload_size = (size_t)colormap.size * (size_t)depth;
    if (payload_size > 0U && palette->entries != NULL
        && colormap.table != NULL) {
        for (index = 0U; index < colormap.size; ++index) {
            for (plane = 0U; plane < depth; ++plane) {
                palette->entries[index * depth + plane]
                    = (unsigned char)colormap.table[index]->tuple[plane];
                if (float_entries != NULL) {
                    float_entries[index * depth + plane]
                        = sixel_palette_heckbert_sample_to_float(
                            colormap.table[index]->tuple[plane]);
                }
            }
        }
    }

    palette->original_colors = origcolors;
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
                   palette->entry_count,
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

SIXELSTATUS
sixel_palette_build_heckbert_float32(sixel_palette_t *palette,
                                     float const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     sixel_logger_t *logger,
                                     int *job_seq,
                                     char const *engine_name,
                                     sixel_palette_telemetry_t *telemetry)
{
    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_heckbert_float32: "
            "requires RGBFLOAT32 input.");
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_palette_build_heckbert(palette,
                                        (unsigned char const *)data,
                                        length,
                                        pixelformat,
                                        allocator,
                                        logger,
                                        job_seq,
                                        engine_name,
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
