/* SPDX-License-Identifier: MIT */
/* Ordinary K-means exact bins retain source coordinates. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_exact(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_EXACT,
        1) ? 0 : 1;
}
