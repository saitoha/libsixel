/* SPDX-License-Identifier: MIT */
/* Float K-means rejects nonfinite samples without retry. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_nonfinite(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KMEANS, 1, 0) ? 0 : 1;
}
