/* SPDX-License-Identifier: MIT */
/* Protect rendered-table precedence at the public loader boundary. */
#include "../../_support/loader/builtin/psd_color_test_common.h"

int
test_psd_color_0261(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_color_test(46);
}
