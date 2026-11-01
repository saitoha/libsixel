/* SPDX-License-Identifier: MIT */
/* Reject a DNL length below four. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0213_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(10);
}
