/* SPDX-License-Identifier: MIT */
/* Verify rare gpu mse k64 i24 s42. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0104(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/palette/kmeans/rare-16.png",
        64, 24, 42, 1, 8.1379791686729324e-05 * 1.0025);
}
