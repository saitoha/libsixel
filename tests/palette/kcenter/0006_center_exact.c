/* SPDX-License-Identifier: MIT */
/* K-center exact bins distinguish extended coordinates. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_center_exact(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KCENTER,
        SIXEL_PALETTE_BINNING_EXACT, 1) ? 0 : 1;
}
