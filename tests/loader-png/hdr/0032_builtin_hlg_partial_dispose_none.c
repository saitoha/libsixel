/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/png_hdr_composition_common.h"

int
test_png_hdr_32(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_hdr_composition_test(0, 2, 0);
}
