/* SPDX-License-Identifier: MIT */
/* Skip stuffed FF bytes while finding the first-scan DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0209_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(6);
}
