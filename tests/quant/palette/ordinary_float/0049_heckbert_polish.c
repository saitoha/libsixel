/* SPDX-License-Identifier: MIT */
/* Heckbert Ward refinement retains signed means. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_polish(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_heckbert_representative(SIXEL_REP_CENTER_BOX, 1) ? 0 : 1;
}
