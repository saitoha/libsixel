/* SPDX-License-Identifier: MIT */
/* Reject every truncated DNL marker field. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0214_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(11);
}
