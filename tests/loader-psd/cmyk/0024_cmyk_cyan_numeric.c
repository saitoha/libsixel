/* SPDX-License-Identifier: MIT */
/* Verify cmyk cyan numeric. */
#include "../../_support/loader/builtin/psd_spot_test_common.h"

int
test_psd_color_0075(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_spot_test(75);
}
