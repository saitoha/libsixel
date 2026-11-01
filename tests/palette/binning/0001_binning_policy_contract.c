/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that binning policy resolution keeps semantic choices separate from
 * storage and computes bounded contribution capacity from sample metadata.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

static int
binning_entry_bounds_are_valid(void)
{
    SIXELSTATUS status;
    size_t bound;

    status = SIXEL_FALSE;
    bound = 0u;
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_NONE,
        0u,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        257u,
        &bound);
    if (SIXEL_FAILED(status) || bound != 257u) {
        return 0;
    }
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_EXACT,
        0u,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        257u,
        &bound);
    if (SIXEL_FAILED(status) || bound != 257u) {
        return 0;
    }
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_HARD,
        4u,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        5000u,
        &bound);
    if (SIXEL_FAILED(status) || bound != 4096u) {
        return 0;
    }
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_SOFT,
        4u,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        100u,
        &bound);
    if (SIXEL_FAILED(status) || bound != 800u) {
        return 0;
    }
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_SOFT,
        4u,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        1000u,
        &bound);
    if (SIXEL_FAILED(status) || bound != 4096u) {
        return 0;
    }
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_SOFT,
        4u,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        100u,
        &bound);
    if (status != SIXEL_BAD_ARGUMENT || bound != 4096u) {
        return 0;
    }
    status = sixel_palette_binning_entry_bound(
        SIXEL_PALETTE_BINNING_SOFT,
        8u,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        SIZE_MAX,
        &bound);
    if (SIXEL_FAILED(status) || bound != 16777216u) {
        return 0;
    }
    return 1;
}

static int
binning_resolution_is_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_binning_state_t automatic;
    sixel_palette_binning_state_t explicit_policy;
    sixel_palette_binning_state_t bypassed;

    status = SIXEL_FALSE;
    sixel_palette_binning_state_init(
        &automatic,
        SIXEL_PALETTE_BINNING_AUTO,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_binning_resolve(
        &automatic,
        SIXEL_PALETTE_BINNING_SOFT,
        7u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE,
        1000u,
        SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA);
    if (SIXEL_FAILED(status) ||
            automatic.policy.requested != SIXEL_PALETTE_BINNING_AUTO ||
            automatic.policy.effective != SIXEL_PALETTE_BINNING_SOFT ||
            automatic.policy.origin != SIXEL_PALETTE_POLICY_ORIGIN_AUTO ||
            automatic.policy.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            automatic.policy.reason !=
                SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA ||
            automatic.bits_per_axis != 7u ||
            automatic.grid_map != SIXEL_PALETTE_BINNING_GRID_UNIFORM ||
            automatic.kernel !=
                SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR ||
            automatic.backend !=
                SIXEL_PALETTE_BINNING_BACKEND_COMPACT_SPARSE ||
            automatic.source_point_count != 1000u ||
            automatic.entry_capacity_bound != 8000u) {
        return 0;
    }

    sixel_palette_binning_state_init(
        &explicit_policy,
        SIXEL_PALETTE_BINNING_HARD,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_binning_resolve(
        &explicit_policy,
        SIXEL_PALETTE_BINNING_SOFT,
        6u,
        SIXEL_PALETTE_BINNING_GRID_UNIFORM,
        SIXEL_PALETTE_BINNING_KERNEL_TRILINEAR,
        SIXEL_PALETTE_BINNING_BACKEND_DENSE,
        100u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (status != SIXEL_LOGIC_ERROR ||
            explicit_policy.policy.phase !=
                SIXEL_PALETTE_POLICY_UNRESOLVED ||
            explicit_policy.bits_per_axis != 0u ||
            explicit_policy.entry_capacity_bound != 0u) {
        return 0;
    }
    status = sixel_palette_binning_resolve(
        &explicit_policy,
        SIXEL_PALETTE_BINNING_HARD,
        6u,
        SIXEL_PALETTE_BINNING_GRID_SRGB,
        SIXEL_PALETTE_BINNING_KERNEL_NONE,
        SIXEL_PALETTE_BINNING_BACKEND_DENSE,
        100u,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status) ||
            explicit_policy.policy.effective !=
                SIXEL_PALETTE_BINNING_HARD ||
            explicit_policy.entry_capacity_bound != 100u) {
        return 0;
    }

    sixel_palette_binning_state_init(
        &bypassed,
        SIXEL_PALETTE_BINNING_AUTO,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_binning_mark_bypassed(&bypassed);
    if (SIXEL_FAILED(status) ||
            bypassed.policy.phase != SIXEL_PALETTE_POLICY_BYPASSED ||
            bypassed.policy.reason !=
                SIXEL_PALETTE_RESOLUTION_NOT_APPLICABLE ||
            bypassed.backend !=
                SIXEL_PALETTE_BINNING_BACKEND_UNRESOLVED ||
            bypassed.entry_capacity_bound != 0u) {
        return 0;
    }
    return 1;
}

int
test_palette_0030_binning_policy(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!binning_entry_bounds_are_valid() ||
            !binning_resolution_is_valid()) {
        fprintf(stderr, "binning policy contract failed\n");
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
