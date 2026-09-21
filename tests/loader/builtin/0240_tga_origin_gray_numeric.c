/* SPDX-License-Identifier: MIT */
/* Read grayscale palette entries directly after the image ID. */
#include "tga_origin_test_common.h"

int
test_loader_0240_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(12);
}
