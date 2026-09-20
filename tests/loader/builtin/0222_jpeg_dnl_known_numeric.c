/* SPDX-License-Identifier: MIT */
/* Preserve known-height JPEG with a matching DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0222_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(19);
}
