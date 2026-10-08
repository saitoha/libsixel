/* SPDX-License-Identifier: MIT */
/* Heckbert distinct-color mean retains float values. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_average(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_heckbert_representative(SIXEL_REP_AVERAGE_COLORS, 0) ? 0 : 1;
}
