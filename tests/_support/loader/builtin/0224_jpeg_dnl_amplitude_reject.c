/* SPDX-License-Identifier: MIT */
/* Reject missing amplitude bits before DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0224_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(21);
}
