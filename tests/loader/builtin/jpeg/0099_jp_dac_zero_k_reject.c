/* SPDX-License-Identifier: MIT */
/* Reject dac zero k. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0099(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(2, 7);
}
