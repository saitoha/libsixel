/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_gpu_blockcull_batch_partial_tiles(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_batch_equivalence(7, 0, 0, 0);
}
