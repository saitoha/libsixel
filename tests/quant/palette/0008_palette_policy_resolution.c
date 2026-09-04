/*
 * SPDX-License-Identifier: MIT
 *
 * Palette policy lifecycle test.  A request origin remains stable while the
 * effective value advances through resolution and execution.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

static int
palette_resolution_lifecycle_is_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_frame_state_t first;
    sixel_palette_frame_state_t second;

    status = SIXEL_FALSE;
    sixel_palette_frame_state_init(&first);
    sixel_palette_frame_state_init(&second);

    sixel_palette_policy_resolution_init(
        &first.quantizer,
        0,
        SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
    status = sixel_palette_policy_resolve(
        &first.quantizer,
        2,
        SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA);
    if (SIXEL_FAILED(status) ||
            first.quantizer.requested != 0 ||
            first.quantizer.effective != 2 ||
            first.quantizer.origin != SIXEL_PALETTE_POLICY_ORIGIN_AUTO ||
            first.quantizer.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
            first.quantizer.reason !=
                SIXEL_PALETTE_RESOLUTION_SAMPLE_METADATA) {
        return 0;
    }

    status = sixel_palette_policy_mark_executed(&first.quantizer);
    if (SIXEL_FAILED(status) ||
            first.quantizer.phase != SIXEL_PALETTE_POLICY_EXECUTED) {
        return 0;
    }
    status = sixel_palette_policy_resolve(
        &first.quantizer,
        3,
        SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE);
    if (status != SIXEL_LOGIC_ERROR || first.quantizer.effective != 2) {
        return 0;
    }

    sixel_palette_policy_resolution_init(
        &second.quantizer,
        1,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_policy_mark_executed(&second.quantizer);
    if (status != SIXEL_LOGIC_ERROR) {
        return 0;
    }
    status = sixel_palette_policy_mark_bypassed(&second.sampling);
    if (SIXEL_FAILED(status) ||
            second.sampling.phase != SIXEL_PALETTE_POLICY_BYPASSED ||
            second.sampling.effective !=
                SIXEL_PALETTE_POLICY_VALUE_UNSET ||
            second.sampling.reason !=
                SIXEL_PALETTE_RESOLUTION_NOT_APPLICABLE) {
        return 0;
    }

    return 1;
}

int
test_palette_0008_palette_policy_resolution(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!palette_resolution_lifecycle_is_valid()) {
        fprintf(stderr, "palette policy lifecycle contract failed\n");
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
