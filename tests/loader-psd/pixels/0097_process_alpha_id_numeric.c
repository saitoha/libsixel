/* SPDX-License-Identifier: MIT */
/* Verify process alpha id numeric. */
#include "../../_support/loader/builtin/psd_process_test_common.h"

int
test_psd_color_0105(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(105);
}
