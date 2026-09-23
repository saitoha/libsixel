/* SPDX-License-Identifier: MIT */
/* Reject truncated dqt. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0132(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 43);
}
