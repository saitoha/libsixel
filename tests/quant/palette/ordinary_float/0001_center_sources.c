/* SPDX-License-Identifier: MIT */
/* Ordinary center retains extended source representatives in five spaces. */
#include "ordinary_float_test_common.h"

int
test_ordinary_center_sources(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KCENTER,
        SIXEL_PALETTE_BINNING_NONE, 1) ? 0 : 1;
}
