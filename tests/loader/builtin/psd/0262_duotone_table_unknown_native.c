/* SPDX-License-Identifier: MIT */
/* Protect rendered-table precedence at the public loader boundary. */
#include "../psd_color_test_common.h"

int
test_psd_color_0262(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_color_test(47);
}
