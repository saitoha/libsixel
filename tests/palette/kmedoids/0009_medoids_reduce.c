/* SPDX-License-Identifier: MIT */
/* Reduced ordinary palettes select extended source candidates. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_medoids_reduce(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_reduce(SIXEL_QUANTIZE_MODEL_KMEDOIDS) ? 0 : 1;
}
