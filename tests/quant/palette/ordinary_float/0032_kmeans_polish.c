/* SPDX-License-Identifier: MIT */
/* Ordinary K-means Ward polishing retains the signed mean. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_polish(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_mean(3) ? 0 : 1;
}
