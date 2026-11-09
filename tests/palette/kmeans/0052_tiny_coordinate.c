/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_kmgpu_0052_tiny_coordinate(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_numeric(0);
}
