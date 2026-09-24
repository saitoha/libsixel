/* SPDX-License-Identifier: MIT */
/* Verify gray alpha nan reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0208(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(208);
}
