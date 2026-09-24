/* SPDX-License-Identifier: MIT */
/* Verify process alpha mt16 reject. */
#include "../psd_process_test_common.h"

int
test_psd_color_0108(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(108);
}
