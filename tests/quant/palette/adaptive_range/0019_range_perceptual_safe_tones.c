/* SPDX-License-Identifier: MIT */
#include "adaptive_range_test_common.h"

int
test_0019_range_perceptual_safe_tones(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return adaptive_range_test_gate(19);
}
