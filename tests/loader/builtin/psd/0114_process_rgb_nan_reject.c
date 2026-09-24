/* SPDX-License-Identifier: MIT */
/* Verify process rgb nan reject. */
#include "../psd_process_test_common.h"

int
test_psd_color_0114(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(114);
}
