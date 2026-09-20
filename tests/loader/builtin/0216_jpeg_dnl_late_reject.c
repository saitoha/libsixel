/* SPDX-License-Identifier: MIT */
/* Reject DNL after the second scan. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0216_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(13);
}
