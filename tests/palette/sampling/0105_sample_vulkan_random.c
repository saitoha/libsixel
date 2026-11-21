/* SPDX-License-Identifier: MIT */
#include "sample_vulkan_exact.h"
int
test_sample_vulkan_random(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return sample_vulkan_exact_check(0);
}
