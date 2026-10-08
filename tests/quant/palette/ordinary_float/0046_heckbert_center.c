/* SPDX-License-Identifier: MIT */
/* Heckbert box center retains float extrema. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_center(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_heckbert_representative(SIXEL_REP_CENTER_BOX, 0) ? 0 : 1;
}
