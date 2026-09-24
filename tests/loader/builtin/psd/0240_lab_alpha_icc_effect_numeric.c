/* SPDX-License-Identifier: MIT */
/* Verify lab alpha icc effect numeric. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0240(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(240);
}
