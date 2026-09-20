/* SPDX-License-Identifier: MIT */
/* Skip bounded DNL extension bytes without decoding them. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0221_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(18);
}
