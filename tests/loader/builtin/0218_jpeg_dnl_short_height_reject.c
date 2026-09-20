/* SPDX-License-Identifier: MIT */
/* Reject a DNL height that leaves excess encoded blocks. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0218_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(15);
}
