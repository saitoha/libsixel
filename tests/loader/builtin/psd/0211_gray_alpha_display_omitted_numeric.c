/* SPDX-License-Identifier: MIT */
/* Verify gray alpha display omitted numeric. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0211(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(211);
}
