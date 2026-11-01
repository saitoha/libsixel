/* SPDX-License-Identifier: MIT */
/* Verify lab alpha truncated reject. */
#include "../../_support/loader/builtin/psd_alpha_test_common.h"

int
test_psd_color_0255(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(255);
}
