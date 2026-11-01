/* SPDX-License-Identifier: MIT */
/* Ward merge retains signed independent means. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_medoids_ward(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_ward(SIXEL_QUANTIZE_MODEL_KMEDOIDS) ? 0 : 1;
}
