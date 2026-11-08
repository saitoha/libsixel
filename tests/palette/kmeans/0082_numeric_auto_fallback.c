/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"
int
test_kmgpu_0082_numeric_auto_fallback(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_numeric_fallback(0);
}
