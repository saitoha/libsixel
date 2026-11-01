/* SPDX-License-Identifier: MIT */
/* Verify lab psb prediction numeric. */
#include "../../_support/loader/builtin/psd_model_test_common.h"

int
test_psd_color_0192(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_model_test(192);
}
