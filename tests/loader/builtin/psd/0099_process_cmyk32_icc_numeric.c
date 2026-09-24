/* SPDX-License-Identifier: MIT */
/* Verify process cmyk32 icc numeric. */
#include "../psd_process_test_common.h"

int
test_psd_color_0099(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(99);
}
