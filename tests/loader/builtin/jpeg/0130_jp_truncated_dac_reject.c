/* SPDX-License-Identifier: MIT */
/* Reject truncated dac. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0130(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 40);
}
