/* SPDX-License-Identifier: MIT */
/* Verify gradient cpu mse k64 i24 s42. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0109(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image_quality(
        "tests/data/palette/kmeans/gradient-16.png",
        64, 24, 42, 0, 0.0016953813333921992 * 1.0025);
}
