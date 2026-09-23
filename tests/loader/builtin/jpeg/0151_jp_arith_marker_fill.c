/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith marker fill samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0151(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(3, 103);
}
