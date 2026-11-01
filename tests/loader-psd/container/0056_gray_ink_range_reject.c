/* SPDX-License-Identifier: MIT */
/* Check Gray color-record interpretation through the public loader. */
#include "../../_support/loader/builtin/psd_gray_ink_test_common.h"

int
test_psd_color_0269(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_gray_ink_test(7);
}
