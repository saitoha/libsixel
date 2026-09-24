/* SPDX-License-Identifier: MIT */
/* Verify process alpha layer reject. */
#include "../psd_process_test_common.h"

int
test_psd_color_0106(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(106);
}
