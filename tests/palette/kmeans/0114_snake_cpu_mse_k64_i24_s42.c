/* SPDX-License-Identifier: MIT */
/* Verify snake cpu mse k64 i24 s42. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0114(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/inputs/snake_16.png",
        64, 24, 42, 0, 0.0001267968450873112 * 1.0025);
}
