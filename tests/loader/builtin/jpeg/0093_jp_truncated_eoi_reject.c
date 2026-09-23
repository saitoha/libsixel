/* SPDX-License-Identifier: MIT */
/* Reject truncated eoi. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0093(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 1);
}
