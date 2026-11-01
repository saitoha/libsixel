/* SPDX-License-Identifier: MIT */
/* Ordinary K-means Ward preserves signed means. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_ward(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_ward(SIXEL_QUANTIZE_MODEL_KMEANS) ? 0 : 1;
}
