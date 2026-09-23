/* SPDX-License-Identifier: MIT */
/* Reject repeated known-height DNL. */
#include "../jpeg_dnl_test_common.h"

int
test_loader_jp_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(36);
}
