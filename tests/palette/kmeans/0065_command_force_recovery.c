/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/gpu_kmeans/gpu_kmeans_contract.h"

int
test_kmgpu_0065_command_force_recovery(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return kmgpu_contract_fault(3, 1, 0);
}
