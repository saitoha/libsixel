/* SPDX-License-Identifier: MIT */
/* Reject a duplicate DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0217_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(14);
}
