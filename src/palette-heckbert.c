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
    sixel_palette_heckbert_max_channels = 4,
    sixel_palette_heckbert_axis_bins = 256
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
 * Build an 8-bit quantization table so the histogram pass can avoid
 * per-channel shift/rounding branches.
 */
static void
histogram_build_quantize_lut(struct histogram_control const *control,
                             unsigned int quantized_lut[256])
{
    unsigned int sample8;

    for (sample8 = 0U; sample8 < 256U; ++sample8) {
        quantized_lut[sample8] = histogram_quantize(sample8, control);
    }
}

/*
 * Build plane-indexed packed values for depth 3/4 RGB(A) fast-path packing.
 */
static void
histogram_build_fast_pack_lut(unsigned int depth,
                              unsigned int bits,
                              unsigned int const quantized_lut[256],
                              uint32_t packed_lut[4][256])
{
    unsigned int plane;
    unsigned int sample8;
    unsigned int shift;

    for (plane = 0U; plane < 4U; ++plane) {
        for (sample8 = 0U; sample8 < 256U; ++sample8) {
            packed_lut[plane][sample8] = 0U;
        }
    }
    for (plane = 0U; plane < depth; ++plane) {
        shift = (depth - 1U - plane) * bits;
        for (sample8 = 0U; sample8 < 256U; ++sample8) {
            packed_lut[plane][sample8]
                = (uint32_t)quantized_lut[sample8] << shift;
        }
    }
}

static uint32_t
histogram_pack_color_fast_rgb(unsigned char const *data,
                              uint32_t packed_lut[4][256])
{
    return packed_lut[0][data[0]]
           | packed_lut[1][data[1]]
           | packed_lut[2][data[2]];
}

static uint32_t
histogram_pack_color_fast_rgba(unsigned char const *data,
                               uint32_t packed_lut[4][256])
{
    return packed_lut[0][data[0]]
           | packed_lut[1][data[1]]
           | packed_lut[2][data[2]]
           | packed_lut[3][data[3]];
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
 * Generic LUT-based packer used when the 3/4-channel fast paths are not
 * applicable.
 */
static unsigned int
histogram_pack_color_from_lut(unsigned char const *data,
                              unsigned int depth,
                              unsigned int bits,
                              unsigned int const quantized_lut[256])
{
    unsigned int n;
    uint32_t packed;
    unsigned int sample8;

    packed = 0U;
    for (n = 0U; n < depth; ++n) {
        sample8 = (unsigned int)data[depth - 1U - n];
        packed |= (uint32_t)quantized_lut[sample8] << (n * bits);
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
    unsigned int quantized_lut[256];
    uint32_t packed_lut[4][256];
    unsigned int bits;
    int use_fast_pack;
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
    histogram_build_quantize_lut(&control, quantized_lut);
    bits = control.channel_bits;
    use_fast_pack = 0;
    if (channel_stride == 1U && (depth_u == 3U || depth_u == 4U)) {
        histogram_build_fast_pack_lut(depth_u,
                                      bits,
                                      quantized_lut,
                                      packed_lut);
        use_fast_pack = 1;
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
        if (use_fast_pack && depth_u == 3U) {
            bucket_index = histogram_pack_color_fast_rgb(bucket_input,
                                                         packed_lut);
        } else if (use_fast_pack && depth_u == 4U) {
            bucket_index = histogram_pack_color_fast_rgba(bucket_input,
                                                          packed_lut);
        } else if (control.channel_shift == 0U) {
            bucket_index = histogram_pack_color(bucket_input,
                                                depth_u,
                                                &control);
        } else {
            bucket_index = histogram_pack_color_from_lut(bucket_input,
                                                         depth_u,
                                                         bits,
                                                         quantized_lut);
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

static tupletable2 const *force_palette_source;
static double compareplanePcaAxis[3];
static unsigned int compareplanePcaDimensions;
static unsigned int compareplaneTieDepth;

struct sixel_pca_sort_entry {
    struct tupleint *entry;
    double key;
};

static int
compareplane_lexicographic(struct tupleint const *lhs,
                           struct tupleint const *rhs,
                           unsigned int depth)
{
    unsigned int plane;
    int lhs_value;
    int rhs_value;

    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    for (plane = 0U; plane < depth; ++plane) {
        lhs_value = (int)lhs->tuple[plane];
        rhs_value = (int)rhs->tuple[plane];
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
    if (lhs->sum < rhs->sum) {
        return 1;
    }
    if (lhs->sum > rhs->sum) {
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
    unsigned int pos;
    struct box tmp;

    pos = index;
    if (bv == NULL || count < 2U || index >= count) {
        return;
    }
    while (pos > 0U && sumcompare(&bv[pos - 1U], &bv[pos]) > 0) {
        tmp = bv[pos - 1U];
        bv[pos - 1U] = bv[pos];
        bv[pos] = tmp;
        --pos;
    }
    while (pos + 1U < count && sumcompare(&bv[pos], &bv[pos + 1U]) > 0) {
        tmp = bv[pos];
        bv[pos] = bv[pos + 1U];
        bv[pos + 1U] = tmp;
        ++pos;
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

static unsigned int
sixel_palette_heckbert_entry_weight(struct tupleint const *entry)
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
                               [sixel_palette_heckbert_max_channels]
                               [sixel_palette_heckbert_axis_bins])
{
    unsigned int plane;
    unsigned int i;
    struct tupleint const *entry;
    unsigned int weight;
    unsigned int axis_value;

    for (plane = 0U; plane < depth; ++plane) {
        minval[plane] = colorfreqtable.table[boxStart]->tuple[plane];
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
            sample value;

            value = entry->tuple[plane];
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

/*
 * Reorder tuples by one 8-bit axis using an in-place bucket permutation.
 * This replaces O(n log n) qsort for norm/lum splits with O(n + 256) work.
 */
static SIXELSTATUS
sortBoxByAxisBuckets(tupletable2 const colorfreqtable,
                     unsigned int boxStart,
                     unsigned int boxSize,
                     unsigned int axis,
                     unsigned int const axis_count
                         [sixel_palette_heckbert_max_channels]
                         [sixel_palette_heckbert_axis_bins],
                     sample const minval[],
                     sample const maxval[])
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
    struct tupleint *entry;
    struct tupleint *swapped;

    if (axis >= sixel_palette_heckbert_max_channels) {
        sixel_helper_set_additional_message(
            "Internal error: invalid split axis in sortBoxByAxisBuckets.");
        return SIXEL_BAD_ARGUMENT;
    }
    for (bucket = 0U; bucket < sixel_palette_heckbert_axis_bins; ++bucket) {
        bucket_count[bucket] = 0U;
        bucket_next[bucket] = 0U;
        bucket_end[bucket] = 0U;
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
            entry = colorfreqtable.table[boxStart + index];
            axis_value = (unsigned int)entry->tuple[axis];
            if (axis_value >= sixel_palette_heckbert_axis_bins) {
                axis_value = sixel_palette_heckbert_axis_bins - 1U;
            }
            bucket_count[axis_value] += 1U;
            if (axis_value < min_bucket) {
                min_bucket = axis_value;
            }
            if (axis_value > max_bucket) {
                max_bucket = axis_value;
            }
        }
    }
    if (boxSize == 0U || min_bucket >= max_bucket) {
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
            entry = colorfreqtable.table[index];
            axis_value = (unsigned int)entry->tuple[axis];
            if (axis_value >= sixel_palette_heckbert_axis_bins) {
                axis_value = sixel_palette_heckbert_axis_bins - 1U;
            }
            if (axis_value == bucket) {
                bucket_next[bucket] += 1U;
                continue;
            }
            swapped = colorfreqtable.table[bucket_next[axis_value]];
            colorfreqtable.table[bucket_next[axis_value]] = entry;
            colorfreqtable.table[index] = swapped;
            bucket_next[axis_value] += 1U;
        }
    }

    return SIXEL_OK;
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
    struct tupleint *entry;
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
     * Collect first and second moments in one scan so PCA split setup does
     * not walk the same box twice.
     */
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        weight = (double)entry->value;
        if (weight == 0.0) {
            weight = 1.0;
        }
        weight_sum += weight;
        for (plane = 0U; plane < dims; ++plane) {
            tuple_value[plane] = (double)entry->tuple[plane];
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
    for (plane = 0U; plane < dims; ++plane) {
        for (other = plane; other < dims; ++other) {
            normalized_second = second_moment[plane][other] / weight_sum;
            cov_value = normalized_second - mean[plane] * mean[other];
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
                       double axis[3])
{
    struct sixel_pca_sort_entry *cache;
    unsigned int i;
    unsigned int plane;
    struct tupleint *entry;
    double key;

    cache = NULL;
    i = 0U;
    plane = 0U;
    entry = NULL;
    key = 0.0;
    cache = (struct sixel_pca_sort_entry *)malloc(
        boxSize * sizeof(struct sixel_pca_sort_entry));
    if (cache == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    compareplaneTieDepth = depth;
    for (i = 0U; i < boxSize; ++i) {
        entry = colorfreqtable.table[boxStart + i];
        key = 0.0;
        for (plane = 0U; plane < dimensions; ++plane) {
            key += (double)entry->tuple[plane] * axis[plane];
        }
        cache[i].entry = entry;
        cache[i].key = key;
    }
    qsort((char *)cache,
          boxSize,
          sizeof(struct sixel_pca_sort_entry),
          compareplanePcaCached);
    for (i = 0U; i < boxSize; ++i) {
        colorfreqtable.table[boxStart + i] = cache[i].entry;
    }
    free(cache);

    return SIXEL_OK;
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

    if (boxSize == 0U) {
        for (plane = 0U; plane < depth; ++plane) {
            newTuple[plane] = 0;
        }
        return;
    }

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
    unsigned int *assignment;
    unsigned int previous_cluster;
    int has_assignment_history;
    int assignment_changed;
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
    assignment = NULL;
    previous_cluster = 0U;
    has_assignment_history = 0;
    assignment_changed = 0;
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
    assignment = (unsigned int *)malloc(
        colorfreqtable.size * sizeof(unsigned int));
    if (assignment != NULL) {
        for (entry_index = 0U; entry_index < colorfreqtable.size;
                ++entry_index) {
            assignment[entry_index] = UINT_MAX;
        }
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
        assignment_changed = 0;
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
            previous_cluster = UINT_MAX;
            if (assignment != NULL) {
                previous_cluster = assignment[entry_index];
                if (previous_cluster < cluster_count) {
                    best_cluster = previous_cluster;
                }
            }
            best_distance = 0.0;
            offset = (size_t)best_cluster * (size_t)depth;
            for (component = 0U; component < depth; ++component) {
                diff = (double)entry->tuple[component]
                    - centers[offset + (size_t)component];
                best_distance += diff * diff;
            }
            for (cluster_index = 0U; cluster_index < cluster_count;
                    ++cluster_index) {
                if (cluster_index == best_cluster) {
                    continue;
                }
                distance = 0.0;
                offset = (size_t)cluster_index * (size_t)depth;
                for (component = 0U; component < depth; ++component) {
                    diff = (double)entry->tuple[component]
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
            if (assignment != NULL) {
                assignment[entry_index] = best_cluster;
                if (previous_cluster != best_cluster) {
                    assignment_changed = 1;
                }
            }
            offset = (size_t)best_cluster * (size_t)depth;
            cluster_weight[best_cluster] += value;
            for (component = 0U; component < depth; ++component) {
                cluster_sums[offset + (size_t)component]
                    += (double)entry->tuple[component] * (double)value;
            }
        }
        if (assignment != NULL) {
            if (has_assignment_history && !assignment_changed) {
                break;
            }
            has_assignment_history = 1;
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
    if (assignment != NULL) {
        free(assignment);
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
    unsigned int const axis_count[sixel_palette_heckbert_max_channels]
                                 [sixel_palette_heckbert_axis_bins],
    int methodForLargest,
    unsigned int sum,
    unsigned int dimensions,
    unsigned int *medianIndex,
    unsigned int *lowersum,
    unsigned int *axis_used,
    double *pca_ratio)
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
        status = sortBoxByPcaProjection(colorfreqtable,
                                        boxStart,
                                        boxSize,
                                        depth,
                                        dimensions,
                                        pca_axis);
        if (status == SIXEL_BAD_ALLOCATION) {
            qsort((char *)&colorfreqtable.table[boxStart],
                  boxSize,
                  sizeof(colorfreqtable.table[boxStart]),
                  compareplanePca);
            status = SIXEL_OK;
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (axis_used != NULL) {
            *axis_used = 0U;
        }
    } else {
        if (methodForLargest == SIXEL_LARGE_LUM) {
            axis = largestByLuminosity(minval, maxval, depth);
        } else {
            axis = largestByNorm(minval, maxval, depth);
        }
        status = sortBoxByAxisBuckets(colorfreqtable,
                                      boxStart,
                                      boxSize,
                                      axis,
                                      axis_count,
                                      minval,
                                      maxval);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (axis_used != NULL) {
            *axis_used = axis;
        }
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
         int methodForLargest)
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
    unsigned int axis_count[sixel_palette_heckbert_max_channels]
                           [sixel_palette_heckbert_axis_bins];
    unsigned int (*axis_count_ptr)[sixel_palette_heckbert_axis_bins];
    int split_method;
    int degenerate;

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
    if (methodForLargest != SIXEL_LARGE_PCA) {
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
        if (split_method != SIXEL_LARGE_PCA && axis_count_ptr == NULL) {
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
                                                      &medianIndex,
                                                      &lowersum,
                                                      &axis_used,
                                                      &pca_ratio);
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
    *boxesP = old_boxes + 1U;
    sixel_palette_heckbert_reposition_box(bv, old_boxes, bi);
    sixel_palette_heckbert_reposition_box(bv, *boxesP, old_boxes);
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
                        = (double)entry->tuple[plane] * (double)value;
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

/* pcc may ICE on very long parameter lists; bundle into one request object. */
typedef struct sixel_palette_heckbert_colormap_request {
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    int use_reversible;
    tupletable2 *colormap;
    unsigned int *origcolors;
    unsigned int pixel_stride;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    double *iterate_ms;
    unsigned int *iterate_count;
    double *merge_ms;
    unsigned int *merge_iterate_count;
    int *merge_mode;
} sixel_palette_heckbert_colormap_request_t;

static SIXELSTATUS
sixel_palette_heckbert_colormap(
    sixel_palette_heckbert_colormap_request_t const *request)
{
    SIXELSTATUS status;
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    int use_reversible;
    tupletable2 *colormapP;
    unsigned int *origcolors;
    unsigned int pixel_stride;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    double *iterate_ms;
    unsigned int *iterate_count;
    double *merge_ms;
    unsigned int *merge_iterate_count;
    int *merge_mode;
    tupletable2 colorfreqtable;
    unsigned int i;
    unsigned int n;

    status = SIXEL_FALSE;
    colorfreqtable.size = 0U;
    colorfreqtable.table = NULL;
    if (request == NULL || request->colormap == NULL
            || request->allocator == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    palette = request->palette;
    data = request->data;
    length = request->length;
    depth = request->depth;
    use_reversible = request->use_reversible;
    colormapP = request->colormap;
    origcolors = request->origcolors;
    pixel_stride = request->pixel_stride;
    pixelformat = request->pixelformat;
    allocator = request->allocator;
    logger = request->logger;
    job_seq = request->job_seq;
    engine_name = request->engine_name;
    iterate_ms = request->iterate_ms;
    iterate_count = request->iterate_count;
    merge_ms = request->merge_ms;
    merge_iterate_count = request->merge_iterate_count;
    merge_mode = request->merge_mode;

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
    colormap_request.palette = palette;
    colormap_request.data = data;
    colormap_request.length = length;
    colormap_request.depth = depth;
    colormap_request.use_reversible = reversible_for_quantizer;
    colormap_request.colormap = &colormap;
    colormap_request.origcolors = &origcolors;
    colormap_request.pixel_stride = pixel_stride;
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
