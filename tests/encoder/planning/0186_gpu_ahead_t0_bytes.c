/* SPDX-License-Identifier: MIT */
#include "tests/_support/planner/pipeline/gpu_ahead_common.h"

int
test_gpu_ahead_t0_bytes(int argc, char **argv)
{
#if SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    (void)argc;
    (void)argv;
    return gpu_ahead_compare("delta:T0:refresh_ms=80");
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
