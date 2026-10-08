/* SPDX-License-Identifier: MIT */
/* Failed float K-means allocation is not retried as byte. */
#include "ordinary_float_test_common.h"

int
test_ordinary_kmeans_fault(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KMEANS, 0, 0) ? 0 : 1;
}
