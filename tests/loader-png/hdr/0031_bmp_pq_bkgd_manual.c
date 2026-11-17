/* SPDX-License-Identifier: MIT */
#include "tests/_support/loader/png_hdr_composition_common.h"

int
test_png_hdr_31(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_hdr_composition_test(2, 1, 1);
}
