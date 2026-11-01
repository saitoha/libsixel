/* SPDX-License-Identifier: MIT */
/* K-means residual feedback distinguishes extended samples. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_feedback(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_feedback(0, 1) ? 0 : 1;
}
