/* SPDX-License-Identifier: MIT */
/* Verify rare cpu mse k64 i12 s7. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0110(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/palette/kmeans/rare-16.png",
        64, 12, 7, 0, 0.00011661085153299421 * 1.0025);
}
