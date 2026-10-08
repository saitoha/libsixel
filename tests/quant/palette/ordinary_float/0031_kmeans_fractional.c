/* SPDX-License-Identifier: MIT */
/* Ordinary K-means Ward retains fractional soft-bin populations. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_fractional(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_mean(0) ? 0 : 1;
}
