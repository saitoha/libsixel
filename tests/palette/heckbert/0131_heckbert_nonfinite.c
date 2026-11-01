/* SPDX-License-Identifier: MIT */
/* Heckbert nonfinite input fails without retry. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_nonfinite(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_MEDIANCUT, 1, 0) ? 0 : 1;
}
