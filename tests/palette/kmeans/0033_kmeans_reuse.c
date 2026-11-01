/* SPDX-License-Identifier: MIT */
/* K-means can rebuild after float allocation failure. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_reuse(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KMEANS, 0, 1) ? 0 : 1;
}
