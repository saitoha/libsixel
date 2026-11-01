/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that hard and soft binning run through the filter vtable and publish
 * a weighted-point artifact without changing the sample coordinates.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/filter-binning.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "src/palette-plan.h"
#include "src/weighted-point-set.h"

int
test_filter_0031_filter_binning(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_palette_binning_state_t raw_state;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t raw_points;
    sixel_weighted_point_set_t output;
    sixel_filter_binning_config_t config;
    sixel_filter_t *filter;
    double coordinates[3];
    double weight_sum;
    size_t index;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    sixel_palette_binning_state_init(
        &raw_state,
        SIXEL_PALETTE_BINNING_NONE,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_weighted_point_set_init(&raw_points);
    sixel_weighted_point_set_init(&output);
    memset(&config, 0, sizeof(config));
    filter = NULL;
    coordinates[0] = 127.5;
    coordinates[1] = 127.5;
    coordinates[2] = 127.5;
    weight_sum = 0.0;
    index = 0u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_binning_resolve(
        &raw_state,
        SIXEL_PALETTE_BINNING_NONE,
        0u,
        SIXEL_PALETTE_BINNING_GRID_NONE,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_DIRECT,
        1u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_weighted_point_set_bind_borrowed(
        &raw_points,
        coordinates,
        NULL,
        1u,
        1.0,
        SIXEL_COLORSPACE_GAMMA,
        &raw_state);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_HARD,
        4u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        1u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_BINNING,
        &config,
        &filter);
    if (SIXEL_FAILED(status) || filter == NULL ||
            filter->kind != SIXEL_FILTER_KIND_BINNING) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_bind_weighted_input(filter, &raw_points);
    sixel_filter_bind_weighted_output(
        filter,
        &output,
        SIXEL_COLORSPACE_OKLAB);
    status = sixel_filter_run(filter, allocator, NULL);
    if (status != SIXEL_LOGIC_ERROR ||
            output.ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            binning.policy.phase != SIXEL_PALETTE_POLICY_RESOLVED) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_bind_weighted_output(
        filter,
        &output,
        SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) ||
            binning.policy.phase != SIXEL_PALETTE_POLICY_EXECUTED ||
            output.ownership != SIXEL_WEIGHTED_POINT_OWNED ||
            output.policy != SIXEL_PALETTE_BINNING_HARD ||
            output.point_count != 1u || output.weights[0] != 1.0 ||
            output.coordinates[0] != 127.5 ||
            output.coordinates[1] != 127.5 ||
            output.coordinates[2] != 127.5) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_free(filter);
    filter = NULL;
    sixel_weighted_point_set_dispose(&output);

    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_SOFT,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_SOFT,
        4u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        1u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    status = sixel_filter_factory_create_by_name("binning",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status) || filter == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_bind_weighted_input(filter, &raw_points);
    sixel_filter_bind_weighted_output(
        filter,
        &output,
        SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) || output.point_count != 8u ||
            output.entry_capacity_bound != 8u ||
            output.policy != SIXEL_PALETTE_BINNING_SOFT) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    for (index = 0u; index < output.point_count; ++index) {
        if (output.weights[index] != 0.125 ||
                output.coordinates[index * 3u + 0u] != 127.5 ||
                output.coordinates[index * 3u + 1u] != 127.5 ||
                output.coordinates[index * 3u + 2u] != 127.5) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
        weight_sum += output.weights[index];
    }
    if (weight_sum != 1.0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&output);
    sixel_weighted_point_set_dispose(&raw_points);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "binning filter contract failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
