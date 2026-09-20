/* SPDX-License-Identifier: MIT */
/* Preserve lossless predictor samples after delayed height. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0207_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(4);
}
