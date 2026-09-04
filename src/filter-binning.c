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
    weight = 0.0;
    if (histogram == NULL || input == NULL || config == NULL ||
            config->binning == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    policy = (sixel_palette_binning_policy_t)
        config->binning->policy.effective;

    for (index = 0u; index < input->point_count; ++index) {
        weight = input->weights == NULL ? 1.0 : input->weights[index];
        if (weight <= 0.0) {
            continue;
        }
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
    size_t expected_entries;
    int completed;

    status = SIXEL_FALSE;
    state = NULL;
    binning = NULL;
    policy = SIXEL_PALETTE_BINNING_AUTO;
    input = NULL;
    output = NULL;
    sixel_palette_bin_histogram_clear(&histogram);
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
            output->ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            filter->output.colorspace != input->colorspace) {
        return SIXEL_LOGIC_ERROR;
    }
    policy = (sixel_palette_binning_policy_t)binning->policy.effective;
    if ((policy != SIXEL_PALETTE_BINNING_HARD &&
         policy != SIXEL_PALETTE_BINNING_SOFT) ||
            binning->backend !=
                SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE ||
            binning->source_point_count != input->point_count) {
        return SIXEL_BAD_ARGUMENT;
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
