/* SPDX-License-Identifier: MIT */
/* Ordinary K-means hard bins distinguish extended samples. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_hard(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_HARD,
        1) ? 0 : 1;
}
