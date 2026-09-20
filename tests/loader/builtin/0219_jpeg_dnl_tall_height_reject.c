/* SPDX-License-Identifier: MIT */
/* Reject a DNL height requiring missing encoded blocks. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0219_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(16);
}
