/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that every binning allocation failure leaves an empty output and a
 * resolved, unexecuted policy. This keeps filter failure atomic for callers.
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

static unsigned int binning_failure_allocation_count;
static unsigned int binning_failure_allocation_target;

static void *
binning_failure_malloc(size_t size)
{
    ++binning_failure_allocation_count;
    if (binning_failure_allocation_count ==
            binning_failure_allocation_target) {
        return NULL;
    }
    return malloc(size);
}

static void *
binning_failure_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void *
binning_failure_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void
binning_failure_free(void *ptr)
{
    free(ptr);
}

int
test_filter_0032_filter_binning_failure_output(int argc, char **argv)
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
    unsigned int failure_target;

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
    coordinates[0] = 16.0;
    coordinates[1] = 32.0;
    coordinates[2] = 64.0;
    failure_target = 0u;
    binning_failure_allocation_count = 0u;
    binning_failure_allocation_target = 0u;

    status = sixel_allocator_new(&allocator,
                                 binning_failure_malloc,
                                 binning_failure_calloc,
                                 binning_failure_realloc,
                                 binning_failure_free);
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

    for (failure_target = 1u; failure_target <= 3u; ++failure_target) {
        sixel_palette_binning_state_init(
            &binning,
            SIXEL_PALETTE_BINNING_HARD,
            SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
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
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        sixel_filter_bind_weighted_input(filter, &raw_points);
        sixel_filter_bind_weighted_output(
            filter,
            &output,
            SIXEL_COLORSPACE_GAMMA);
        binning_failure_allocation_count = 0u;
        binning_failure_allocation_target = failure_target;
        status = sixel_filter_run(filter, allocator, NULL);
        binning_failure_allocation_target = 0u;
        if (status != SIXEL_BAD_ALLOCATION ||
                output.ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
                output.coordinates != NULL || output.weights != NULL ||
                binning.policy.phase != SIXEL_PALETTE_POLICY_RESOLVED) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
        sixel_filter_free(filter);
        filter = NULL;
    }
    status = SIXEL_OK;

cleanup:
    binning_failure_allocation_target = 0u;
    sixel_filter_free(filter);
    sixel_weighted_point_set_dispose(&output);
    sixel_weighted_point_set_dispose(&raw_points);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "binning allocation failure was not atomic\n");
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
