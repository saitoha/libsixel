/* SPDX-License-Identifier: MIT */
/* Measure completed device time against elapsed frame time. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <sixel.h>
#include "src/gpu-statistics.h"

int
test_gpu_headroom(int argc, char **argv)
{
    sixel_gpu_headroom_t state;
    sixel_gpu_headroom_t other;
    unsigned long long busy;
    unsigned long long wall;
    int choice;
    int i;

    (void)argc;
    (void)argv;
    memset(&state, 0, sizeof(state));
    busy = 0;
    wall = 1000000000ULL;
    choice = sixel_gpu_headroom_step(&state,
        SIXEL_GPU_POLICY_AUTO, busy, wall);
    if (choice != 0) return 1;
    for (i = 0; i < 16; ++i) {
        busy += 2000000ULL;
        wall += 10000000ULL;
        choice = sixel_gpu_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 1) return 2;
    for (i = 0; i < 24; ++i) {
        busy += 5500000ULL;
        wall += 10000000ULL;
        choice = sixel_gpu_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 1) return 3;
    for (i = 0; i < 16; ++i) {
        busy += 9500000ULL;
        wall += 10000000ULL;
        choice = sixel_gpu_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 0) return 4;
    for (i = 0; i < 16; ++i) {
        busy += 5500000ULL;
        wall += 10000000ULL;
        choice = sixel_gpu_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 0) return 5;
    choice = sixel_gpu_headroom_step(&state,
        SIXEL_GPU_POLICY_OFF, busy, wall);
    if (choice != 0 || state.windows != 0 || state.gpu != 0) return 6;
    /* Fast frames still need 20 ms per window, and a single long frame
     * must not bypass the minimum of eight frame observations. */
    memset(&state, 0, sizeof(state));
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0, 100);
    for (i = 1; i <= 16; ++i)
        sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0,
            100 + (unsigned long long)i * 1000000ULL);
    if (state.windows != 0 || state.gpu != 0) return 7;
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0, 20000100);
    if (state.windows != 1 || state.gpu != 0) return 8;
    for (i = 1; i <= 7; ++i)
        sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0,
            20000100 + (unsigned long long)i * 10000000ULL);
    if (state.windows != 1 || state.gpu != 0) return 9;
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0, 100000100);
    if (state.windows != 2 || state.gpu != 1) return 10;
    /* A second stage starts on CPU without changing the first stage. */
    memset(&other, 0, sizeof(other));
    sixel_gpu_headroom_step(&other, SIXEL_GPU_POLICY_AUTO, 0, 100000100);
    if (other.gpu != 0 || state.gpu != 1) return 11;
    choice = sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_OFF, 0, 0);
    if (choice != 0 || state.windows != 0 || state.last_wall_ns != 0)
        return 12;
    /* Thresholds are strict, and each new window halves the prior load.
     * Compare the stored load to values rounded to double precision. */
    state.last_wall_ns = 100;
    state.windows = 2;
    state.load = 0.5;
    state.frames = 7;
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO,
        10000000, 20000100);
    if (state.gpu != 0 || state.load != 0.5) return 13;
    state.gpu = 1;
    state.load = 0.7;
    state.frames = 7;
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO,
        24000000, 40000100);
    if (state.gpu != 1 || state.load != (double)0.7) return 14;
    state.frames = 7;
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO,
        44000000, 60000100);
    if (state.gpu != 0 || state.load != (double)0.85) return 15;
    /* Counter or clock rollback invalidates the observation window. */
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0, 60000101);
    if (state.windows != 0 || state.gpu != 0) return 16;
    state.gpu = 1;
    sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_AUTO, 0, 60000100);
    if (state.windows != 0 || state.gpu != 0) return 17;
    /* OFFLOAD takes the device at once, before any observation window,
     * and keeps it under full device load. */
    memset(&state, 0, sizeof(state));
    choice = sixel_gpu_headroom_step(&state,
        SIXEL_GPU_POLICY_OFFLOAD, 0, 100);
    if (choice != 1 || state.gpu != 1) return 18;
    for (i = 1; i <= 16; ++i) {
        choice = sixel_gpu_headroom_step(&state, SIXEL_GPU_POLICY_OFFLOAD,
            (unsigned long long)i * 10000000ULL,
            100 + (unsigned long long)i * 10000000ULL);
        if (choice != 1) return 19;
    }
    return 0;
}
