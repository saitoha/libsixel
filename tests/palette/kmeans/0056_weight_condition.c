/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_kmgpu_0056_weight_condition(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_numeric(4);
}
