/* SPDX-License-Identifier: MIT */
/* Ordinary K-means preserves extended coordinates in all float spaces. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_sources(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_NONE,
        1) ? 0 : 1;
}
