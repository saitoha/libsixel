/* SPDX-License-Identifier: MIT */
/* Reject an early DNL at a restart interval boundary. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0220_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(17);
}
