/* SPDX-License-Identifier: MIT */
#include "adaptive_range_test_common.h"

int
test_0020_range_palette_boundary(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_gate(20);
}
