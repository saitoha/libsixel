/* SPDX-License-Identifier: MIT */
/* Verify process missing composite reject. */
#include "../../_support/loader/builtin/psd_process_test_common.h"

int
test_psd_color_0121(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(121);
}
