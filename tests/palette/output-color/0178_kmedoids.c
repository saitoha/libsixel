/* SPDX-License-Identifier: MIT */
/* kmedoids retains generated sub-byte coordinates without native output */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_kmedoids(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retention_generated(SIXEL_QUANTIZE_MODEL_KMEDOIDS) ? 0 : 1;
}
