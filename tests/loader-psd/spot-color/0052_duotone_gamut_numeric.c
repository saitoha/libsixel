/* SPDX-License-Identifier: MIT */
/* Verify duotone gamut numeric. */
#include "../../_support/loader/builtin/psd_color_test_common.h"

int
test_psd_color_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_color_test(8);
}
