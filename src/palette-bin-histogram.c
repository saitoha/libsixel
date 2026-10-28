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
#include "options.h"
#include "loader-common.h"
#include "palette-common-snap.h"

#if defined(_MSC_VER)
# define SIXEL_RANGE_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_RANGE_TLS _Thread_local
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_RANGE_TLS __thread
#else
/* The encoder's quantizer override lock serializes non-TLS workers. */
# define SIXEL_RANGE_TLS
#endif

static SIXEL_RANGE_TLS int range_strength_override;
static SIXEL_RANGE_TLS unsigned int range_strength_value;
#undef SIXEL_RANGE_TLS

void
sixel_palette_bin_range_set_strength(int enabled, unsigned int strength)
{
    range_strength_override = enabled;
    range_strength_value = strength > 100u ? 100u : strength;
}

unsigned int
sixel_palette_bin_range_get_strength(void)
{
    unsigned int strength;

    if (range_strength_override) {
        return range_strength_value;
    }
    strength = 100u;
    sixel_option_resolve_registered_uint_binding(
        SIXEL_OPTION_SCHEMA_BINNING_POLICY, "auto",
        SIXEL_SUBOPTION_BINDING_ID_2(
            palette_range_strength, palette_range_strength_override),
        &strength);
    return strength;
}


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

SIXELSTATUS
sixel_palette_bin_expand_bounds(
    void const *samples,
    unsigned int sample_count,
    int input_is_float32,
    double scale[3],
    double offset[3])
{
    double lower;
    double upper;
    double minimum;
    double maximum;
    double value;
    unsigned int channel;
    unsigned int index;

    if (samples == NULL || scale == NULL || offset == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* These are addressing bounds, not clustering coordinates. Start with
     * the nominal interval so in-range jobs retain their established grid.
     * Extend it only where required: saturating the address would merge
     * distinct wide-gamut colors into the same edge cell at every bin depth.
     * Binary32 source extrema and their differences fit safely in double. */
    for (channel = 0u; channel < 3u; ++channel) {
        if (!isfinite(scale[channel]) || scale[channel] <= 0.0 ||
                !isfinite(offset[channel])) {
            return SIXEL_BAD_ARGUMENT;
        }
        lower = -offset[channel] / scale[channel];
        upper = lower + 255.0 / scale[channel];
        minimum = lower;
        maximum = upper;
        for (index = 0u; index < sample_count; ++index) {
            value = input_is_float32
                ? ((float const *)samples)[(size_t)index * 3u + channel]
                : ((double const *)samples)[(size_t)index * 3u + channel];
            if (!isfinite(value)) {
                return SIXEL_BAD_INPUT;
            }
            if (value < minimum) {
                minimum = value;
            }
            if (value > maximum) {
                maximum = value;
            }
        }
        if (minimum < lower || maximum > upper) {
            scale[channel] = 255.0 / (maximum - minimum);
            offset[channel] = -minimum * scale[channel];
        }
    }
    return SIXEL_OK;
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

void
sixel_palette_bin_range_init(sixel_palette_bin_range_t *range,
                             unsigned int levels)
{
    memset(range, 0, sizeof(*range));
    range->levels = levels;
    range->axis_weight[0] = 100u;
    range->axis_weight[1] = 100u;
    range->axis_weight[2] = 100u;
}

void
sixel_palette_bin_range_observe(sixel_palette_bin_range_t *range,
                                unsigned int channel,
                                unsigned int bin,
                                double value)
{
    sixel_palette_bin_range_cell_t *cell;

    cell = &range->cells[channel][bin];
    if (cell->count == 0.0) {
        cell->minimum = value;
        cell->maximum = value;
    } else {
        if (value < cell->minimum) {
            cell->minimum = value;
        }
        if (value > cell->maximum) {
            cell->maximum = value;
        }
    }
    cell->count += 1.0;
    cell->sum += value;
    cell->square_sum += value * value;
}

/* Keep the gate and marginal reduction on exactly the consumer's grid,
 * including Heckbert's rounded LUT and medoids' byte-scaled float map. */
static unsigned int
sixel_palette_bin_nominal_index(double *value, unsigned int axis,
    unsigned int levels, int byte_coordinates, double const scale[3],
    double const offset[3], sixel_palette_binning_grid_map_t grid_map,
    int raw_coordinates, unsigned int const *byte_lut)
{
    double mapped;
    unsigned int bin;

    if (byte_lut != NULL && byte_coordinates) {
        bin = byte_lut[(unsigned int)*value];
    } else if (raw_coordinates) {
        mapped = *value;
        if (!byte_coordinates && scale != NULL && offset != NULL &&
                scale[axis] > 0.0) {
            mapped = *value * scale[axis] + offset[axis];
        }
        if (!isfinite(mapped) || mapped < 0.0) {
            mapped = 0.0;
        } else if (mapped > 255.0) {
            mapped = 255.0;
        }
        bin = (unsigned int)mapped * levels / 256u;
    } else {
        mapped = sixel_palette_bin_map_sample_to_unit(*value,
            !byte_coordinates, axis, scale, offset, grid_map);
        bin = (unsigned int)(mapped * levels);
        if (!byte_coordinates) {
            *value = mapped;
        }
    }
    return bin >= levels ? levels - 1u : bin;
}

static SIXELSTATUS
sixel_palette_bin_range_count(sixel_sample_statistics_t const *statistics,
    sixel_sample_stream_t const *stream,
    sixel_weighted_point_set_t const *points,
    unsigned int levels, unsigned int threshold,
    double const scale[3], double const offset[3],
    sixel_palette_binning_grid_map_t grid_map, int raw_coordinates,
    unsigned int const *byte_lut, sixel_palette_bin_range_t const *range,
    unsigned int *occupied_out, int *complete, unsigned int *recoverable)
{
    uint32_t keys[SIXEL_SAMPLE_SAFE_LIMIT * 4];
    uint64_t safe_pairs[SIXEL_SAMPLE_SAFE_LIMIT * 2];
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    SIXELSTATUS status;
    double sample[3];
    size_t index;
    size_t count;
    size_t visible_index;
    unsigned int channels;
    unsigned int axis;
    unsigned int occupied;
    unsigned int bin;
    unsigned int slot;
    uint32_t key;
    uint32_t safe_key;
    uint64_t pair;
    unsigned int safe_slot;
    int floating;
    int visible;

    if (statistics == NULL || occupied_out == NULL || levels == 0u ||
            levels > 256u || threshold > SIXEL_SAMPLE_SAFE_LIMIT + 1u ||
            (stream == NULL) == (points == NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    occupied = 0u;
    index = 0u;
    visible_index = 0u;
    if (recoverable != NULL) {
        *recoverable = 0u;
        memset(safe_pairs, 0xff, sizeof(safe_pairs));
    }
    count = points != NULL ? points->point_count : stream->point_count;
    if (threshold <= 1u) {
        goto report;
    }
    channels = 0u;
    floating = 0;
    if (stream != NULL) {
        status = sixel_sample_stream_get_view(stream, &view,
            &transparency, &channels, &floating);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    memset(keys, 0xff, sizeof(keys));
    for (index = 0u; index < count; ++index) {
        if (points != NULL) {
            memcpy(sample, points->coordinates + index * 3u,
                sizeof(sample));
        } else {
            status = sixel_sample_stream_read(&view, &transparency,
                channels, floating, index, sample, &visible);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (!visible) {
                continue;
            }
        }
        key = 0u;
        safe_key = 0u;
        if (recoverable != NULL) {
            if (statistics->byte_coordinates) {
                for (axis = 0u; axis < 3u; ++axis) {
                    safe_key = (safe_key << 8u) |
                        sixel_safe_tones[(unsigned int)sample[axis]];
                }
            } else {
                safe_key = statistics->safe_keys[visible_index];
            }
        }
        for (axis = 0u; axis < 3u; ++axis) {
            bin = sixel_palette_bin_nominal_index(
                &sample[axis], axis, levels, statistics->byte_coordinates,
                scale, offset, grid_map, raw_coordinates, byte_lut);
            if (range != NULL) {
                bin = sixel_palette_bin_range_sample_index(range, axis,
                    bin, sample[axis], visible_index);
            }
            key = key * levels + bin;
        }
        ++visible_index;
        /* Heckbert includes alpha in its rounded byte histogram. Other
         * consumers exclude hidden samples and aggregate RGB alone. */
        if (byte_lut != NULL && channels == 4u && !floating) {
            key = key * levels + byte_lut[view.pixels[index * 4u + 3u]];
        }
        /* A safe color may straddle nominal cells which must remain
         * separate. Count distinguishable colors within each original
         * cell, not just globally, so the budget cannot discard recoverable
         * tones merely to pay for those protected existing boundaries. */
        if (recoverable != NULL && *recoverable < SIXEL_SAMPLE_SAFE_LIMIT) {
            pair = ((uint64_t)key << 24u) | safe_key;
            safe_slot = sixel_palette_bin_hash_u32(
                (uint32_t)pair ^ (uint32_t)(pair >> 32u)) &
                (SIXEL_SAMPLE_SAFE_LIMIT * 2u - 1u);
            while (safe_pairs[safe_slot] != UINT64_MAX &&
                    safe_pairs[safe_slot] != pair) {
                safe_slot = (safe_slot + 1u) &
                    (SIXEL_SAMPLE_SAFE_LIMIT * 2u - 1u);
            }
            if (safe_pairs[safe_slot] == UINT64_MAX) {
                safe_pairs[safe_slot] = pair;
                ++*recoverable;
            }
        }
        slot = sixel_palette_bin_hash_u32(key) &
            (SIXEL_SAMPLE_SAFE_LIMIT * 4u - 1u);
        while (keys[slot] != UINT32_MAX && keys[slot] != key) {
            slot = (slot + 1u) & (SIXEL_SAMPLE_SAFE_LIMIT * 4u - 1u);
        }
        if (keys[slot] == UINT32_MAX) {
            keys[slot] = key;
            if (++occupied >= threshold) {
                break;
            }
        }
    }
report:
    *occupied_out = occupied;
    *complete = index == count;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_bin_range_gate_count(
    sixel_sample_statistics_t const *statistics,
    sixel_sample_stream_t const *stream,
    sixel_weighted_point_set_t const *points,
    unsigned int levels, unsigned int requested_colors,
    double const scale[3], double const offset[3],
    sixel_palette_binning_grid_map_t grid_map, int raw_coordinates,
    unsigned int const *byte_lut, int *enabled, unsigned int *occupied,
    unsigned int *recoverable)
{
    SIXELSTATUS status;
    unsigned int threshold;
    int complete;

    if (statistics == NULL || enabled == NULL || requested_colors == 0u ||
            requested_colors > SIXEL_PALETTE_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    *enabled = 0;
    threshold = requested_colors < statistics->safe_color_count
        ? requested_colors : statistics->safe_color_count;
    status = sixel_palette_bin_range_count(statistics, stream, points,
        levels, threshold, scale, offset, grid_map, raw_coordinates,
        byte_lut, NULL, occupied, &complete, recoverable);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    *enabled = *occupied != 0u && *occupied < threshold;
    sixel_trace_topic_message("palette_contract",
        "LSXRANGE1|requested=%u|safe=%u|hard=%u|complete=%d|eligible=%d",
        requested_colors, statistics->safe_color_count, *occupied,
        complete, *enabled);
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_bin_range_gate(sixel_sample_statistics_t const *statistics,
    sixel_sample_stream_t const *stream,
    sixel_weighted_point_set_t const *points,
    unsigned int levels, unsigned int requested_colors,
    double const scale[3], double const offset[3],
    sixel_palette_binning_grid_map_t grid_map, int raw_coordinates,
    unsigned int const *byte_lut, int *enabled)
{
    unsigned int occupied;

    return sixel_palette_bin_range_gate_count(statistics, stream, points,
        levels, requested_colors, scale, offset, grid_map, raw_coordinates,
        byte_lut, enabled, &occupied, NULL);
}

SIXELSTATUS
sixel_palette_bin_range_from_statistics(sixel_palette_bin_range_t *range,
    sixel_sample_statistics_t const *statistics, double const scale[3],
    double const offset[3], sixel_palette_binning_grid_map_t grid_map,
    int raw_coordinates, unsigned int const *byte_lut)
{
    sixel_palette_bin_range_cell_t *cell;
    size_t index;
    size_t limit;
    size_t count;
    unsigned int axis;
    unsigned int bin;
    double value;
    double weight;

    if (range == NULL || statistics == NULL || range->levels == 0u ||
            range->levels > 256u) {
        return SIXEL_BAD_ARGUMENT;
    }
    for (axis = 0u; axis < 3u; ++axis) {
        limit = statistics->byte_coordinates ? 256u
            : statistics->capacities[axis];
        for (index = 0u; index < limit; ++index) {
            count = statistics->byte_coordinates
                ? statistics->byte_counts[axis][index]
                : statistics->axes[axis][index].count;
            if (count == 0u) {
                continue;
            }
            value = statistics->byte_coordinates ? (double)index
                : statistics->axes[axis][index].value;
            bin = sixel_palette_bin_nominal_index(&value, axis,
                range->levels, statistics->byte_coordinates, scale,
                offset, grid_map, raw_coordinates, byte_lut);
            cell = &range->cells[axis][bin];
            if (cell->count == 0.0 || value < cell->minimum) {
                cell->minimum = value;
            }
            if (cell->count == 0.0 || value > cell->maximum) {
                cell->maximum = value;
            }
            weight = (double)count;
            cell->count += weight;
            cell->sum += value * weight;
            cell->square_sum += value * value * weight;
        }
    }
    return SIXEL_OK;
}

void
sixel_palette_bin_range_finish(sixel_palette_bin_range_t *range,
                               int byte_coordinates, unsigned int strength)
{
    sixel_palette_bin_range_cell_t *cell;
    double error[256];
    double score;
    double best_score;
    double span;
    unsigned int channel;
    unsigned int bin;
    unsigned int occupied;
    unsigned int budget;
    unsigned int used;
    unsigned int best;
    unsigned int first;
    unsigned int axis_strength;

    if (strength > 100u) {
        strength = 100u;
    }
    for (channel = 0u; channel < 3u; ++channel) {
        range->active[channel] = 0;
        axis_strength = strength * range->axis_weight[channel] / 100u;
        occupied = 0u;
        for (bin = 0u; bin < range->levels; ++bin) {
            cell = &range->cells[channel][bin];
            cell->first = bin;
            cell->divisions = cell->count != 0.0 ? 1u : 0u;
            occupied += cell->divisions;
            error[bin] = cell->count != 0.0
                ? cell->square_sum - cell->sum * cell->sum / cell->count
                : 0.0;
        }
        used = occupied;
        /* Strength spends a fraction of unused nominal addresses. Zero
         * keeps the original grid; 100 preserves full refinement. */
        budget = occupied +
            (range->levels - occupied) * axis_strength / 100u;
        /* Approximate residual error decreases quadratically with interval
         * subdivision. Spend each spare address on the largest remaining
         * weighted error. Constants need no subdivisions; byte intervals
         * cannot benefit from more addresses than their integer span. */
        while (used < budget) {
            best = range->levels;
            best_score = 0.0;
            for (bin = 0u; bin < range->levels; ++bin) {
                cell = &range->cells[channel][bin];
                span = cell->maximum - cell->minimum;
                if (cell->divisions == 0u || span <= 0.0 ||
                        (byte_coordinates &&
                         cell->divisions >= (unsigned int)span + 1u)) {
                    continue;
                }
                score = error[bin] /
                    ((double)cell->divisions * cell->divisions);
                if (score > best_score) {
                    best = bin;
                    best_score = score;
                }
            }
            if (best == range->levels) {
                break;
            }
            ++range->cells[channel][best].divisions;
            ++used;
        }
        if (used == occupied) {
            continue;
        }
        range->active[channel] = 1;
        first = 0u;
        for (bin = 0u; bin < range->levels; ++bin) {
            cell = &range->cells[channel][bin];
            cell->first = first;
            first += cell->divisions;
            span = cell->maximum - cell->minimum;
            if (cell->divisions > 1u && span > 0.0) {
                cell->scale = byte_coordinates
                    ? (double)cell->divisions / (span + 1.0)
                    : (double)cell->divisions / span;
            }
        }
    }
}

unsigned int
sixel_palette_bin_range_index(sixel_palette_bin_range_t const *range,
                              unsigned int channel,
                              unsigned int bin,
                              double value)
{
    sixel_palette_bin_range_cell_t const *cell;
    double position;
    unsigned int subdivision;

    if (!range->active[channel]) {
        return bin;
    }
    cell = &range->cells[channel][bin];
    /* Unobserved byte values can be queried while compiling a fast LUT,
     * but cannot occur in this frame's second pass. Keep their keys valid. */
    if (cell->divisions == 0u) {
        return bin;
    }
    if (cell->divisions <= 1u) {
        return cell->first;
    }
    position = (value - cell->minimum) * cell->scale;
    subdivision = position <= 0.0 ? 0u :
        position >= (double)cell->divisions ? cell->divisions - 1u :
        (unsigned int)position;
    return cell->first + subdivision;
}

unsigned int
sixel_palette_bin_range_sample_index(sixel_palette_bin_range_t const *range,
    unsigned int channel, unsigned int bin, double value,
    size_t visible_index)
{
    sixel_sample_statistics_t const *statistics;

    statistics = range->statistics;
    if (range->active[channel] && statistics != NULL) {
        if (statistics->byte_coordinates) {
            value = sixel_safe_tones[(unsigned int)value];
        } else {
            value = statistics->safe_coordinates[
                visible_index * 3u + channel];
            if (!range->raw_coordinates) {
                value = sixel_palette_bin_map_sample_to_unit(value, 1,
                    channel, range->scale, range->offset, range->grid_map);
            }
        }
    }
    return sixel_palette_bin_range_index(range, channel, bin, value);
}

SIXELSTATUS
sixel_palette_bin_range_prepare(sixel_palette_bin_range_t *range,
    sixel_sample_statistics_t const *statistics,
    sixel_sample_stream_t const *stream,
    sixel_weighted_point_set_t const *points,
    unsigned int requested_colors, double const scale[3],
    double const offset[3], sixel_palette_binning_grid_map_t grid_map,
    int raw_coordinates, unsigned int const *byte_lut,
    unsigned int strength, int model)
{
    SIXELSTATUS status;
    unsigned int hard;
    unsigned int count;
    unsigned int best_count;
    unsigned int factor;
    unsigned int recoverable;
    unsigned int budget;
    unsigned int lower;
    unsigned int upper;
    unsigned int trial;
    unsigned int probes;
    unsigned int axis;
    int enabled;
    int complete;

    if (range == NULL || statistics == NULL || strength > 100u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (strength == 0u || !statistics->safe_colors_valid ||
            (statistics->byte_coordinates &&
             statistics->colorspace != SIXEL_COLORSPACE_GAMMA)) {
        return SIXEL_OK;
    }
    status = sixel_palette_bin_range_gate_count(statistics, stream, points,
        range->levels, requested_colors, scale, offset, grid_map,
        raw_coordinates, byte_lut, &enabled, &hard, &recoverable);
    if (SIXEL_FAILED(status) || !enabled) {
        return status;
    }
    status = sixel_palette_bin_range_from_statistics(range, statistics,
        scale, offset, grid_map, raw_coordinates, byte_lut);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    range->statistics = statistics;
    range->grid_map = grid_map;
    range->raw_coordinates = raw_coordinates;
    for (axis = 0u; axis < 3u; ++axis) {
        range->scale[axis] = scale != NULL ? scale[axis] : 255.0;
        range->offset[axis] = offset != NULL ? offset[axis] : 0.0;
    }
    /* This policy is selected by the actual consumer, including AUTO's
     * resolved model. Extra candidates are evidence-bounded solver input,
     * not a promise of extra output colors or improved image quality.
     * Centroid and medoid fitting benefit from a modest surplus. Median-cut
     * and farthest-first selection retain the tighter bound: increasing
     * their candidate population can change splits or extreme choices
     * without improving both spatial structure and color fidelity. */
    factor = model == SIXEL_QUANTIZE_MODEL_KMEANS ||
        model == SIXEL_QUANTIZE_MODEL_KMEDOIDS ? 2u : 1u;
    budget = requested_colors * factor;
    if (budget > recoverable) {
        budget = recoverable;
    }
    budget = hard + (budget - hard) * strength / 100u;
    lower = 0u;
    upper = strength + 1u;
    trial = strength;
    probes = 0u;
    best_count = hard;
    /* Counts need not be monotone: changing an interval's divisions moves
     * its internal boundaries. Keep only verified feasible trials. The
     * bounded search selects the strongest tested feasible map, rather than
     * claiming a global optimum or relying on a monotonicity assumption.
     * A probe stops at budget+1, before aggregating or invoking a solver. */
    while (trial != 0u && upper > lower + 1u) {
        sixel_palette_bin_range_finish(range, statistics->byte_coordinates,
            trial);
        status = sixel_palette_bin_range_count(statistics, stream, points,
            range->levels, budget + 1u, scale, offset, grid_map,
            raw_coordinates, byte_lut, range, &count, &complete, NULL);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        ++probes;
        if (complete && count <= budget) {
            lower = trial;
            best_count = count;
        } else {
            upper = trial;
        }
        trial = lower + (upper - lower) / 2u;
    }
    sixel_palette_bin_range_finish(range, statistics->byte_coordinates,
        lower);
    sixel_trace_topic_message("palette_contract",
        "LSXRBUDGET1|model=%d|hard=%u|safe=%u|recoverable=%u|budget=%u|"
        "candidates=%u|"
        "strength=%u|effective=%u|probes=%u|axis0=%u|axis1=%u|axis2=%u",
        model, hard, statistics->safe_color_count, recoverable, budget,
        best_count,
        strength, lower, probes, range->axis_weight[0],
        range->axis_weight[1], range->axis_weight[2]);
    return SIXEL_OK;
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
