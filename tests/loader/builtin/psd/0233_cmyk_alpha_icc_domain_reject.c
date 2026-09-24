/* SPDX-License-Identifier: MIT */
/* Verify cmyk alpha icc domain reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0233(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(233);
}
