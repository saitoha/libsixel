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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-binning.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "palette-bin-histogram.h"
#include "palette-common-snap.h"
#include "palette-sample.h"
#include "pixelformat.h"
#include "status.h"
#include "weighted-point-set.h"

typedef struct sixel_filter_binning_state {
    sixel_filter_binning_config_t config;
} sixel_filter_binning_state_t;

/*
 * Exact aggregation cannot use the finite-grid histogram key: full coordinate
 * precision participates in equality.  Keep this table private to the filter
 * so the compact-sparse backend remains an implementation choice, not an
 * artifact format exposed to quantizers.
 */
typedef struct sixel_filter_exact_entry {
    uint64_t hash;
    double coordinates[3];
    double weight;
    int occupied;
} sixel_filter_exact_entry_t;

typedef struct sixel_filter_exact_table {
    sixel_filter_exact_entry_t *entries;
    unsigned int capacity;
    unsigned int mask;
    unsigned int size;
} sixel_filter_exact_table_t;

#define SIXEL_FILTER_EXACT_INITIAL_ENTRY_LIMIT 64u

static int
sixel_filter_binning_allocation_size(size_t count,
                                     size_t factor,
                                     size_t entry_size,
                                     size_t *allocation_size)
{
    size_t item_count;

    item_count = 0u;
    if (allocation_size == NULL || factor == 0u || entry_size == 0u
            || count > SIZE_MAX / factor) {
        return 0;
    }
    item_count = count * factor;
    if (item_count > SIZE_MAX / entry_size) {
        return 0;
    }
    *allocation_size = item_count * entry_size;
    return 1;
}

/*
 * Allocate a tuple table with enough space to store "size" tuples of the
 * specified depth. The histogram and the median-cut palette share the
 * allocation layout, including alignment on 32-bit hosts.
 */
SIXELSTATUS
sixel_heckbert_alloc_table(sixel_heckbert_tuple_t ***result,
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
    sixel_heckbert_tuple_t **tbl;
    unsigned int i;

    status = SIXEL_FALSE;
    if (result == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (UINT_MAX / sizeof(sixel_heckbert_tuple_t) < size) {
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

    mainTableSize = size * sizeof(sixel_heckbert_tuple_t *);
    /* The tuple pool must remain double-aligned on 32-bit hosts too. */
    if (mainTableSize > UINT_MAX - (sizeof(double) - 1U)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    mainTableSize = (mainTableSize + sizeof(double) - 1U)
        / sizeof(double) * sizeof(double);
    if ((UINT_MAX - mainTableSize) / sizeof(sixel_heckbert_tuple_t) < size) {
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

    tupleIntSize = sizeof(sixel_heckbert_tuple_t)
                 + (depth - 1U) * sizeof(double);
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
    tbl = (sixel_heckbert_tuple_t **)pool;

    for (i = 0U; i < size; ++i) {
        tbl[i] = (sixel_heckbert_tuple_t *)((char *)pool
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

struct histogram_control {
    unsigned int channel_shift;
    unsigned int channel_bits;
    unsigned int channel_mask;
    int reversible_rounding;
};

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

/* Bucket keys group nearby samples; they do not encode representative colors.
 * Keep the reversible grouping offset to preserve its occupied-cell contract.
 * Representatives are computed from the actual samples in each bucket. */
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

/* Grow only the occupied-cell statistics. Each slot stores an exact count
 * followed by depth channel sums; the dense map stores only slot + 1, with
 * zero reserved for an unseen bucket. Slot order is first occurrence order,
 * the same order formerly recorded by refmap. The byte-length bound keeps
 * every count and 255 * count safely within uint64_t. */
static SIXELSTATUS
histogram_add_slot(uint64_t **moments,
                    size_t *capacity,
                    unsigned int slot,
                    unsigned int depth,
                    size_t limit,
                    sixel_allocator_t *allocator)
{
    size_t grown;
    size_t stride;
    uint64_t *replacement;

    stride = (size_t)depth + 1U;
    if ((size_t)slot == *capacity) {
        grown = *capacity == 0U ? 256U : *capacity * 2U;
        if (grown > limit) {
            grown = limit;
        }
        if (grown <= (size_t)slot ||
                grown > SIZE_MAX / stride / sizeof(uint64_t)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        replacement = sixel_allocator_realloc(allocator, *moments,
            grown * stride * sizeof(uint64_t));
        if (replacement == NULL) {
            sixel_helper_set_additional_message(
                "unable to allocate occupied histogram statistics.");
            return SIXEL_BAD_ALLOCATION;
        }
        *moments = replacement;
        *capacity = grown;
    }
    memset(*moments + (size_t)slot * stride, 0,
        stride * sizeof(uint64_t));
    return SIXEL_OK;
}

/*
 * Construct the histogram consumed by the median-cut splitter.  The control
 * flow is intentionally verbose:
 *
 *   1. Consume every byte pixel supplied by upstream sampling.
 *   2. Quantize each pixel into the dense histogram space.
 *   3. Accumulate exact counts/sums only for occupied cells.
 *   4. Export fractional means, independently of saturated tuple weights.
 */
static SIXELSTATUS
sixel_lut_build_histogram(unsigned char const *data,
                          unsigned int length,
                          unsigned int depth,
                          unsigned int pixel_stride,
                          int pixelformat,
                          int use_reversible,
                          int policy,
                          unsigned int requested_colors,
                          sixel_sample_statistics_t *statistics,
                          sixel_heckbert_histogram_t *colorfreqtable,
                          sixel_allocator_t *allocator)
{
    sixel_sample_statistics_t *owned_statistics;
    SIXELSTATUS status;
    typedef uint32_t unit_t;
    unit_t *slotmap;
    uint64_t *moments;
    uint64_t *moment;
    size_t capacity;
    size_t limit;
    unsigned int slots;
    unit_t slot;
    unsigned int bucket_index;
    unsigned int step;
    size_t hist_size;
    struct histogram_control control;
    unsigned int depth_u;
    unsigned char reversible_pixel[SIXEL_HECKBERT_MAX_CHANNELS];
    unsigned int i;
    unsigned int n;
    unsigned int plane;
    unsigned int channel_stride;
    unsigned char const *bucket_input;
    unsigned int quantized_lut[256];
    uint32_t packed_lut[4][256];
    unsigned int bits;
    int use_fast_pack;
    sixel_palette_bin_range_t range;
    unsigned int value;
    unsigned int address;
    unsigned int strength;
    sixel_sample_stream_t gate_samples;

    owned_statistics = NULL;
    status = SIXEL_FALSE;
    slotmap = NULL;
    moments = NULL;
    capacity = 0U;
    slots = 0U;
    bucket_input = NULL;
    if (colorfreqtable == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    colorfreqtable->preserve_extended = 0;
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
    if (depth > SIXEL_HECKBERT_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "sixel_lut_build_histogram: unsupported channel count.");
        return SIXEL_BAD_ARGUMENT;
    }

    channel_stride = pixel_stride / depth;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat) || channel_stride != 1U) {
        sixel_helper_set_additional_message(
            "sixel_lut_build_histogram: requires byte input.");
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Sampling is upstream, so the histogram walks every supplied
     * pixel.  The stride stays aligned to the caller's layout to avoid losing
     * information after the upstream down-sampler trimmed the dataset.
     * Stop at the last complete pixel without adding stride to the loop
     * index in the comparison: that addition can wrap at UINT_MAX.
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
    strength = sixel_palette_bin_range_get_strength();
    if (strength != 0u && use_fast_pack && !use_reversible &&
            policy == SIXEL_LUT_POLICY_AUTO) {
        /* Retain the sampled RGB values. Only the grouping LUT changes;
         * occupied moments below still accumulate the original bytes.
         * Alpha retains its nominal grid, as it is not a color axis. */
        sixel_palette_bin_range_init(&range, 1u << bits);
        if (statistics == NULL) {
            status = sixel_palette_prepare_sample_statistics(data,
                length - length % pixel_stride, pixelformat,
                sixel_pixelformat_get_colorspace(pixelformat), 0,
                &owned_statistics, allocator, NULL);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            statistics = owned_statistics;
        }
        sixel_sample_stream_init(&gate_samples);
        status = sixel_sample_stream_bind_borrowed_buffer(&gate_samples,
            data, length - length % pixel_stride, pixelformat,
            statistics->colorspace, NULL, SIXEL_PALETTE_SAMPLING_FULL_FRAME,
            SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_palette_bin_range_prepare(&range, statistics,
                &gate_samples, NULL, requested_colors, NULL, NULL,
                SIXEL_PALETTE_BINNING_GRID_NONE, 1, quantized_lut,
                strength, SIXEL_QUANTIZE_MODEL_MEDIANCUT);
        }
        sixel_sample_stream_dispose(&gate_samples);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (range.active[0] || range.active[1] || range.active[2]) {
            for (plane = 0u; plane < 3u; ++plane) {
                for (value = 0u; value < 256u; ++value) {
                    address = sixel_palette_bin_range_sample_index(&range,
                        plane, quantized_lut[value], (double)value, 0u);
                    packed_lut[plane][value] =
                        (uint32_t)address << ((depth_u - 1u - plane) * bits);
                }
            }
        }
    }
    hist_size = histogram_dense_size(depth_u, &control);
    if (hist_size > SIZE_MAX / sizeof(unit_t)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    slotmap = (unit_t *)sixel_allocator_calloc(allocator,
                                                hist_size,
                                                sizeof(unit_t));
    if (slotmap == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for histogram slot map.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    limit = length / pixel_stride;
    if (limit > hist_size) {
        limit = hist_size;
    }

    if (limit == 0U) {
        goto export_histogram;
    }
    /* Reserve the first occupied-cell block before entering the hot loops.
     * Slot insertion still initializes only the cells actually visited. */
    capacity = limit < 256U ? limit : 256U;
    moments = sixel_allocator_malloc(allocator,
        capacity * (depth_u + 1U) * sizeof(*moments));
    if (moments == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    /*
     * Keep the dominant 8bit RGB/RGBA path branch-light so histogram packing
     * does not pay per-pixel reversible checks.
     */
    if (!use_reversible
            && use_fast_pack
            && depth_u == 3U) {
        for (i = 0U; i < length - length % pixel_stride; i += step) {
            bucket_index = histogram_pack_color_fast_rgb(data + i,
                                                         packed_lut);
            slot = slotmap[bucket_index];
            if (slot == 0U) {
                status = histogram_add_slot(&moments, &capacity, slots,
                    depth_u, limit, allocator);
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                slot = ++slots;
                slotmap[bucket_index] = slot;
            }
            moment = moments + (size_t)(slot - 1U) * 4U;
            ++moment[0];
            moment[1] += data[i + 0U];
            moment[2] += data[i + 1U];
            moment[3] += data[i + 2U];
        }
    } else if (!use_reversible
               && use_fast_pack
               && depth_u == 4U) {
        for (i = 0U; i < length - length % pixel_stride; i += step) {
            bucket_index = histogram_pack_color_fast_rgba(data + i,
                                                          packed_lut);
            slot = slotmap[bucket_index];
            if (slot == 0U) {
                status = histogram_add_slot(&moments, &capacity, slots,
                    depth_u, limit, allocator);
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                slot = ++slots;
                slotmap[bucket_index] = slot;
            }
            moment = moments + (size_t)(slot - 1U) * 5U;
            ++moment[0];
            moment[1] += data[i + 0U];
            moment[2] += data[i + 1U];
            moment[3] += data[i + 2U];
            moment[4] += data[i + 3U];
        }
    } else {
        for (i = 0U; i < length - length % pixel_stride; i += step) {
            bucket_input = data + i;
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
            slot = slotmap[bucket_index];
            if (slot == 0U) {
                status = histogram_add_slot(&moments, &capacity, slots,
                    depth_u, limit, allocator);
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                slot = ++slots;
                slotmap[bucket_index] = slot;
            }
            moment = moments + (size_t)(slot - 1U) * (depth_u + 1U);
            ++moment[0];
            for (plane = 0U; plane < depth_u; ++plane) {
                moment[plane + 1U] += bucket_input[plane];
            }
        }
    }

export_histogram:
    colorfreqtable->size = slots;
    status = sixel_heckbert_alloc_table(&colorfreqtable->table, depth, slots,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (i = 0U; i < slots; ++i) {
        moment = moments + (size_t)i * (depth_u + 1U);
        colorfreqtable->table[i]->value = moment[0] > UINT32_MAX
            ? UINT32_MAX : (unsigned int)moment[0];
        for (n = 0U; n < depth; ++n) {
            colorfreqtable->table[i]->components[n]
                = (double)moment[n + 1U] / (double)moment[0];
            if (use_reversible) {
                /* Average the snapped input, then restore its tone grid.
                 * Rounding here is part of reversible mapping only. */
                colorfreqtable->table[i]->components[n]
                    = sixel_palette_reversible_value((unsigned int)floor(
                        colorfreqtable->table[i]->components[n] + 0.5));
            }
        }
    }

    sixel_debugf("%u colors found", colorfreqtable->size);
    status = SIXEL_OK;

cleanup:
    sixel_sample_statistics_unref(owned_statistics);
    sixel_allocator_free(allocator, moments);
    sixel_allocator_free(allocator, slotmap);

    return status;
}
/* Typed float histogram entries retain sub-byte differences and out-of-gamut
 * coordinates. Occurrence weights describe identical triples, not cells on
 * a bounded lookup grid. Lookup selection is independent of this histogram.
 * Scale the nominal span to 255 for existing split/merge metrics, but do not
 * translate signed axes: subtracting an offset would erase tiny values.
 * Snap converts this zero-origin scale back to typed coordinates explicitly. */
static int
sixel_heckbert_native_compare(void const *left, void const *right)
{
    float const *a;
    float const *b;
    unsigned int channel;

    a = *(float const *const *)left;
    b = *(float const *const *)right;
    for (channel = 0U; channel < 3U; ++channel) {
        if (a[channel] < b[channel]) {
            return -1;
        }
        if (a[channel] > b[channel]) {
            return 1;
        }
    }
    return 0;
}

static SIXELSTATUS
sixel_heckbert_native_histogram(unsigned char const *data,
                               unsigned int length,
                               int pixelformat,
                               sixel_heckbert_histogram_t *histogram,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    float const *pixels;
    float const **sorted;
    double range[3];
    unsigned int count;
    unsigned int index;
    unsigned int channel;
    unsigned int unique;
    sixel_heckbert_tuple_t *entry;

    histogram->preserve_extended = 1;
    if (data == NULL || !SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat) ||
            sixel_helper_compute_depth(pixelformat) != 3 * sizeof(float) ||
            length == 0U || length % (3U * sizeof(float)) != 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    count = length / (3U * sizeof(float));
    pixels = (float const *)(void const *)data;
    for (channel = 0U; channel < 3U; ++channel) {
        range[channel] = sixel_pixelformat_float_channel_range(
            pixelformat, (int)channel);
    }
    /* Sort borrowed input references first. Allocating double tuples for
     * every pixel before deduplication exceeds the allocation limit even
     * for a repeated-color 4K image. Only distinct triples need tuples.
     * Positive per-axis scaling preserves order and equality, including
     * signed zero, so sorting finite source values gives the same bins. */
    /* count is nonzero here. Divide by it so this guard also compiles
     * when size_t can represent every unsigned int allocation count. */
    if (sizeof(*sorted) > (size_t)-1 / count) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    sorted = sixel_allocator_malloc(allocator, count * sizeof(*sorted));
    if (sorted == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0U; index < count; ++index) {
        sorted[index] = pixels + index * 3U;
        for (channel = 0U; channel < 3U; ++channel) {
            if (!isfinite(sorted[index][channel])) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
    }
    qsort(sorted, count, sizeof(*sorted), sixel_heckbert_native_compare);
    unique = 1U;
    for (index = 1U; index < count; ++index) {
        if (sixel_heckbert_native_compare(sorted + index - 1U,
                                         sorted + index) != 0) {
            unique++;
        }
    }
    status = sixel_heckbert_alloc_table(&histogram->table, 3U, unique,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    unique = 0U;
    entry = NULL;
    for (index = 0U; index < count; ++index) {
        if (index != 0U &&
                sixel_heckbert_native_compare(sorted + index - 1U,
                                             sorted + index) == 0) {
            entry->value++;
            continue;
        }
        entry = histogram->table[unique++];
        entry->value = 1U;
        for (channel = 0U; channel < 3U; ++channel) {
            entry->components[channel] =
                (double)sorted[index][channel] * 255.0 / range[channel];
        }
    }
    histogram->size = unique;
    status = SIXEL_OK;
end:
    sixel_allocator_free(allocator, sorted);
    return status;
}

static SIXELSTATUS
sixel_filter_binning_apply_heckbert(sixel_filter_t *filter,
    sixel_filter_binning_config_t const *config, sixel_allocator_t *allocator)
{
    sixel_heckbert_binning_request_t const *request;
    sixel_heckbert_histogram_t *histogram;
    unsigned int count;
    int completed;
    SIXELSTATUS status;

    request = config->heckbert;
    histogram = config->heckbert_output;
    if (histogram->table != NULL || request->depth == 0u ||
            request->depth > SIXEL_HECKBERT_MAX_CHANNELS ||
            request->pixel_stride == 0u)
        return SIXEL_BAD_ARGUMENT;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        status = sixel_heckbert_native_histogram(request->data, request->length,
            request->pixelformat, histogram, allocator);
    } else {
        status = sixel_lut_build_histogram(request->data, request->length,
            request->depth, request->pixel_stride, request->pixelformat,
            request->reversible, request->lut_policy, request->requested_colors,
            request->statistics, histogram, allocator);
    }
    if (status != SIXEL_OK)
        return status;

    count = request->length / request->pixel_stride;
    completed = count > (unsigned int)INT_MAX ? INT_MAX : (int)count;
    filter->progress.total_units = completed;
    filter->progress.completed_units = completed;
    status = sixel_filter_update_progress(filter, completed);
    return status == SIXEL_FALSE ? SIXEL_OK : status;
}

static SIXELSTATUS
sixel_filter_binning_apply(sixel_filter_t *filter,
                           sixel_allocator_t *allocator,
                           sixel_timeline_logger_t *logger);

static void
sixel_filter_binning_dispose(sixel_filter_t *filter);

static sixel_filter_vtbl_t const sixel_filter_binning_vtbl = {
    "binning",
    SIXEL_FILTER_KIND_BINNING,
    sixel_filter_binning_apply,
    sixel_filter_binning_dispose,
    NULL,
    NULL,
    NULL
};

static void
sixel_filter_exact_table_clear(sixel_filter_exact_table_t *table)
{
    if (table == NULL) {
        return;
    }
    table->entries = NULL;
    table->capacity = 0u;
    table->mask = 0u;
    table->size = 0u;
}

static SIXELSTATUS
sixel_filter_exact_table_prepare(sixel_filter_exact_table_t *table,
                                 size_t expected_entries,
                                 sixel_allocator_t *allocator)
{
    unsigned int capacity;
    size_t initial_entries;
    size_t threshold;
    size_t index;
    size_t allocation_size;

    capacity = 8u;
    initial_entries = 0u;
    threshold = 0u;
    index = 0u;
    allocation_size = 0u;
    if (table == NULL || expected_entries == 0u || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_filter_exact_table_clear(table);
    initial_entries = expected_entries;
    if (initial_entries > SIXEL_FILTER_EXACT_INITIAL_ENTRY_LIMIT) {
        initial_entries = SIXEL_FILTER_EXACT_INITIAL_ENTRY_LIMIT;
    }
    while (capacity < (1u << 30)) {
        threshold = (size_t)capacity * 7u / 10u;
        if (threshold >= initial_entries) {
            break;
        }
        capacity <<= 1u;
    }
    if (!sixel_filter_binning_allocation_size(
            (size_t)capacity,
            1u,
            sizeof(*table->entries),
            &allocation_size)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    table->entries =
        (sixel_filter_exact_entry_t *)sixel_allocator_malloc(
            allocator,
            allocation_size);
    if (table->entries == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < (size_t)capacity; ++index) {
        table->entries[index].hash = 0u;
        table->entries[index].coordinates[0] = 0.0;
        table->entries[index].coordinates[1] = 0.0;
        table->entries[index].coordinates[2] = 0.0;
        table->entries[index].weight = 0.0;
        table->entries[index].occupied = 0;
    }
    table->capacity = capacity;
    table->mask = capacity - 1u;
    return SIXEL_OK;
}

static void
sixel_filter_exact_table_dispose(sixel_filter_exact_table_t *table,
                                 sixel_allocator_t *allocator)
{
    if (table == NULL || allocator == NULL) {
        return;
    }
    if (table->entries != NULL) {
        sixel_allocator_free(allocator, table->entries);
    }
    sixel_filter_exact_table_clear(table);
}

static SIXELSTATUS
sixel_filter_exact_table_grow(sixel_filter_exact_table_t *table,
                              sixel_allocator_t *allocator)
{
    sixel_filter_exact_entry_t *grown;
    unsigned int old_capacity;
    unsigned int new_capacity;
    unsigned int new_mask;
    unsigned int index;
    unsigned int slot;
    size_t allocation_size;

    grown = NULL;
    old_capacity = 0u;
    new_capacity = 0u;
    new_mask = 0u;
    index = 0u;
    slot = 0u;
    allocation_size = 0u;
    if (table == NULL || table->entries == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    old_capacity = table->capacity;
    if (old_capacity == 0u || old_capacity >= (1u << 30)) {
        return SIXEL_BAD_ALLOCATION;
    }
    new_capacity = old_capacity << 1u;
    new_mask = new_capacity - 1u;
    if (!sixel_filter_binning_allocation_size(
            (size_t)new_capacity,
            1u,
            sizeof(*grown),
            &allocation_size)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    grown = (sixel_filter_exact_entry_t *)sixel_allocator_malloc(
        allocator,
        allocation_size);
    if (grown == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < new_capacity; ++index) {
        grown[index].hash = 0u;
        grown[index].coordinates[0] = 0.0;
        grown[index].coordinates[1] = 0.0;
        grown[index].coordinates[2] = 0.0;
        grown[index].weight = 0.0;
        grown[index].occupied = 0;
    }
    for (index = 0u; index < old_capacity; ++index) {
        if (table->entries[index].occupied == 0) {
            continue;
        }
        slot = (unsigned int)table->entries[index].hash & new_mask;
        while (grown[slot].occupied != 0) {
            slot = (slot + 1u) & new_mask;
        }
        grown[slot] = table->entries[index];
    }
    sixel_allocator_free(allocator, table->entries);
    table->entries = grown;
    table->capacity = new_capacity;
    table->mask = new_mask;
    return SIXEL_OK;
}

static uint64_t
#if defined(__clang__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
sixel_filter_exact_mix(uint64_t value)
{
    /* SplitMix64 depends on multiplication modulo 2^64. */
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static uint64_t
#if defined(__clang__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
sixel_filter_exact_coordinate_hash(double const coordinates[3])
{
    uint64_t hash;
    uint64_t bits;
    unsigned int channel;

    hash = UINT64_C(0x9e3779b97f4a7c15);
    bits = 0u;
    channel = 0u;
    for (channel = 0u; channel < 3u; ++channel) {
        /* Numeric equality treats both zero signs as the same coordinate. */
        if (coordinates[channel] != 0.0) {
            memcpy(&bits, coordinates + channel, sizeof(bits));
        } else {
            bits = 0u;
        }
        /* Coordinate hashing also depends on arithmetic modulo 2^64. */
        hash ^= sixel_filter_exact_mix(
            bits + (uint64_t)channel * UINT64_C(0x9e3779b97f4a7c15));
        hash = sixel_filter_exact_mix(hash);
    }
    return hash;
}

static int
sixel_filter_binning_coordinates_are_valid(double const coordinates[3])
{
    unsigned int channel;

    channel = 0u;
    if (coordinates == NULL) {
        return 0;
    }
    for (channel = 0u; channel < 3u; ++channel) {
        if (!(coordinates[channel] <= DBL_MAX &&
              coordinates[channel] >= -DBL_MAX)) {
            return 0;
        }
    }
    return 1;
}

static int
sixel_filter_exact_coordinates_equal(double const first[3],
                                     double const second[3])
{
    return first[0] == second[0] &&
        first[1] == second[1] &&
        first[2] == second[2];
}

static SIXELSTATUS
sixel_filter_exact_table_add(sixel_filter_exact_table_t *table,
                             double const coordinates[3],
                             double weight,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_filter_exact_entry_t *entry;
    uint64_t hash;
    unsigned int slot;

    status = SIXEL_OK;
    entry = NULL;
    hash = 0u;
    slot = 0u;
    if (table == NULL || table->entries == NULL || allocator == NULL ||
            !sixel_filter_binning_coordinates_are_valid(coordinates)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (weight <= 0.0) {
        return SIXEL_OK;
    }
    hash = sixel_filter_exact_coordinate_hash(coordinates);
    slot = (unsigned int)hash & table->mask;
    while (table->entries[slot].occupied != 0) {
        entry = table->entries + slot;
        if (entry->hash == hash &&
                sixel_filter_exact_coordinates_equal(
                    entry->coordinates,
                    coordinates)) {
            entry->weight += weight;
            return SIXEL_OK;
        }
        slot = (slot + 1u) & table->mask;
    }
    if ((size_t)(table->size + 1u) * 10u >
            (size_t)table->capacity * 7u) {
        status = sixel_filter_exact_table_grow(table, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        slot = (unsigned int)hash & table->mask;
        while (table->entries[slot].occupied != 0) {
            slot = (slot + 1u) & table->mask;
        }
    }
    entry = table->entries + slot;
    entry->hash = hash;
    memcpy(entry->coordinates,
           coordinates,
           sizeof(entry->coordinates));
    entry->weight = weight;
    entry->occupied = 1;
    ++table->size;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_binning_add_exact(
    sixel_filter_exact_table_t *table,
    sixel_weighted_point_set_t const *input,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    size_t index;

    status = SIXEL_OK;
    index = 0u;
    if (table == NULL || input == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    for (index = 0u; index < input->point_count; ++index) {
        status = sixel_filter_exact_table_add(
            table,
            input->coordinates + index * 3u,
            1.0,
            allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_binning_add_samples(
    sixel_palette_bin_histogram_t *histogram,
    sixel_weighted_point_set_t const *input,
    sixel_filter_binning_config_t const *config,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_palette_binning_policy_t policy;
    size_t index;
    unsigned int channel;
    double sample[3];
    double mapped[3];
    double weight;
    sixel_palette_bin_range_t range;
    unsigned int bin;
    int adaptive;
    unsigned int strength;

    status = SIXEL_OK;
    policy = SIXEL_PALETTE_BINNING_AUTO;
    index = 0u;
    channel = 0u;
    memset(sample, 0, sizeof(sample));
    memset(mapped, 0, sizeof(mapped));
    weight = 1.0;
    if (histogram == NULL || input == NULL || config == NULL ||
            config->binning == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    policy = (sixel_palette_binning_policy_t)
        config->binning->policy.effective;

    strength = sixel_palette_bin_range_get_strength();
    adaptive = strength != 0u && config->adaptive_range &&
        policy == SIXEL_PALETTE_BINNING_HARD;
    range.levels = histogram->bin_count;
    if (adaptive) {
        sixel_palette_bin_range_init(&range, histogram->bin_count);
        if (input->statistics == NULL ||
                input->statistics->colorspace != input->colorspace ||
                input->statistics->point_count != input->point_count ||
                input->statistics->byte_coordinates ==
                    (config->input_is_float32 != 0)) {
            return SIXEL_LOGIC_ERROR;
        }
        status = sixel_palette_bin_range_prepare(&range, input->statistics,
            NULL, input, config->requested_colors, config->scale,
            config->offset, config->binning->grid_map, 0, NULL, strength,
            config->quantize_model);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        adaptive = range.active[0] || range.active[1] || range.active[2];
    }

    for (index = 0u; index < input->point_count; ++index) {
        for (channel = 0u; channel < 3u; ++channel) {
            sample[channel] = input->coordinates[index * 3u + channel];
            mapped[channel] = sixel_palette_bin_map_sample_to_unit(
                sample[channel],
                config->input_is_float32,
                channel,
                config->scale,
                config->offset,
                config->binning->grid_map);
            if (adaptive) {
                bin = (unsigned int)(mapped[channel] * range.levels);
                if (bin >= range.levels) {
                    bin = range.levels - 1u;
                }
                bin = sixel_palette_bin_range_sample_index(&range,
                    channel, bin,
                    config->input_is_float32 ? mapped[channel] :
                    sample[channel], index);
                mapped[channel] = ((double)bin + 0.5) / range.levels;
            }
        }
        if (policy == SIXEL_PALETTE_BINNING_SOFT) {
            status = sixel_palette_bin_histogram_add_soft_trilinear(
                histogram,
                sample,
                mapped,
                weight,
                allocator);
        } else {
            status = sixel_palette_bin_histogram_add_hard(histogram,
                                                          sample,
                                                          mapped,
                                                          weight,
                                                          allocator);
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_binning_add_stream(
    sixel_palette_bin_histogram_t *histogram,
    sixel_sample_stream_t const *stream,
    sixel_filter_binning_config_t const *config,
    sixel_allocator_t *allocator,
    double *total_weight_out)
{
    SIXELSTATUS status;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    sixel_palette_binning_policy_t policy;
    size_t index;
    unsigned int channel;
    unsigned int channels;
    int input_is_float32;
    int visible;
    double sample[3];
    double mapped[3];
    double total_weight;
    sixel_palette_bin_range_t range;
    unsigned int bin;
    int adaptive;
    unsigned int strength;

    status = SIXEL_BAD_ARGUMENT;
    memset(&view, 0, sizeof(view));
    memset(&transparency, 0, sizeof(transparency));
    policy = SIXEL_PALETTE_BINNING_AUTO;
    index = 0u;
    channel = 0u;
    channels = 0u;
    input_is_float32 = 0;
    visible = 0;
    memset(sample, 0, sizeof(sample));
    memset(mapped, 0, sizeof(mapped));
    total_weight = 0.0;
    if (histogram == NULL || stream == NULL || config == NULL ||
            config->binning == NULL || allocator == NULL ||
            total_weight_out == NULL) {
        return status;
    }
    status = sixel_sample_stream_get_view(stream,
                                                   &view,
                                                   &transparency,
                                                   &channels,
                                                   &input_is_float32);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    policy = (sixel_palette_binning_policy_t)
        config->binning->policy.effective;
    if (input_is_float32 &&
            (!sixel_filter_binning_coordinates_are_valid(config->scale) ||
             !sixel_filter_binning_coordinates_are_valid(config->offset) ||
             config->scale[0] <= 0.0 || config->scale[1] <= 0.0 ||
             config->scale[2] <= 0.0)) {
        return SIXEL_BAD_ARGUMENT;
    }

    strength = sixel_palette_bin_range_get_strength();
    adaptive = strength != 0u && config->adaptive_range &&
        policy == SIXEL_PALETTE_BINNING_HARD;
    range.levels = histogram->bin_count;
    if (adaptive) {
        sixel_palette_bin_range_init(&range, histogram->bin_count);
        if (stream->statistics == NULL ||
                stream->statistics->colorspace != stream->colorspace ||
                stream->statistics->byte_coordinates ==
                    (input_is_float32 != 0)) {
            return SIXEL_LOGIC_ERROR;
        }
        status = sixel_palette_bin_range_prepare(&range, stream->statistics,
            stream, NULL, config->requested_colors, config->scale,
            config->offset, config->binning->grid_map, 0, NULL, strength,
            config->quantize_model);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        adaptive = range.active[0] || range.active[1] || range.active[2];
    }

    for (index = 0u; index < stream->point_count; ++index) {
        status = sixel_sample_stream_read(
            &view,
            &transparency,
            channels,
            input_is_float32,
            index,
            sample,
            &visible);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (!visible) {
            continue;
        }
        for (channel = 0u; channel < 3u; ++channel) {
            mapped[channel] = sixel_palette_bin_map_sample_to_unit(
                sample[channel],
                input_is_float32,
                channel,
                config->scale,
                config->offset,
                config->binning->grid_map);
            if (adaptive) {
                bin = (unsigned int)(mapped[channel] * range.levels);
                if (bin >= range.levels) {
                    bin = range.levels - 1u;
                }
                bin = sixel_palette_bin_range_sample_index(&range,
                    channel, bin,
                    input_is_float32 ? mapped[channel] : sample[channel],
                    (size_t)total_weight);
                mapped[channel] = ((double)bin + 0.5) / range.levels;
            }
        }
        if (policy == SIXEL_PALETTE_BINNING_SOFT) {
            status = sixel_palette_bin_histogram_add_soft_trilinear(
                histogram,
                sample,
                mapped,
                1.0,
                allocator);
        } else {
            status = sixel_palette_bin_histogram_add_hard(histogram,
                                                          sample,
                                                          mapped,
                                                          1.0,
                                                          allocator);
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
        total_weight += 1.0;
    }
    *total_weight_out = total_weight;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_binning_add_exact_stream(
    sixel_filter_exact_table_t *table,
    sixel_sample_stream_t const *stream,
    sixel_filter_binning_config_t const *config,
    sixel_allocator_t *allocator,
    double *total_weight_out)
{
    SIXELSTATUS status;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    size_t index;
    unsigned int channels;
    int input_is_float32;
    int visible;
    double sample[3];
    double total_weight;

    status = SIXEL_BAD_ARGUMENT;
    memset(&view, 0, sizeof(view));
    memset(&transparency, 0, sizeof(transparency));
    index = 0u;
    channels = 0u;
    input_is_float32 = 0;
    visible = 0;
    memset(sample, 0, sizeof(sample));
    total_weight = 0.0;
    if (table == NULL || stream == NULL || config == NULL ||
            allocator == NULL || total_weight_out == NULL) {
        return status;
    }
    status = sixel_sample_stream_get_view(stream,
                                                   &view,
                                                   &transparency,
                                                   &channels,
                                                   &input_is_float32);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (index = 0u; index < stream->point_count; ++index) {
        status = sixel_sample_stream_read(
            &view,
            &transparency,
            channels,
            input_is_float32,
            index,
            sample,
            &visible);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (!visible) {
            continue;
        }
        status = sixel_filter_exact_table_add(table,
                                              sample,
                                              1.0,
                                              allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        total_weight += 1.0;
    }
    *total_weight_out = total_weight;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_binning_publish_direct_stream(
    sixel_sample_stream_t const *stream,
    sixel_filter_binning_config_t const *config,
    sixel_weighted_point_set_t *output,
    sixel_palette_binning_state_t *binning,
    int colorspace,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    size_t index;
    size_t visible_count;
    unsigned int channels;
    int input_is_float32;
    int visible;
    double sample[3];
    double *coordinates;

    status = SIXEL_BAD_ARGUMENT;
    memset(&view, 0, sizeof(view));
    memset(&transparency, 0, sizeof(transparency));
    index = 0u;
    visible_count = 0u;
    channels = 0u;
    input_is_float32 = 0;
    visible = 0;
    memset(sample, 0, sizeof(sample));
    coordinates = NULL;
    if (stream == NULL || config == NULL || output == NULL ||
            binning == NULL || allocator == NULL ||
            stream->point_count > SIZE_MAX / 3u / sizeof(double)) {
        return status;
    }
    status = sixel_sample_stream_get_view(stream,
                                                   &view,
                                                   &transparency,
                                                   &channels,
                                                   &input_is_float32);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    coordinates = (double *)sixel_allocator_malloc(
        allocator,
        stream->point_count * 3u * sizeof(double));
    if (coordinates == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < stream->point_count; ++index) {
        status = sixel_sample_stream_read(
            &view,
            &transparency,
            channels,
            input_is_float32,
            index,
            sample,
            &visible);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (!visible) {
            continue;
        }
        memcpy(coordinates + visible_count * 3u,
               sample,
               sizeof(sample));
        ++visible_count;
    }
    if (visible_count == 0u) {
        sixel_helper_set_additional_message(
            "sixel_filter_binning_apply: no visible samples.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    status = sixel_weighted_point_set_take_owned(
        output,
        &coordinates,
        NULL,
        visible_count,
        (double)visible_count,
        colorspace,
        binning,
        allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_policy_mark_executed(&binning->policy);

cleanup:
    if (coordinates != NULL) {
        sixel_allocator_free(allocator, coordinates);
    }
    return status;
}

static int
sixel_filter_binning_compare_entry_key(void const *lhs,
                                       void const *rhs)
{
    sixel_palette_bin_entry_t const *const *first;
    sixel_palette_bin_entry_t const *const *second;

    first = (sixel_palette_bin_entry_t const *const *)lhs;
    second = (sixel_palette_bin_entry_t const *const *)rhs;
    if ((*first)->key < (*second)->key) {
        return -1;
    }
    if ((*first)->key > (*second)->key) {
        return 1;
    }
    return 0;
}

static SIXELSTATUS
sixel_filter_binning_publish(
    sixel_palette_bin_histogram_t const *histogram,
    sixel_weighted_point_set_t *output,
    sixel_palette_binning_state_t *binning,
    double total_weight,
    int colorspace,
    sixel_filter_binning_output_order_t output_order,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int index;
    unsigned int channel;
    size_t output_index;
    double *coordinates;
    double *weights;
    sixel_palette_bin_entry_t const **ordered_entries;
    sixel_palette_bin_entry_t const *entry;
    size_t coordinates_size;
    size_t weights_size;
    size_t ordered_entries_size;

    status = SIXEL_FALSE;
    index = 0u;
    channel = 0u;
    output_index = 0u;
    coordinates = NULL;
    weights = NULL;
    ordered_entries = NULL;
    entry = NULL;
    coordinates_size = 0u;
    weights_size = 0u;
    ordered_entries_size = 0u;
    if (histogram == NULL || output == NULL || binning == NULL ||
            allocator == NULL || histogram->size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_filter_binning_allocation_size(
            (size_t)histogram->size,
            3u,
            sizeof(double),
            &coordinates_size)
            || !sixel_filter_binning_allocation_size(
                (size_t)histogram->size,
                1u,
                sizeof(double),
                &weights_size)
            || !sixel_filter_binning_allocation_size(
                (size_t)histogram->size,
                1u,
                sizeof(*ordered_entries),
                &ordered_entries_size)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    coordinates = (double *)sixel_allocator_malloc(
        allocator,
        coordinates_size);
    weights = (double *)sixel_allocator_malloc(
        allocator,
        weights_size);
    if (coordinates == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    if (output_order ==
            SIXEL_FILTER_BINNING_OUTPUT_BIN_KEY_ASCENDING) {
        ordered_entries =
            (sixel_palette_bin_entry_t const **)sixel_allocator_malloc(
                allocator,
                ordered_entries_size);
        if (ordered_entries == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (index = 0u; index < histogram->capacity; ++index) {
            entry = histogram->entries + index;
            if (entry->key == UINT32_MAX || entry->weight <= 0.0) {
                continue;
            }
            ordered_entries[output_index] = entry;
            ++output_index;
        }
        if (output_index != (size_t)histogram->size) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
        qsort(ordered_entries,
              histogram->size,
              sizeof(*ordered_entries),
              sixel_filter_binning_compare_entry_key);
        output_index = 0u;
        for (index = 0u; index < histogram->size; ++index) {
            entry = ordered_entries[index];
            weights[output_index] = entry->weight;
            for (channel = 0u; channel < 3u; ++channel) {
                coordinates[output_index * 3u + channel] =
                    entry->sum[channel] / entry->weight;
            }
            ++output_index;
        }
    } else {
        for (index = 0u; index < histogram->capacity; ++index) {
            entry = histogram->entries + index;
            if (entry->key == UINT32_MAX || entry->weight <= 0.0) {
                continue;
            }
            weights[output_index] = entry->weight;
            for (channel = 0u; channel < 3u; ++channel) {
                coordinates[output_index * 3u + channel] =
                    entry->sum[channel] / entry->weight;
            }
            ++output_index;
        }
    }
    if (output_index != (size_t)histogram->size) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = sixel_weighted_point_set_take_owned(output,
                                                 &coordinates,
                                                 &weights,
                                                 output_index,
                                                 total_weight,
                                                 colorspace,
                                                 binning,
                                                 allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_policy_mark_executed(&binning->policy);

cleanup:
    if (ordered_entries != NULL) {
        sixel_allocator_free(allocator, ordered_entries);
    }
    if (coordinates != NULL) {
        sixel_allocator_free(allocator, coordinates);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    return status;
}

static SIXELSTATUS
sixel_filter_binning_publish_exact(
    sixel_filter_exact_table_t const *table,
    sixel_weighted_point_set_t *output,
    sixel_palette_binning_state_t *binning,
    double total_weight,
    int colorspace,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int index;
    size_t output_index;
    double *coordinates;
    double *weights;
    sixel_filter_exact_entry_t const *entry;
    size_t coordinates_size;
    size_t weights_size;

    status = SIXEL_FALSE;
    index = 0u;
    output_index = 0u;
    coordinates = NULL;
    weights = NULL;
    entry = NULL;
    coordinates_size = 0u;
    weights_size = 0u;
    if (table == NULL || output == NULL || binning == NULL ||
            allocator == NULL || table->size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_filter_binning_allocation_size(
            (size_t)table->size,
            3u,
            sizeof(double),
            &coordinates_size)
            || !sixel_filter_binning_allocation_size(
                (size_t)table->size,
                1u,
                sizeof(double),
                &weights_size)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    coordinates = (double *)sixel_allocator_malloc(
        allocator,
        coordinates_size);
    weights = (double *)sixel_allocator_malloc(
        allocator,
        weights_size);
    if (coordinates == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (index = 0u; index < table->capacity; ++index) {
        entry = table->entries + index;
        if (entry->occupied == 0 || entry->weight <= 0.0) {
            continue;
        }
        memcpy(coordinates + output_index * 3u,
               entry->coordinates,
               sizeof(entry->coordinates));
        weights[output_index] = entry->weight;
        ++output_index;
    }
    if (output_index != (size_t)table->size) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = sixel_weighted_point_set_take_owned(output,
                                                 &coordinates,
                                                 &weights,
                                                 output_index,
                                                 total_weight,
                                                 colorspace,
                                                 binning,
                                                 allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_policy_mark_executed(&binning->policy);

cleanup:
    if (coordinates != NULL) {
        sixel_allocator_free(allocator, coordinates);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    return status;
}

static SIXELSTATUS
sixel_filter_binning_apply(sixel_filter_t *filter,
                           sixel_allocator_t *allocator,
                           sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_binning_state_t *state;
    sixel_palette_binning_state_t *binning;
    sixel_palette_binning_policy_t policy;
    sixel_weighted_point_set_t *input;
    sixel_sample_stream_t *samples;
    sixel_weighted_point_set_t *output;
    sixel_palette_bin_histogram_t histogram;
    sixel_filter_exact_table_t exact_table;
    size_t expected_entries;
    size_t source_point_count;
    double total_weight;
    int completed;

    status = SIXEL_FALSE;
    state = NULL;
    binning = NULL;
    policy = SIXEL_PALETTE_BINNING_AUTO;
    input = NULL;
    samples = NULL;
    output = NULL;
    sixel_palette_bin_histogram_clear(&histogram);
    sixel_filter_exact_table_clear(&exact_table);
    expected_entries = 0u;
    source_point_count = 0u;
    total_weight = 0.0;
    completed = 0;
    (void)logger;
    if (filter == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    state = (sixel_filter_binning_state_t *)filter->userdata;
    if (state != NULL && state->config.binning == NULL)
        return sixel_filter_binning_apply_heckbert(filter, &state->config,
                                                  allocator);
    input = filter->input.weighted_points;
    samples = filter->input.sample_stream;
    output = filter->output.weighted_points;
    if (state == NULL || output == NULL ||
            (input == NULL && samples == NULL) ||
            (input != NULL && samples != NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    binning = state->config.binning;
    if (binning == NULL ||
            binning->policy.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            output->ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            (state->config.output_order !=
                 SIXEL_FILTER_BINNING_OUTPUT_NATIVE &&
             state->config.output_order !=
                 SIXEL_FILTER_BINNING_OUTPUT_BIN_KEY_ASCENDING)) {
        return SIXEL_LOGIC_ERROR;
    }
    if (input != NULL) {
        if (input->ownership == SIXEL_WEIGHTED_POINT_EMPTY ||
                input->policy != SIXEL_PALETTE_BINNING_NONE ||
                input->weights != NULL ||
                filter->output.colorspace != input->colorspace) {
            return SIXEL_LOGIC_ERROR;
        }
        source_point_count = input->point_count;
        total_weight = input->total_weight;
    } else {
        if (samples->storage == SIXEL_SAMPLE_STREAM_EMPTY ||
                (samples->storage ==
                     SIXEL_SAMPLE_STREAM_BORROWED_BUFFER
                 ? samples->buffer == NULL : samples->frame == NULL) ||
                filter->output.colorspace != samples->colorspace) {
            return SIXEL_LOGIC_ERROR;
        }
        source_point_count = samples->point_count;
    }
    policy = (sixel_palette_binning_policy_t)binning->policy.effective;
    if ((policy != SIXEL_PALETTE_BINNING_NONE &&
         policy != SIXEL_PALETTE_BINNING_EXACT &&
         policy != SIXEL_PALETTE_BINNING_HARD &&
         policy != SIXEL_PALETTE_BINNING_SOFT) ||
            binning->source_point_count != source_point_count ||
            (policy == SIXEL_PALETTE_BINNING_NONE &&
             binning->backend != SIXEL_PALETTE_BINNING_BACKEND_DIRECT) ||
            (policy != SIXEL_PALETTE_BINNING_NONE &&
             binning->backend !=
                 SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE)) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (policy == SIXEL_PALETTE_BINNING_NONE) {
        if (samples == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        status = sixel_filter_binning_publish_direct_stream(
            samples,
            &state->config,
            output,
            binning,
            filter->output.colorspace,
            allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        goto progress;
    }

    if (policy == SIXEL_PALETTE_BINNING_EXACT) {
        status = sixel_filter_exact_table_prepare(&exact_table,
                                                  source_point_count,
                                                  allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (input != NULL) {
            status = sixel_filter_binning_add_exact(&exact_table,
                                                    input,
                                                    allocator);
        } else {
            status = sixel_filter_binning_add_exact_stream(
                &exact_table,
                samples,
                &state->config,
                allocator,
                &total_weight);
        }
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (exact_table.size == 0u) {
            sixel_helper_set_additional_message(
                "sixel_filter_binning_apply: no visible samples.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        status = sixel_filter_binning_publish_exact(
            &exact_table,
            output,
            binning,
            total_weight,
            filter->output.colorspace,
            allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        goto progress;
    }

    /*
     * The resolver has already bounded occupancy by both source
     * contributions and the finite grid domain.  Using the raw sample count
     * here would make a small hard grid allocate in proportion to image size.
     */
    expected_entries = binning->entry_capacity_bound;
    status = sixel_palette_bin_histogram_init(
        &histogram,
        expected_entries,
        binning->bits_per_axis,
        allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (input != NULL) {
        status = sixel_filter_binning_add_samples(&histogram,
                                                  input,
                                                  &state->config,
                                                  allocator);
    } else {
        status = sixel_filter_binning_add_stream(&histogram,
                                                 samples,
                                                 &state->config,
                                                 allocator,
                                                 &total_weight);
    }
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (histogram.size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_filter_binning_apply: no visible samples.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    status = sixel_filter_binning_publish(&histogram,
                                          output,
                                          binning,
                                          total_weight,
                                          filter->output.colorspace,
                                          state->config.output_order,
                                          allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

progress:
    /*
     * Expose the population seen by the quantizer.  The source population is
     * not enough to reproduce performance or quality comparisons because a
     * finite grid can have very different occupancy in different color
     * spaces even when its bits-per-axis value is unchanged.
     */
    sixel_trace_topic_message(
        "palette_contract",
        "LSXBSTAT1|bits=%u|source_points=%" SIXEL_PRI_SIZE
        "|effective_points=%" SIXEL_PRI_SIZE,
        binning->bits_per_axis,
        SIXEL_PRINTF_SIZE(source_point_count),
        SIXEL_PRINTF_SIZE(output->point_count));
    completed = source_point_count > (size_t)INT_MAX
        ? INT_MAX : (int)source_point_count;
    filter->progress.total_units = completed;
    filter->progress.completed_units = completed;
    status = sixel_filter_update_progress(filter, completed);
    if (status == SIXEL_FALSE) {
        status = SIXEL_OK;
    }

cleanup:
    sixel_palette_bin_histogram_dispose(&histogram, allocator);
    sixel_filter_exact_table_dispose(&exact_table, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_weighted_point_set_dispose(output);
    }
    return status;
}

static void
sixel_filter_binning_dispose(sixel_filter_t *filter)
{
    sixel_filter_binning_state_t *state;

    if (filter == NULL) {
        return;
    }
    state = (sixel_filter_binning_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_binning_init(
    sixel_filter_t *filter,
    sixel_filter_binning_config_t const *config)
{
    SIXELSTATUS status;
    sixel_filter_binning_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;
    if (filter == NULL || config == NULL ||
            (config->binning == NULL &&
             (config->heckbert == NULL || config->heckbert_output == NULL))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((config->output_order != SIXEL_FILTER_BINNING_OUTPUT_NATIVE &&
         config->output_order !=
             SIXEL_FILTER_BINNING_OUTPUT_BIN_KEY_ASCENDING)) {
        return SIXEL_BAD_ARGUMENT;
    }
    state = (sixel_filter_binning_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        sixel_helper_set_additional_message(
            "sixel_filter_binning_init: malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    state->config = *config;
    status = sixel_filter_init_with_vtbl(filter,
                                         &sixel_filter_binning_vtbl,
                                         state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }
    sixel_filter_set_progress(filter, NULL, NULL, 1);
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
