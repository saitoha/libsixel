/* SPDX-License-Identifier: MIT */
/* Invalid float coordinates fail without a lossy retry. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_center_nonfinite(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KCENTER, 1, 0) ? 0 : 1;
}
