/* SPDX-License-Identifier: MIT */
/* Resolve delayed height across lossless RGB restart markers. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0208_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(5);
}
