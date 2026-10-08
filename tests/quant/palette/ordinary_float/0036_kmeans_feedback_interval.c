/* SPDX-License-Identifier: MIT */
/* K-means feedback respects its iteration interval. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_feedback_interval(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_feedback(0, 2) ? 0 : 1;
}
