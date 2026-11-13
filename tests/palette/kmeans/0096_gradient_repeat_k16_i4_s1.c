/* SPDX-License-Identifier: MIT */
/* Verify gradient repeat k16 i4 s1. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0096(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_repeat(
        "tests/data/palette/kmeans/gradient-16.png",
        16, 4, 1);
}
