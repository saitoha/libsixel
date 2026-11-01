/* SPDX-License-Identifier: MIT */
/* Typed float input determines arithmetic despite a byte preference. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_medoids_hint(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        SIXEL_PALETTE_BINNING_NONE, 0) ? 0 : 1;
}
