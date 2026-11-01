/* SPDX-License-Identifier: MIT */
/* Ordinary K-means honors partial iteration and final snap. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_snap(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_snap(SIXEL_QUANTIZE_MODEL_KMEANS) ? 0 : 1;
}
