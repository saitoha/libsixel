/* SPDX-License-Identifier: MIT */
/* Accept repeated FF fill bytes before the DNL marker. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0225_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(22);
}
