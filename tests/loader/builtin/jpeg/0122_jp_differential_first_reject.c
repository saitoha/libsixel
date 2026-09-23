/* SPDX-License-Identifier: MIT */
/* Reject differential first. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0122(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(18, 30);
}
