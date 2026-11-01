/* SPDX-License-Identifier: MIT */
/* kcenter retains generated sub-byte coordinates without native output */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_kcenter(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retention_generated(SIXEL_QUANTIZE_MODEL_KCENTER) ? 0 : 1;
}
