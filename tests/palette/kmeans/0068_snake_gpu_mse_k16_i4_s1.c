/* SPDX-License-Identifier: MIT */
/* Verify snake gpu mse k16 i4 s1. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0068(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/inputs/snake_16.png",
        16, 4, 1, 1, 0.00068967986399283161 * 1.0025);
}
