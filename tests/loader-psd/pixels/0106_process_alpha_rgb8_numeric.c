/* SPDX-License-Identifier: MIT */
/* Verify process alpha rgb8 numeric. */
#include "../../_support/loader/builtin/psd_alpha_test_common.h"

int
test_psd_color_0127(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(127);
}
