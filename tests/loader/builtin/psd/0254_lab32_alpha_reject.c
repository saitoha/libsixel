/* SPDX-License-Identifier: MIT */
/* Verify lab32 alpha reject. */
#include "../psd_alpha_test_common.h"

int
test_psd_color_0254(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(254);
}
