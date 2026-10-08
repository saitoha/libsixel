/* SPDX-License-Identifier: MIT */
/* K-means empty-cluster repair retains signed donor moments. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_empty(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_empty() ? 0 : 1;
}
