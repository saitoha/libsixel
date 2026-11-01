/* SPDX-License-Identifier: MIT */
/* Reject an oversized segment while searching for DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0227_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(24);
}
