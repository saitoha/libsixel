/* SPDX-License-Identifier: MIT */
/* Verify gray alpha icc domain reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0232(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(232);
}
