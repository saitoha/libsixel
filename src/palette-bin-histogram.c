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

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <sixel.h>

#include "palette-bin-histogram.h"

void
sixel_palette_bin_histogram_clear(
    sixel_palette_bin_histogram_t *histogram)
{
    if (histogram == NULL) {
        return;
    }
    histogram->entries = NULL;
    histogram->capacity = 0u;
    histogram->mask = 0u;
    histogram->size = 0u;
    histogram->bits_per_axis = 0u;
    histogram->bin_count = 0u;
}

static uint32_t
sixel_palette_bin_hash_u32(uint32_t key)
{
    key ^= key >> 16;
    /* Keep the low 32 bits without unsigned-overflow reports. */
    key = (uint32_t)((uint64_t)key * 0x7feb352dU);
    key ^= key >> 15;
    key = (uint32_t)((uint64_t)key * 0x846ca68bU);
    key ^= key >> 16;

    return key;
}

static unsigned int
sixel_palette_bin_recommended_capacity(size_t expected)
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

static int
sixel_palette_bin_allocation_size(unsigned int capacity,
                                  size_t entry_size,
                                  size_t *allocation_size)
{
    size_t count;

    count = (size_t)capacity;
    if (allocation_size == NULL || entry_size == 0u ||
            count > SIZE_MAX / entry_size) {
        return 0;
    }
    *allocation_size = count * entry_size;
    return 1;
}

SIXELSTATUS
sixel_palette_bin_histogram_init(
    sixel_palette_bin_histogram_t *histogram,
    size_t expected_entries,
    unsigned int bits_per_axis,
    sixel_allocator_t *allocator)
{
    size_t index;
    size_t allocation_size;
    unsigned int capacity;

    index = 0u;
    allocation_size = 0u;
    capacity = 0u;
    if (histogram == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (bits_per_axis < SIXEL_PALETTE_BINNING_MIN_BITS ||
            bits_per_axis > SIXEL_PALETTE_BINNING_MAX_BITS) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_palette_bin_histogram_clear(histogram);
    histogram->bits_per_axis = bits_per_axis;
    histogram->bin_count = 1u << bits_per_axis;

    capacity = sixel_palette_bin_recommended_capacity(expected_entries);
    if (!sixel_palette_bin_allocation_size(
            capacity,
            sizeof(*histogram->entries),
            &allocation_size)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    histogram->entries =
        (sixel_palette_bin_entry_t *)sixel_allocator_malloc(
            allocator,
            allocation_size);
    if (histogram->entries == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < (size_t)capacity; ++index) {
        histogram->entries[index].key = UINT32_MAX;
        histogram->entries[index].weight = 0.0;
        histogram->entries[index].sum[0] = 0.0;
        histogram->entries[index].sum[1] = 0.0;
        histogram->entries[index].sum[2] = 0.0;
    }
    histogram->capacity = capacity;
    histogram->mask = capacity - 1u;

    return SIXEL_OK;
}

void
sixel_palette_bin_histogram_dispose(
    sixel_palette_bin_histogram_t *histogram,
    sixel_allocator_t *allocator)
{
    if (histogram == NULL || allocator == NULL) {
        return;
    }
    if (histogram->entries != NULL) {
        sixel_allocator_free(allocator, histogram->entries);
    }
    sixel_palette_bin_histogram_clear(histogram);
}

static SIXELSTATUS
sixel_palette_bin_histogram_grow(
    sixel_palette_bin_histogram_t *histogram,
    sixel_allocator_t *allocator)
{
    sixel_palette_bin_entry_t *grown;
    unsigned int old_capacity;
    unsigned int new_capacity;
    unsigned int new_mask;
    unsigned int slot;
    unsigned int index;
    size_t allocation_size;

    grown = NULL;
    old_capacity = 0u;
    new_capacity = 0u;
    new_mask = 0u;
    slot = 0u;
    index = 0u;
    allocation_size = 0u;
    if (histogram == NULL || allocator == NULL ||
            histogram->entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    old_capacity = histogram->capacity;
    if (old_capacity == 0u || old_capacity >= (1u << 30)) {
        return SIXEL_BAD_ALLOCATION;
    }
    new_capacity = old_capacity << 1u;
    new_mask = new_capacity - 1u;
    if (!sixel_palette_bin_allocation_size(new_capacity,
                                           sizeof(*grown),
                                           &allocation_size)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    grown = (sixel_palette_bin_entry_t *)sixel_allocator_malloc(
        allocator,
        allocation_size);
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
        slot = sixel_palette_bin_hash_u32(
            histogram->entries[index].key) & new_mask;
        while (grown[slot].key != UINT32_MAX) {
            slot = (slot + 1u) & new_mask;
        }
        grown[slot] = histogram->entries[index];
    }

    sixel_allocator_free(allocator, histogram->entries);
    histogram->entries = grown;
    histogram->capacity = new_capacity;
    histogram->mask = new_mask;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_bin_histogram_add(
    sixel_palette_bin_histogram_t *histogram,
    uint32_t key,
    double weight,
    double const sample[3],
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int slot;
    double *sum;

    status = SIXEL_OK;
    slot = 0u;
    sum = NULL;
    if (histogram == NULL || histogram->entries == NULL ||
            sample == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (weight <= 0.0) {
        return SIXEL_OK;
    }

    if ((size_t)(histogram->size + 1u) * 10u >
            (size_t)histogram->capacity * 7u) {
        status = sixel_palette_bin_histogram_grow(histogram, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    slot = sixel_palette_bin_hash_u32(key) & histogram->mask;
    while (histogram->entries[slot].key != UINT32_MAX &&
            histogram->entries[slot].key != key) {
        slot = (slot + 1u) & histogram->mask;
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
sixel_palette_bin_clamp_unit(double value)
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
sixel_palette_bin_srgb_encode(double value)
{
    double clamped;

    clamped = sixel_palette_bin_clamp_unit(value);
    if (clamped <= 0.0031308) {
        return clamped * 12.92;
    }
    return 1.055 * pow(clamped, 1.0 / 2.4) - 0.055;
}

double
sixel_palette_bin_map_sample_to_unit(
    double sample,
    int input_is_float32,
    unsigned int channel,
    double const *scale,
    double const *offset,
    sixel_palette_binning_grid_map_t grid_map)
{
    double unit;

    unit = 0.0;
    if (channel >= 3u ||
            (grid_map != SIXEL_PALETTE_BINNING_GRID_UNIFORM &&
             grid_map != SIXEL_PALETTE_BINNING_GRID_UNIFORM_256 &&
             grid_map != SIXEL_PALETTE_BINNING_GRID_SRGB)) {
        return 0.0;
    }
    if (input_is_float32) {
        if (scale != NULL && offset != NULL && scale[channel] > 0.0) {
            unit = sample * scale[channel];
            unit += offset[channel];
            unit /= grid_map == SIXEL_PALETTE_BINNING_GRID_UNIFORM_256
                ? 256.0 : 255.0;
        }
    } else {
        unit = sample /
            (grid_map == SIXEL_PALETTE_BINNING_GRID_UNIFORM_256
             ? 256.0 : 255.0);
    }
    unit = sixel_palette_bin_clamp_unit(unit);
    if (grid_map == SIXEL_PALETTE_BINNING_GRID_SRGB) {
        unit = sixel_palette_bin_srgb_encode(unit);
    }
    return sixel_palette_bin_clamp_unit(unit);
}

static uint32_t
sixel_palette_bin_pack_key(unsigned int first,
                           unsigned int second,
                           unsigned int third,
                           unsigned int bits_per_axis)
{
    return (uint32_t)((first << (bits_per_axis * 2u)) |
                      (second << bits_per_axis) | third);
}

SIXELSTATUS
sixel_palette_bin_histogram_add_hard(
    sixel_palette_bin_histogram_t *histogram,
    double const sample[3],
    double const mapped[3],
    double weight,
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
    if (histogram == NULL || sample == NULL || mapped == NULL ||
            allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (weight <= 0.0) {
        return SIXEL_OK;
    }
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
    key = sixel_palette_bin_pack_key(index[0],
                                     index[1],
                                     index[2],
                                     histogram->bits_per_axis);

    return sixel_palette_bin_histogram_add(histogram,
                                           key,
                                           weight,
                                           sample,
                                           allocator);
}

SIXELSTATUS
sixel_palette_bin_histogram_add_soft_trilinear(
    sixel_palette_bin_histogram_t *histogram,
    double const sample[3],
    double const mapped[3],
    double source_weight,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    double coordinate[3];
    double fraction[3];
    unsigned int low[3];
    unsigned int high[3];
    double axis_weight[3][2];
    unsigned int first;
    unsigned int second;
    unsigned int third;
    unsigned int index0;
    unsigned int index1;
    unsigned int index2;
    double weight;
    double low_as_double;
    uint32_t key;

    status = SIXEL_OK;
    memset(coordinate, 0, sizeof(coordinate));
    memset(fraction, 0, sizeof(fraction));
    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    memset(axis_weight, 0, sizeof(axis_weight));
    first = 0u;
    second = 0u;
    third = 0u;
    index0 = 0u;
    index1 = 0u;
    index2 = 0u;
    weight = 0.0;
    low_as_double = 0.0;
    key = 0u;
    if (histogram == NULL || sample == NULL || mapped == NULL ||
            allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (source_weight <= 0.0) {
        return SIXEL_OK;
    }

    for (first = 0u; first < 3u; ++first) {
        coordinate[first] = mapped[first] *
            (double)(histogram->bin_count - 1u);
        if (coordinate[first] < 0.0) {
            coordinate[first] = 0.0;
        }
        if (coordinate[first] >
                (double)(histogram->bin_count - 1u)) {
            coordinate[first] =
                (double)(histogram->bin_count - 1u);
        }
        low_as_double = floor(coordinate[first]);
        if (low_as_double < 0.0) {
            low_as_double = 0.0;
        }
        if (low_as_double > (double)(histogram->bin_count - 1u)) {
            low_as_double = (double)(histogram->bin_count - 1u);
        }
        low[first] = (unsigned int)low_as_double;
        high[first] = low[first];
        if (high[first] + 1u < histogram->bin_count) {
            high[first] += 1u;
        }
        fraction[first] = coordinate[first] - (double)low[first];
        if (high[first] == low[first]) {
            fraction[first] = 0.0;
        }
        axis_weight[first][0] = 1.0 - fraction[first];
        axis_weight[first][1] = fraction[first];
    }

    for (first = 0u; first < 2u; ++first) {
        for (second = 0u; second < 2u; ++second) {
            for (third = 0u; third < 2u; ++third) {
                index0 = first == 0u ? low[0] : high[0];
                index1 = second == 0u ? low[1] : high[1];
                index2 = third == 0u ? low[2] : high[2];
                weight = source_weight * axis_weight[0][first] *
                    axis_weight[1][second] * axis_weight[2][third];
                if (weight <= 0.0) {
                    continue;
                }
                key = sixel_palette_bin_pack_key(
                    index0,
                    index1,
                    index2,
                    histogram->bits_per_axis);
                status = sixel_palette_bin_histogram_add(histogram,
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
