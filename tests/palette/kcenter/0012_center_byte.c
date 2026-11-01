/* SPDX-License-Identifier: MIT */
/* True byte inputs retain byte storage and exact representatives. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_center_byte(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_byte(SIXEL_QUANTIZE_MODEL_KCENTER) ? 0 : 1;
}
