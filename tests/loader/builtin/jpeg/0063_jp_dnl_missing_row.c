/* SPDX-License-Identifier: MIT */
/* Reject known-height DNL with missing MCU rows. */
#include "../jpeg_dnl_test_common.h"

int
test_loader_jp_0063(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(34);
}
