/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/adaptive_range/adaptive_range_test_common.h"

int
test_0016_range_sufficient_colors(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_gate(16);
}
