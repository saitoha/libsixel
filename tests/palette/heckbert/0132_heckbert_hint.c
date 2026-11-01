/* SPDX-License-Identifier: MIT */
/* Typed Heckbert input overrides the byte preference. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_hint(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_PALETTE_BINNING_NONE, 0) ? 0 : 1;
}
