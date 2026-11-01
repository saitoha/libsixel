/* SPDX-License-Identifier: MIT */
/* Verify process icc domain reject. */
#include "../../_support/loader/builtin/psd_process_test_common.h"

int
test_psd_color_0101(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(101);
}
