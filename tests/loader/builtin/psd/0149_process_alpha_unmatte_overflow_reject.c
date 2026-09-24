/* SPDX-License-Identifier: MIT */
/* Verify process alpha unmatte overflow reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0149(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(149);
}
