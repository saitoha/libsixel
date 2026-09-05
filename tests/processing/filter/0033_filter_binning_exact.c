/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that exact binning combines only numerically identical coordinates,
 * preserves their mass, and publishes the shared weighted-point artifact.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/filter-binning.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "src/palette-plan.h"
#include "src/weighted-point-set.h"

static double
find_exact_weight(sixel_weighted_point_set_t const *set,
                  double first,
                  double second,
                  double third)
{
    size_t index;

    index = 0u;
    if (set == NULL || set->coordinates == NULL || set->weights == NULL) {
        return -1.0;
    }
    for (index = 0u; index < set->point_count; ++index) {
        if (set->coordinates[index * 3u + 0u] == first &&
                set->coordinates[index * 3u + 1u] == second &&
                set->coordinates[index * 3u + 2u] == third) {
            return set->weights[index];
        }
    }
    return -1.0;
}

int
test_filter_0033_filter_binning_exact(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_palette_binning_state_t raw_state;
    sixel_palette_binning_state_t binning;
    sixel_palette_binning_state_t repeat_binning;
    sixel_weighted_point_set_t raw_points;
    sixel_weighted_point_set_t output;
    sixel_weighted_point_set_t repeat_output;
    sixel_filter_binning_config_t config;
    sixel_filter_t *filter;
    sixel_filter_t *repeat_filter;
    double coordinates[291];
    double weights[97];
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
        SIXEL_PALETTE_BINNING_EXACT,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_palette_binning_state_init(
        &repeat_binning,
        SIXEL_PALETTE_BINNING_EXACT,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    sixel_weighted_point_set_init(&raw_points);
    sixel_weighted_point_set_init(&output);
    sixel_weighted_point_set_init(&repeat_output);
    memset(&config, 0, sizeof(config));
    filter = NULL;
    repeat_filter = NULL;
    index = 0u;
    for (index = 0u; index < 97u; ++index) {
        coordinates[index * 3u + 0u] = 1000.0 + (double)index;
        coordinates[index * 3u + 1u] = 2000.0 + (double)index;
        coordinates[index * 3u + 2u] = 3000.0 + (double)index;
        weights[index] = 1.0;
    }
    coordinates[0] = 10.0;
    coordinates[1] = 20.0;
    coordinates[2] = 30.0;
    coordinates[3] = 10.0;
    coordinates[4] = 20.0;
    coordinates[5] = 30.0;
    coordinates[6] = -0.0;
    coordinates[7] = 40.0;
    coordinates[8] = 50.0;
    coordinates[9] = 0.0;
    coordinates[10] = 40.0;
    coordinates[11] = 50.0;
    coordinates[12] = 1.0;
    coordinates[13] = 60.0;
    coordinates[14] = 70.0;
    coordinates[15] = 1.0 + (double)FLT_EPSILON;
    coordinates[16] = 60.0;
    coordinates[17] = 70.0;

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
        97u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_weighted_point_set_bind_borrowed(
        &raw_points,
        coordinates,
        weights,
        97u,
        97.0,
        SIXEL_COLORSPACE_GAMMA,
        &raw_state);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_EXACT,
        0u,
        SIXEL_PALETTE_BINNING_GRID_NONE,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        97u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &binning;
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_BINNING,
        &config,
        &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_filter_bind_weighted_input(filter, &raw_points);
    sixel_filter_bind_weighted_output(
        filter,
        &output,
        SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(filter, allocator, NULL);
    if (status != SIXEL_LOGIC_ERROR ||
            output.ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            binning.policy.phase != SIXEL_PALETTE_POLICY_RESOLVED) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_filter_free(filter);
    filter = NULL;
    sixel_weighted_point_set_dispose(&raw_points);
    status = sixel_weighted_point_set_bind_borrowed(
        &raw_points,
        coordinates,
        NULL,
        97u,
        97.0,
        SIXEL_COLORSPACE_GAMMA,
        &raw_state);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_BINNING,
        &config,
        &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_filter_bind_weighted_input(filter, &raw_points);
    sixel_filter_bind_weighted_output(
        filter,
        &output,
        SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status) ||
            binning.policy.phase != SIXEL_PALETTE_POLICY_EXECUTED ||
            output.ownership != SIXEL_WEIGHTED_POINT_OWNED ||
            output.policy != SIXEL_PALETTE_BINNING_EXACT ||
            output.backend !=
                SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE ||
            output.source_point_count != 97u ||
            output.point_count != 95u ||
            output.entry_capacity_bound != 97u ||
            output.total_weight != 97.0 ||
            find_exact_weight(&output, 10.0, 20.0, 30.0) != 2.0 ||
            find_exact_weight(&output, 0.0, 40.0, 50.0) != 2.0 ||
            find_exact_weight(&output, 1.0, 60.0, 70.0) != 1.0 ||
            find_exact_weight(&output,
                              1.0 + (double)FLT_EPSILON,
                              60.0,
                              70.0) != 1.0 ||
            find_exact_weight(&output, 1096.0, 2096.0, 3096.0) !=
                1.0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = sixel_palette_binning_resolve(
        &repeat_binning,
        SIXEL_PALETTE_BINNING_EXACT,
        0u,
        SIXEL_PALETTE_BINNING_GRID_NONE,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        97u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    config.binning = &repeat_binning;
    status = sixel_filter_factory_create_by_kind(
        SIXEL_FILTER_KIND_BINNING,
        &config,
        &repeat_filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_filter_bind_weighted_input(repeat_filter, &raw_points);
    sixel_filter_bind_weighted_output(
        repeat_filter,
        &repeat_output,
        SIXEL_COLORSPACE_GAMMA);
    status = sixel_filter_run(repeat_filter, allocator, NULL);
    if (SIXEL_FAILED(status) ||
            repeat_binning.policy.phase !=
                SIXEL_PALETTE_POLICY_EXECUTED ||
            repeat_output.point_count != output.point_count ||
            memcmp(repeat_output.coordinates,
                   output.coordinates,
                   output.point_count * 3u * sizeof(double)) != 0 ||
            memcmp(repeat_output.weights,
                   output.weights,
                   output.point_count * sizeof(double)) != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = SIXEL_OK;

cleanup:
    sixel_filter_free(repeat_filter);
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&repeat_output);
    sixel_weighted_point_set_dispose(&output);
    sixel_weighted_point_set_dispose(&raw_points);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "exact binning filter contract failed\n");
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
