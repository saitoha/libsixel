/* SPDX-License-Identifier: MIT */
/* Verify process icc unsupported reject. */
#include "../../_support/loader/builtin/psd_process_test_common.h"

int
test_psd_color_0103(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(103);
}
