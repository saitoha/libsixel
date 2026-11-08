/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_kmgpu_0062_buffer_auto_recovery(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_fault(2, 0, 0);
}
