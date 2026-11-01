/* SPDX-License-Identifier: MIT */
/* K-means Ward counts subbyte float colors independently. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_unique(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_unique() ? 0 : 1;
}
