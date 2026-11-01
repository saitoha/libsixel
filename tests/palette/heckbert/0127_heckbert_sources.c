/* SPDX-License-Identifier: MIT */
/* Preserve extended Heckbert sources in five spaces. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_sources(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_PALETTE_BINNING_NONE, 1) ? 0 : 1;
}
