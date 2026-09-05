/*
 * SPDX-License-Identifier: MIT
 *
 * Verify ownership and metadata contracts for the typed binning output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"
#include "src/weighted-point-set.h"

static int
weighted_point_set_contract_is_valid(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_palette_binning_state_t binning;
    sixel_weighted_point_set_t set;
    double borrowed_coordinates[6];
    double *owned_coordinates;
    double *owned_weights;

    status = SIXEL_FALSE;
    allocator = NULL;
    owned_coordinates = NULL;
    owned_weights = NULL;
    sixel_weighted_point_set_init(&set);
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_NONE,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_NONE,
        0u,
        SIXEL_PALETTE_BINNING_GRID_NONE,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_DIRECT,
        2u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_weighted_point_set_bind_borrowed(
        &set,
        borrowed_coordinates,
        NULL,
        1u,
        1.0,
        SIXEL_COLORSPACE_OKLAB,
        &binning);
    if (SIXEL_FAILED(status) ||
            set.ownership != SIXEL_WEIGHTED_POINT_BORROWED ||
            set.source_point_count != 2u || set.point_count != 1u ||
            set.total_weight != 1.0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_weighted_point_set_dispose(&set);
    status = sixel_weighted_point_set_bind_borrowed(
        &set,
        borrowed_coordinates,
        NULL,
        2u,
        2.0,
        SIXEL_COLORSPACE_OKLAB,
        &binning);
    if (SIXEL_FAILED(status) ||
            set.coordinates != borrowed_coordinates ||
            set.weights != NULL ||
            set.ownership != SIXEL_WEIGHTED_POINT_BORROWED ||
            set.policy != SIXEL_PALETTE_BINNING_NONE ||
            set.backend != SIXEL_PALETTE_BINNING_BACKEND_DIRECT ||
            set.source_point_count != 2u || set.point_count != 2u ||
            set.entry_capacity_bound != 2u ||
            set.total_weight != 2.0 ||
            set.colorspace != SIXEL_COLORSPACE_OKLAB) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_weighted_point_set_dispose(&set);
    if (set.ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            set.coordinates != NULL || set.allocator != NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    owned_coordinates = (double *)sixel_allocator_malloc(
        allocator, 4u * 3u * sizeof(double));
    owned_weights = (double *)sixel_allocator_malloc(
        allocator, 4u * sizeof(double));
    if (owned_coordinates == NULL || owned_weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    sixel_palette_binning_state_init(
        &binning,
        SIXEL_PALETTE_BINNING_AUTO,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_binning_resolve(
        &binning,
        SIXEL_PALETTE_BINNING_SOFT,
        4u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        2u,
        SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_weighted_point_set_take_owned(
        &set,
        &owned_coordinates,
        &owned_weights,
        17u,
        2.0,
        SIXEL_COLORSPACE_OKLAB,
        &binning,
        allocator);
    if (status != SIXEL_BAD_ARGUMENT || owned_coordinates == NULL ||
            owned_weights == NULL ||
            set.ownership != SIXEL_WEIGHTED_POINT_EMPTY) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = sixel_weighted_point_set_take_owned(
        &set,
        &owned_coordinates,
        &owned_weights,
        4u,
        2.0,
        SIXEL_COLORSPACE_OKLAB,
        &binning,
        allocator);
    if (SIXEL_FAILED(status) || owned_coordinates != NULL ||
            owned_weights != NULL ||
            set.ownership != SIXEL_WEIGHTED_POINT_OWNED ||
            set.policy != SIXEL_PALETTE_BINNING_SOFT ||
            set.bits_per_axis != 4u ||
            set.kernel != SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR ||
            set.backend !=
                SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE ||
            set.source_point_count != 2u || set.point_count != 4u ||
            set.entry_capacity_bound != 16u ||
            set.total_weight != 2.0 || set.allocator != allocator) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    /* The artifact must retain the allocator after the caller releases it. */
    sixel_allocator_unref(allocator);
    allocator = NULL;
    sixel_weighted_point_set_dispose(&set);
    status = SIXEL_OK;

cleanup:
    sixel_weighted_point_set_dispose(&set);
    if (owned_coordinates != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, owned_coordinates);
    }
    if (owned_weights != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, owned_weights);
    }
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

int
test_palette_0031_weighted_point_set(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!weighted_point_set_contract_is_valid()) {
        fprintf(stderr, "weighted point set contract failed\n");
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
