/* SPDX-License-Identifier: MIT */
/* Verify gradient gpu mse k16 i4 s1. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0069(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/palette/kmeans/gradient-16.png",
        16, 4, 1, 1, 0.0070598915727245705 * 1.0025);
}
