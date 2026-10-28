/* SPDX-License-Identifier: MIT */
#include "adaptive_range_test_common.h"

int
test_0016_range_sufficient_colors(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_gate(16);
}
