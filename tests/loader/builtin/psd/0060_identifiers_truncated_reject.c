/* SPDX-License-Identifier: MIT */
/* Verify identifiers truncated reject. */
#include "../psd_spot_test_common.h"

int
test_psd_color_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_spot_test(60);
}
