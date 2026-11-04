/* SPDX-License-Identifier: MIT */
#include "tests/_support/planner/pipeline/gpu_ahead_common.h"

int
test_gpu_ahead_t48_bytes(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    (void)argc;
    (void)argv;
    return gpu_ahead_compare("delta:T48:refresh_ms=80");
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
