/* SPDX-License-Identifier: MIT */
/* Byte K-means keeps byte-only palette storage. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_byte(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_byte(SIXEL_QUANTIZE_MODEL_KMEANS) ? 0 : 1;
}
