/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_gpu_batch_thread_overflow(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_batch_fault_check(2);
}
