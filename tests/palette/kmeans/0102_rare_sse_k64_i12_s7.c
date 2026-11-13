/* SPDX-License-Identifier: MIT */
/* Verify rare sse k64 i12 s7. */
#include "tests/_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_palette_kmeans_0102(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image(
        "images/measurements/palette-pipeline/rare-colors-600x450.png",
        64, 12, 7, 0);
}
