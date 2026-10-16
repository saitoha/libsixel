/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that automatic sampling uses the stable default before
 * scheduler allocation consumes the effective policy.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

static int
sampling_auto_resolution_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_frame_state_t state;

    status = SIXEL_FALSE;
    sixel_palette_frame_state_init(&state);
    sixel_palette_policy_resolution_init(&state.sampling,
        SIXEL_PALETTE_SAMPLING_AUTO, SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_sampling_resolve_auto(&state);
    if (SIXEL_FAILED(status) ||
            state.sampling.requested != SIXEL_PALETTE_SAMPLING_AUTO ||
            state.sampling.effective != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            state.sampling.origin != SIXEL_PALETTE_POLICY_ORIGIN_AUTO ||
            state.sampling.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            state.sampling.reason !=
                SIXEL_PALETTE_RESOLUTION_SAMPLING_DEFAULT ||
            state.sampling_source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME) {
        return 0;
    }

    sixel_palette_frame_state_init(&state);
    sixel_palette_policy_resolution_init(
        &state.sampling,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_sampling_resolve_auto(&state);
    if (status != SIXEL_BAD_ARGUMENT ||
            state.sampling.phase != SIXEL_PALETTE_POLICY_UNRESOLVED) {
        return 0;
    }

    return 1;
}

int
test_palette_0026_sampling_auto(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!sampling_auto_resolution_valid()) {
        fprintf(stderr, "automatic sampling resolution failed\n");
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
