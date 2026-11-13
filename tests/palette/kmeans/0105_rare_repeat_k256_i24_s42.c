/* SPDX-License-Identifier: MIT */
/* Verify rare repeat k256 i24 s42. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0105(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image(
        "images/measurements/palette-pipeline/rare-colors-600x450.png",
        256, 24, 42, 1);
}
