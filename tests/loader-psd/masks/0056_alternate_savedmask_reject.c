/* SPDX-License-Identifier: MIT */
/* Verify alternate savedmask reject. */
#include "../../_support/loader/builtin/psd_spot_test_common.h"

int
test_psd_color_0085(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_spot_test(85);
}
