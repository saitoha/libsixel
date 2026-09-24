/* SPDX-License-Identifier: MIT */
/* Verify gray16 alpha icc numeric. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0202(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(202);
}
