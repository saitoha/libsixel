/* SPDX-License-Identifier: MIT */
/* Use DNL to redefine a known SOF height. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0223_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(20);
}
