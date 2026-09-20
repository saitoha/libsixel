/* SPDX-License-Identifier: MIT */
/* Reject a deferred-height frame truncated after DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0226_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(23);
}
