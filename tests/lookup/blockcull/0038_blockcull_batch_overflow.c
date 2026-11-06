/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_blockcull_batch_overflow(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_batch_equivalence(255, 0, 0, 0);
}
