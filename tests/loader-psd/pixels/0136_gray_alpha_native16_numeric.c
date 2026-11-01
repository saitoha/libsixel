/* SPDX-License-Identifier: MIT */
/* Verify gray alpha native16 numeric. */
#include "../../_support/loader/builtin/psd_alpha_test_common.h"

int
test_psd_color_0234(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(234);
}
