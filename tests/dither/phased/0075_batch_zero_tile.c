/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_batch_zero_tile(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_batch_fault_check(1);
}
