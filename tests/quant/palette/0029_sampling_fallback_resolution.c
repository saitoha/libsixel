/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that automatic adaptive sampling can be re-resolved to the full
 * preprocessed frame, while explicit and unrelated states remain immutable.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

static int
sampling_fallback_resolution_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_frame_state_t resolved;
    sixel_palette_frame_state_t executed;
    sixel_palette_frame_state_t explicit_policy;

    status = SIXEL_FALSE;
    sixel_palette_frame_state_init(&resolved);
    sixel_palette_frame_state_init(&executed);
    sixel_palette_frame_state_init(&explicit_policy);

    sixel_palette_policy_resolution_init(
        &resolved.sampling,
        SIXEL_PALETTE_SAMPLING_AUTO,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_sampling_resolve(
        &resolved,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
        SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME,
        SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    status = sixel_palette_sampling_resolve_fallback(&resolved);
    if (SIXEL_FAILED(status) ||
            resolved.sampling.requested != SIXEL_PALETTE_SAMPLING_AUTO ||
            resolved.sampling.effective !=
                SIXEL_PALETTE_SAMPLING_FULL_FRAME ||
            resolved.sampling.origin != SIXEL_PALETTE_POLICY_ORIGIN_AUTO ||
            resolved.sampling.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            resolved.sampling.reason != SIXEL_PALETTE_RESOLUTION_FALLBACK ||
            resolved.sampling_source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME) {
        return 0;
    }

    sixel_palette_policy_resolution_init(
        &executed.sampling,
        SIXEL_PALETTE_SAMPLING_AUTO,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_sampling_resolve(
        &executed,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
        SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME,
        SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    status = sixel_palette_policy_mark_executed(&executed.sampling);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    status = sixel_palette_sampling_resolve_fallback(&executed);
    if (SIXEL_FAILED(status) ||
            executed.sampling.effective !=
                SIXEL_PALETTE_SAMPLING_FULL_FRAME ||
            executed.sampling.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            executed.sampling.reason != SIXEL_PALETTE_RESOLUTION_FALLBACK ||
            executed.sampling_source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME) {
        return 0;
    }

    sixel_palette_policy_resolution_init(
        &explicit_policy.sampling,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_sampling_resolve(
        &explicit_policy,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
        SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME,
        SIXEL_PALETTE_RESOLUTION_EXPLICIT);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    status = sixel_palette_sampling_resolve_fallback(&explicit_policy);
    if (status != SIXEL_LOGIC_ERROR ||
            explicit_policy.sampling.effective !=
                SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            explicit_policy.sampling.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            explicit_policy.sampling.reason !=
                SIXEL_PALETTE_RESOLUTION_EXPLICIT ||
            explicit_policy.sampling_source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME) {
        return 0;
    }

    return 1;
}

int
test_palette_0029_sampling_fallback(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!sampling_fallback_resolution_valid()) {
        fprintf(stderr, "sampling fallback resolution failed\n");
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
