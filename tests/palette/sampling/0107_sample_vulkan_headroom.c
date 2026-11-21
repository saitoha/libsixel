/* SPDX-License-Identifier: MIT */
/* Measure completed device time against elapsed frame time. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <sixel.h>
#include "src/filter-sample.h"

int
test_sample_vulkan_headroom(int argc, char **argv)
{
#if defined(HAVE_VULKAN)
    sixel_sample_vulkan_headroom_t state;
    unsigned long long busy;
    unsigned long long wall;
    int choice;
    int i;

    (void)argc;
    (void)argv;
    memset(&state, 0, sizeof(state));
    busy = 0;
    wall = 1000000000ULL;
    choice = sixel_filter_sample_vulkan_headroom_step(&state,
        SIXEL_GPU_POLICY_AUTO, busy, wall);
    if (choice != 0) return 1;
    for (i = 0; i < 16; ++i) {
        busy += 2000000ULL;
        wall += 10000000ULL;
        choice = sixel_filter_sample_vulkan_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 1) return 2;
    for (i = 0; i < 24; ++i) {
        busy += 5500000ULL;
        wall += 10000000ULL;
        choice = sixel_filter_sample_vulkan_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 1) return 3;
    for (i = 0; i < 16; ++i) {
        busy += 9500000ULL;
        wall += 10000000ULL;
        choice = sixel_filter_sample_vulkan_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 0) return 4;
    for (i = 0; i < 16; ++i) {
        busy += 5500000ULL;
        wall += 10000000ULL;
        choice = sixel_filter_sample_vulkan_headroom_step(&state,
            SIXEL_GPU_POLICY_AUTO, busy, wall);
    }
    if (choice != 0) return 5;
    choice = sixel_filter_sample_vulkan_headroom_step(&state,
        SIXEL_GPU_POLICY_OFF, busy, wall);
    if (choice != 0 || state.windows != 0 || state.gpu != 0) return 6;
    return 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
