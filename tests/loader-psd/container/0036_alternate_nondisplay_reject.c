/* SPDX-License-Identifier: MIT */
/* Verify alternate nondisplay reject. */
#include "../../_support/loader/builtin/psd_spot_test_common.h"

int
test_psd_color_0064(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_spot_test(64);
}
