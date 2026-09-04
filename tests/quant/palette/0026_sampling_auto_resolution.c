/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that automatic sampling is resolved from a resource profile before
 * scheduler allocation consumes the effective policy.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/palette-plan.h"

typedef struct sampling_auto_case {
    int total_threads;
    int heavy_operations;
    int async_eligible;
    sixel_palette_sampling_policy_t expected_policy;
    sixel_palette_sampling_source_t expected_source;
} sampling_auto_case_t;

static sampling_auto_case_t const sampling_auto_cases[] = {
    { 1, 0, 1, SIXEL_PALETTE_SAMPLING_FULL_FRAME,
      SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME },
    { 2, 0, 1, SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
      SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME },
    { 3, 1, 1, SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
      SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME },
    { 2, 1, 1, SIXEL_PALETTE_SAMPLING_FULL_FRAME,
      SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME },
    { 8, 0, 0, SIXEL_PALETTE_SAMPLING_FULL_FRAME,
      SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME }
};

static int
sampling_auto_resolution_valid(void)
{
    SIXELSTATUS status;
    sixel_palette_frame_state_t state;
    sampling_auto_case_t const *test_case;
    size_t index;

    status = SIXEL_FALSE;
    test_case = NULL;
    index = 0u;

    for (index = 0u;
            index < sizeof(sampling_auto_cases) /
                sizeof(sampling_auto_cases[0]);
            ++index) {
        test_case = &sampling_auto_cases[index];
        sixel_palette_frame_state_init(&state);
        sixel_palette_policy_resolution_init(
            &state.sampling,
            SIXEL_PALETTE_SAMPLING_AUTO,
            SIXEL_PALETTE_POLICY_ORIGIN_AUTO);
        status = sixel_palette_sampling_resolve_auto(
            &state,
            test_case->total_threads,
            test_case->heavy_operations,
            test_case->async_eligible);
        if (SIXEL_FAILED(status) ||
                state.sampling.requested != SIXEL_PALETTE_SAMPLING_AUTO ||
                state.sampling.effective !=
                    (int)test_case->expected_policy ||
                state.sampling.origin != SIXEL_PALETTE_POLICY_ORIGIN_AUTO ||
                state.sampling.phase != SIXEL_PALETTE_POLICY_RESOLVED ||
                state.sampling.reason !=
                    SIXEL_PALETTE_RESOLUTION_RESOURCE_PROFILE ||
                state.sampling_source != test_case->expected_source) {
            return 0;
        }
    }

    sixel_palette_frame_state_init(&state);
    sixel_palette_policy_resolution_init(
        &state.sampling,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_sampling_resolve_auto(&state, 4, 0, 1);
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
