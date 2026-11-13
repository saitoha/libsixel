/* SPDX-License-Identifier: MIT */
/* Verify snake sse k256 i24 s42. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0094(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image(
        "tests/data/inputs/snake_64.png",
        256, 24,
        42, 0);
}
