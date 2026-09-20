/* SPDX-License-Identifier: MIT */
/* Reject DNL before the first scan. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0215_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(12);
}
