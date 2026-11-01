/* SPDX-License-Identifier: MIT */
/* Reject absent DNL despite marker-shaped bytes in COM. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0211_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(8);
}
