/* SPDX-License-Identifier: MIT */
#include "../../_support/quant/palette/adaptive_range/adaptive_range_test_common.h"

int
test_0018_range_recoverable_tones(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_gate(18);
}
