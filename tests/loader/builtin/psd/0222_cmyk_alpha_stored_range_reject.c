/* SPDX-License-Identifier: MIT */
/* Verify cmyk alpha stored range reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0222(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(222);
}
