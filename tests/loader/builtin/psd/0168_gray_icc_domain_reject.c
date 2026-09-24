/* SPDX-License-Identifier: MIT */
/* Verify gray icc domain reject. */
#include "../psd_model_test_common.h"

int
test_psd_color_0168(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_model_test(168);
}
