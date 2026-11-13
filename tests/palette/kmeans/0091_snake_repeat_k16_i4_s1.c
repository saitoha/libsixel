/* SPDX-License-Identifier: MIT */
/* Verify snake repeat k16 i4 s1. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0091(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image(
        "tests/data/inputs/snake_64.png",
        16, 4, 1,
        1);
}
