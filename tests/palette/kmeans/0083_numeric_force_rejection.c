/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"
int
test_kmgpu_0083_numeric_force_rejection(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_numeric_fallback(1);
}
