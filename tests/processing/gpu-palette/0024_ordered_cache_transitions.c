/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/ordered.md
 */
#include <stdlib.h>
#include <string.h>
#include "ordered_state_common.h"

int
test_gpupal_0024_ordered_cache_transitions(int argc, char **argv)
{
    ordered_state_fixture_t a;
    ordered_state_fixture_t b;
    sixel_index_t saved[ORDERED_STATE_PIXELS];
    int i;
    static int const sizes[] = {2, 256, 1, 33};

    (void)argc;
    (void)argv;
    ordered_state_init(&a, 33, 1);
    ordered_state_init(&b, 33, 9);
    if (!ordered_state_check(&a)) {
        return EXIT_FAILURE;
    }
    /* Ordinary uploads must not overwrite the ordered table's identity. */
    b.request.lut_policy = SIXEL_LUT_POLICY_NONE;
    if (!ordered_state_check(&b) || !ordered_state_check(&a)) {
        return EXIT_FAILURE;
    }
    b.request.lut_policy = SIXEL_LUT_POLICY_GRIDLUT;
    if (!ordered_state_check(&b) || !ordered_state_check(&a)) {
        return EXIT_FAILURE;
    }
    b.request.lut_policy = SIXEL_LUT_POLICY_ORDERED;
    if (!ordered_state_check(&b) || !ordered_state_check(&a)) {
        return EXIT_FAILURE;
    }
    /* Rejection is at the public admission boundary, not a simulated
     * Metal command failure. It must neither write output nor poison the
     * next valid request, under either force or auto policy. */
    memcpy(saved, a.output, sizeof(saved));
    a.request.palette_size--;
    if (sixel_gpu_palette_apply(&a.request) != SIXEL_BAD_ARGUMENT) {
        return EXIT_FAILURE;
    }
    a.request.policy = SIXEL_GPU_POLICY_AUTO;
    if (sixel_gpu_palette_apply(&a.request) != SIXEL_FALSE
            || memcmp(saved, a.output, sizeof(saved)) != 0) {
        return EXIT_FAILURE;
    }
    a.request.policy = SIXEL_GPU_POLICY_FORCE;
    a.request.palette_size++;
    if (!ordered_state_check(&a)) {
        return EXIT_FAILURE;
    }
    /* Changed size and a changed palette at the same pointer both matter. */
    for (i = 0; i < 4; ++i) {
        ordered_state_init(&a, sizes[i], i + 3);
        if (!ordered_state_check(&a)) {
            return EXIT_FAILURE;
        }
    }
    a.palette[0] ^= 255U;
    return ordered_state_check(&a) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
