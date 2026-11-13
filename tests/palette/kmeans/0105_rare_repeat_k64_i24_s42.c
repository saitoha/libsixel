/* SPDX-License-Identifier: MIT */
/* Verify rare repeat k64 i24 s42. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0105(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_repeat(
        "tests/data/palette/kmeans/rare-16.png",
        64, 24, 42);
}
