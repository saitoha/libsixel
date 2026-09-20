/* SPDX-License-Identifier: MIT */
/* Preserve twelve-bit extended sequential samples with DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0205_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(2);
}
