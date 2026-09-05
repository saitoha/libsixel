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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-binning.h"
#include "palette-bin-histogram.h"
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

    capacity = 8u;
    initial_entries = 0u;
    threshold = 0u;
    index = 0u;
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
    if ((size_t)capacity > SIZE_MAX / sizeof(*table->entries)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    table->entries =
        (sixel_filter_exact_entry_t *)sixel_allocator_malloc(
            allocator,
            (size_t)capacity * sizeof(*table->entries));
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

    grown = NULL;
    old_capacity = 0u;
    new_capacity = 0u;
    new_mask = 0u;
    index = 0u;
    slot = 0u;
    if (table == NULL || table->entries == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    old_capacity = table->capacity;
    if (old_capacity == 0u || old_capacity >= (1u << 30)) {
        return SIXEL_BAD_ALLOCATION;
    }
    new_capacity = old_capacity << 1u;
    new_mask = new_capacity - 1u;
    if ((size_t)new_capacity > SIZE_MAX / sizeof(*grown)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    grown = (sixel_filter_exact_entry_t *)sixel_allocator_malloc(
        allocator,
        (size_t)new_capacity * sizeof(*grown));
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
sixel_filter_exact_mix(uint64_t value)
{
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static uint64_t
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
        hash ^= sixel_filter_exact_mix(
            bits + (uint64_t)channel * UINT64_C(0x9e3779b97f4a7c15));
        hash = sixel_filter_exact_mix(hash);
    }
    return hash;
}

static int
sixel_filter_exact_coordinates_are_valid(double const coordinates[3])
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
            !sixel_filter_exact_coordinates_are_valid(coordinates)) {
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
sixel_filter_binning_publish(
    sixel_palette_bin_histogram_t const *histogram,
    sixel_weighted_point_set_t *output,
    sixel_palette_binning_state_t *binning,
    double total_weight,
    int colorspace,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int index;
    unsigned int channel;
    size_t output_index;
    double *coordinates;
    double *weights;
    sixel_palette_bin_entry_t const *entry;

    status = SIXEL_FALSE;
    index = 0u;
    channel = 0u;
    output_index = 0u;
    coordinates = NULL;
    weights = NULL;
    entry = NULL;
    if (histogram == NULL || output == NULL || binning == NULL ||
            allocator == NULL || histogram->size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)histogram->size > SIZE_MAX / 3u / sizeof(double)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    coordinates = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)histogram->size * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)histogram->size * sizeof(double));
    if (coordinates == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

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

    status = SIXEL_FALSE;
    index = 0u;
    output_index = 0u;
    coordinates = NULL;
    weights = NULL;
    entry = NULL;
    if (table == NULL || output == NULL || binning == NULL ||
            allocator == NULL || table->size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)table->size > SIZE_MAX / 3u / sizeof(double)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    coordinates = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)table->size * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)table->size * sizeof(double));
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
    sixel_weighted_point_set_t *output;
    sixel_palette_bin_histogram_t histogram;
    sixel_filter_exact_table_t exact_table;
    size_t expected_entries;
    int completed;

    status = SIXEL_FALSE;
    state = NULL;
    binning = NULL;
    policy = SIXEL_PALETTE_BINNING_AUTO;
    input = NULL;
    output = NULL;
    sixel_palette_bin_histogram_clear(&histogram);
    sixel_filter_exact_table_clear(&exact_table);
    expected_entries = 0u;
    completed = 0;
    (void)logger;
    if (filter == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    state = (sixel_filter_binning_state_t *)filter->userdata;
    input = filter->input.weighted_points;
    output = filter->output.weighted_points;
    if (state == NULL || input == NULL || output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    binning = state->config.binning;
    if (binning == NULL ||
            binning->policy.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            input->ownership == SIXEL_WEIGHTED_POINT_EMPTY ||
            input->policy != SIXEL_PALETTE_BINNING_NONE ||
            input->weights != NULL ||
            output->ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            filter->output.colorspace != input->colorspace) {
        return SIXEL_LOGIC_ERROR;
    }
    policy = (sixel_palette_binning_policy_t)binning->policy.effective;
    if ((policy != SIXEL_PALETTE_BINNING_EXACT &&
         policy != SIXEL_PALETTE_BINNING_HARD &&
         policy != SIXEL_PALETTE_BINNING_SOFT) ||
            binning->backend !=
                SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE ||
            binning->source_point_count != input->point_count) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (policy == SIXEL_PALETTE_BINNING_EXACT) {
        status = sixel_filter_exact_table_prepare(&exact_table,
                                                  input->point_count,
                                                  allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_filter_binning_add_exact(&exact_table,
                                                input,
                                                allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_filter_binning_publish_exact(
            &exact_table,
            output,
            binning,
            input->total_weight,
            input->colorspace,
            allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        goto progress;
    }

    expected_entries = input->point_count;
    if (policy == SIXEL_PALETTE_BINNING_SOFT) {
        if (expected_entries > SIZE_MAX / 8u) {
            expected_entries = SIZE_MAX / 8u;
        }
        expected_entries *= 8u;
    }
    status = sixel_palette_bin_histogram_init(
        &histogram,
        expected_entries,
        binning->bits_per_axis,
        allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_filter_binning_add_samples(&histogram,
                                              input,
                                              &state->config,
                                              allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_filter_binning_publish(&histogram,
                                          output,
                                          binning,
                                          input->total_weight,
                                          input->colorspace,
                                          allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

progress:
    completed = input->point_count > (size_t)INT_MAX
        ? INT_MAX : (int)input->point_count;
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
    if (filter == NULL || config == NULL || config->binning == NULL) {
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
