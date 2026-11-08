/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_kmgpu_0068_snake_sse(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_image("tests/data/inputs/snake_64.png");
}
