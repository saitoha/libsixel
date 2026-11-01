/* SPDX-License-Identifier: MIT */
/* Continue progressive AC scans after the first-scan DNL. */
#include "jpeg_dnl_test_common.h"

int
test_loader_0206_jpeg_dnl(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(3);
}
