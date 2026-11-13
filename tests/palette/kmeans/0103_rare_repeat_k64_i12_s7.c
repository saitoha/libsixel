/* SPDX-License-Identifier: MIT */
/* Verify rare repeat k64 i12 s7. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0103(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_repeat(
        "tests/data/palette/kmeans/rare-16.png",
        64, 12, 7);
}
