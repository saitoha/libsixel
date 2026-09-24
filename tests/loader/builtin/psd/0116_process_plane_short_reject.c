/* SPDX-License-Identifier: MIT */
/* Verify process plane short reject. */
#include "../psd_process_test_common.h"

int
test_psd_color_0116(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(116);
}
