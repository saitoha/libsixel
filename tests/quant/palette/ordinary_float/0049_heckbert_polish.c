/* SPDX-License-Identifier: MIT */
/* Heckbert Ward refinement retains signed means. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_polish(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* AUTO owns the weighted-mean contract after Ward. Explicit center
     * now exports member extrema, covered by the Heckbert policy tests. */
    return ordinary_heckbert_representative(SIXEL_REP_AUTO, 1) ? 0 : 1;
}
