/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/png_hdr_composition_common.h"

int
test_png_hdr_28(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_hdr_composition_test(1, 1, 0);
}
