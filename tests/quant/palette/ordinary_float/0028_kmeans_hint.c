/* SPDX-License-Identifier: MIT */
/* Typed K-means input overrides a byte preference hint. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_hint(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_sources(SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_PALETTE_BINNING_NONE,
        0) ? 0 : 1;
}
