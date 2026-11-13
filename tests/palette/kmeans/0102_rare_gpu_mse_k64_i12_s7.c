/* SPDX-License-Identifier: MIT */
/* Verify rare gpu mse k64 i12 s7. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0102(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/palette/kmeans/rare-16.png",
        64, 12, 7, 1, 0.00011661085153540157 * 1.0025);
}
