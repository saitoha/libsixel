/* SPDX-License-Identifier: MIT */
/* Verify gray alpha overflow reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0207(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(207);
}
