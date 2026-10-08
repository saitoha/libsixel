/* SPDX-License-Identifier: MIT */
/* Reduced ordinary palettes select extended source candidates. */
#include "ordinary_float_test_common.h"

int
test_ordinary_center_reduce(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_reduce(SIXEL_QUANTIZE_MODEL_KCENTER) ? 0 : 1;
}
