/* SPDX-License-Identifier: MIT */
/* Preserve the fixed progressive 4:2:0 pixel digest with DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0210_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(7);
}
