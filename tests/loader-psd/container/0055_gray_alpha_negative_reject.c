/* SPDX-License-Identifier: MIT */
/* Verify gray alpha negative reject. */
#include "../../_support/loader/builtin/psd_alpha_test_common.h"

int
test_psd_color_0230(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_alpha_test(230);
}
