/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith custom dac samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0070(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(4, 0);
}
