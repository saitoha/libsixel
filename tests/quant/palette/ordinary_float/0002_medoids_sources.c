/* SPDX-License-Identifier: MIT */
/* Ordinary medoids retains extended source representatives in five spaces. */
#include "ordinary_float_test_common.h"

int
test_ordinary_medoids_sources(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        SIXEL_PALETTE_BINNING_NONE, 1) ? 0 : 1;
}
