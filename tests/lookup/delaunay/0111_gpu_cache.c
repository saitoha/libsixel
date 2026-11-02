/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "../../_support/processing/gpu-palette/ordered_state_common.h"

/* Every invocation observes a single cache transition. The request hook
 * distinguishes an actual rebuild from an accidentally correct old table. */
int
test_delaunay_gpu_cache(int argc, char **argv)
{
    ordered_state_fixture_t a, b;
    sixel_gpu_palette_test_hooks_t hooks;
    int transition;
    int rebuild;

    if (argc != 2) return EXIT_FAILURE;
    transition = atoi(argv[1]);
    if (transition < 0 || transition > 6) return EXIT_FAILURE;
    memset(&hooks, 0, sizeof(hooks));
    ordered_state_init(&a, 33, 1);
    a.request.lut_policy = SIXEL_LUT_POLICY_DELAUNAY;
    a.request.test_hooks = &hooks;
    if (!ordered_state_check(&a) || hooks.kernel != 6 || !hooks.rebuild) {
        return EXIT_FAILURE;
    }
    rebuild = 0;
    if (transition == 1) {
        a.palette[0] ^= 255U;
        rebuild = 1;
    } else if (transition == 2) {
        a.request.ncolors = 2;
        a.request.palette_size = 6;
        rebuild = 1;
    } else if (transition >= 3) {
        ordered_state_init(&b, 33, 9);
        b.request.lut_policy = transition == 3 ? SIXEL_LUT_POLICY_NONE
            : transition == 4 ? SIXEL_LUT_POLICY_GRIDLUT
            : transition == 5 ? SIXEL_LUT_POLICY_ORDERED
            : SIXEL_LUT_POLICY_DELAUNAY;
        if (!ordered_state_check(&b)) return EXIT_FAILURE;
        rebuild = transition == 6;
    }
    if (!ordered_state_check(&a) || hooks.kernel != 6 ||
            hooks.rebuild != rebuild || !hooks.published) {
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
