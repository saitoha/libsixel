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
#include "loader-common.h"
#include "palette-bin-histogram.h"
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
sixel_filter_binning_get_stream_view(
    sixel_sample_stream_t const *stream,
    sixel_frame_pixels_view_t *view,
    sixel_frame_transparency_t *transparency,
    unsigned int *channels_out,
    int *input_is_float32_out)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;
    int depth;
    unsigned int channels;
    int input_is_float32;

    status = SIXEL_BAD_ARGUMENT;
    frame_if = NULL;
    depth = 0;
    channels = 0u;
    input_is_float32 = 0;
    if (stream == NULL || view == NULL || transparency == NULL ||
            channels_out == NULL || input_is_float32_out == NULL ||
            stream->storage == SIXEL_SAMPLE_STREAM_EMPTY) {
        return status;
    }
    memset(view, 0, sizeof(*view));
    memset(transparency, 0, sizeof(*transparency));
    if (stream->storage == SIXEL_SAMPLE_STREAM_BORROWED_BUFFER) {
        if (stream->buffer == NULL || stream->buffer_size == 0u ||
                stream->point_count == 0u) {
            return status;
        }
        view->pixelformat = stream->pixelformat;
        view->colorspace = stream->colorspace;
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(stream->pixelformat)) {
            view->pixels_float32 = (float *)(void *)stream->buffer;
        } else {
            view->pixels = (unsigned char *)(void *)stream->buffer;
        }
        *transparency = stream->transparency;
    } else {
        if (stream->frame == NULL) {
            return status;
        }
        frame_if = sixel_frame_as_interface(stream->frame);
        if (frame_if == NULL || frame_if->vtbl == NULL ||
                frame_if->vtbl->get_pixels == NULL ||
                frame_if->vtbl->get_transparency == NULL) {
            return status;
        }
        status = frame_if->vtbl->get_pixels(frame_if, view);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (view->width != stream->width ||
                view->height != stream->height ||
                view->pixelformat != stream->pixelformat ||
                view->colorspace != stream->colorspace) {
            return SIXEL_LOGIC_ERROR;
        }
        status = frame_if->vtbl->get_transparency(frame_if, transparency);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if ((transparency->transparent_mask == NULL &&
         transparency->transparent_mask_size != 0u) ||
            (transparency->transparent_mask != NULL &&
             transparency->transparent_mask_size < stream->point_count)) {
        return SIXEL_BAD_INPUT;
    }
    depth = sixel_helper_compute_depth(view->pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    input_is_float32 = SIXEL_PIXELFORMAT_IS_FLOAT32(view->pixelformat);
    if (input_is_float32) {
        if (depth != 3 * (int)sizeof(float) ||
                view->pixels_float32 == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = 3u;
    } else {
        if (view->pixels == NULL ||
                (view->pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
                 view->pixelformat != SIXEL_PIXELFORMAT_RGBA8888)) {
            return SIXEL_BAD_INPUT;
        }
        channels = (unsigned int)depth;
    }
    if (channels != 3u && channels != 4u) {
        return SIXEL_BAD_ARGUMENT;
    }
    *channels_out = channels;
    *input_is_float32_out = input_is_float32;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_binning_read_stream_sample(
    sixel_frame_pixels_view_t const *view,
    sixel_frame_transparency_t const *transparency,
    unsigned int channels,
    int input_is_float32,
    size_t index,
    sixel_filter_binning_config_t const *config,
    double sample[3],
    int *visible_out)
{
    unsigned int channel;

    channel = 0u;
    if (view == NULL || transparency == NULL || config == NULL ||
            sample == NULL || visible_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *visible_out = 0;
    if (transparency->transparent_mask != NULL &&
            transparency->transparent_mask[index] != 0u) {
        return SIXEL_OK;
    }
    if (channels == 4u &&
            transparency->alpha_zero_is_transparent != 0 &&
            view->pixels[index * channels + 3u] == 0u) {
        return SIXEL_OK;
    }
    for (channel = 0u; channel < 3u; ++channel) {
        if (input_is_float32) {
            sample[channel] = (double)
                view->pixels_float32[index * channels + channel];
            if (!(sample[channel] <= DBL_MAX &&
                  sample[channel] >= -DBL_MAX)) {
                return SIXEL_BAD_INPUT;
            }
            if (config->coordinate_mode ==
                    SIXEL_FILTER_BINNING_COORDINATE_CLAMPED) {
                sample[channel] = (double)
                    sixel_pixelformat_float_channel_clamp(
                        view->pixelformat,
                        (int)channel,
                        (float)sample[channel]);
            }
        } else {
            sample[channel] = (double)
                view->pixels[index * channels + channel];
        }
    }
    if (!sixel_filter_binning_coordinates_are_valid(sample)) {
        return SIXEL_BAD_INPUT;
    }
    *visible_out = 1;
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
    status = sixel_filter_binning_get_stream_view(stream,
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

    for (index = 0u; index < stream->point_count; ++index) {
        status = sixel_filter_binning_read_stream_sample(
            &view,
            &transparency,
            channels,
            input_is_float32,
            index,
            config,
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
    status = sixel_filter_binning_get_stream_view(stream,
                                                   &view,
                                                   &transparency,
                                                   &channels,
                                                   &input_is_float32);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (index = 0u; index < stream->point_count; ++index) {
        status = sixel_filter_binning_read_stream_sample(
            &view,
            &transparency,
            channels,
            input_is_float32,
            index,
            config,
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
    status = sixel_filter_binning_get_stream_view(stream,
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
        status = sixel_filter_binning_read_stream_sample(
            &view,
            &transparency,
            channels,
            input_is_float32,
            index,
            config,
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
                 SIXEL_FILTER_BINNING_OUTPUT_BIN_KEY_ASCENDING) ||
            (state->config.coordinate_mode !=
                 SIXEL_FILTER_BINNING_COORDINATE_SOURCE &&
             state->config.coordinate_mode !=
                 SIXEL_FILTER_BINNING_COORDINATE_CLAMPED)) {
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
        "LSXBSTAT1|bits=%u|source_points=%zu|effective_points=%zu",
        binning->bits_per_axis,
        source_point_count,
        output->point_count);
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
    if (filter == NULL || config == NULL || config->binning == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((config->output_order != SIXEL_FILTER_BINNING_OUTPUT_NATIVE &&
         config->output_order !=
             SIXEL_FILTER_BINNING_OUTPUT_BIN_KEY_ASCENDING) ||
            (config->coordinate_mode !=
                 SIXEL_FILTER_BINNING_COORDINATE_SOURCE &&
             config->coordinate_mode !=
                 SIXEL_FILTER_BINNING_COORDINATE_CLAMPED)) {
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
