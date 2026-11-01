/* SPDX-License-Identifier: MIT */
/* Heckbert occurrence mean retains float values. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_histogram(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_heckbert_representative(SIXEL_REP_AVERAGE_PIXELS, 0) ? 0 : 1;
}
