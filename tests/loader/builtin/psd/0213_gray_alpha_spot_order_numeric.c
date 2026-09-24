/* SPDX-License-Identifier: MIT */
/* Verify gray alpha spot order numeric. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0213(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(213);
}
