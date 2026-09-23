/* SPDX-License-Identifier: MIT */
/* Verify hier positive clip with independent samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0157(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(32, 0);
}
