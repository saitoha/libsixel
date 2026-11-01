/* SPDX-License-Identifier: MIT */
/* K-means feedback retains requested partial snap stages. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_kmeans_feedback_snap(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_kmeans_feedback(1, 1) ? 0 : 1;
}
