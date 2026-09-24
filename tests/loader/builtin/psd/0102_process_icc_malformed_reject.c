/* SPDX-License-Identifier: MIT */
/* Verify process icc malformed reject. */
#include "../psd_process_test_common.h"

int
test_psd_color_0102(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(102);
}
