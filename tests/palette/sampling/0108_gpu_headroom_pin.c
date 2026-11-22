/* SPDX-License-Identifier: MIT */
/* Verify the private profiling pin without a GPU backend. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <sixel.h>
#include "src/gpu-statistics.h"

int
test_gpu_headroom_pin(int argc, char **argv)
{
    sixel_gpu_headroom_t state;
    int choice;
    int expected;

    if (argc != 2) return 1;
    expected = strcmp(argv[1], "gpu") == 0 ? 1 : 0;
    memset(&state, 0, sizeof(state));
    state.gpu = 1 - expected;
    choice = sixel_gpu_headroom_step(&state,
        SIXEL_GPU_POLICY_AUTO, 0, 100);
    if (choice != expected || state.gpu != expected) return 2;
    choice = sixel_gpu_headroom_step(&state,
        SIXEL_GPU_POLICY_OFF, 0, 100);
    if (choice != 0 || state.gpu != 0) return 3;
    return 0;
}
