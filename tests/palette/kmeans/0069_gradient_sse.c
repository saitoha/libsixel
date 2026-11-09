/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_kmgpu_0069_gradient_sse(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image(
        "images/measurements/palette-pipeline/smooth-gradient-128x96.png");
}
