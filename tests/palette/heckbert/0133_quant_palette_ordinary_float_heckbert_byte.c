/* SPDX-License-Identifier: MIT */
/* Byte Heckbert preserves its bounded histogram. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_byte(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_byte(SIXEL_QUANTIZE_MODEL_MEDIANCUT) ? 0 : 1;
}
