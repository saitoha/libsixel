/* SPDX-License-Identifier: MIT */
/* Reject a DNL that still declares zero height. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0212_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(9);
}
