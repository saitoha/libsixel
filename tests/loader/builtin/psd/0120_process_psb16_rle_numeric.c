/* SPDX-License-Identifier: MIT */
/* Verify process psb16 rle numeric. */
#include "../psd_process_test_common.h"

int
test_psd_color_0120(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(120);
}
