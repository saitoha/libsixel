/* SPDX-License-Identifier: MIT */
/* K-center hard bins expand addresses without clipping source coordinates. */
#include "ordinary_float_test_common.h"

int
test_ordinary_center_hard(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KCENTER,
        SIXEL_PALETTE_BINNING_HARD, 1) ? 0 : 1;
}
