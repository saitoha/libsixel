/* SPDX-License-Identifier: MIT */
/* Read packed RGB16 palette entries directly after the image ID. */
#include "tga_origin_test_common.h"

int
test_loader_0241_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(13);
}
