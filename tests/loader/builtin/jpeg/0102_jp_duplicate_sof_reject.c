/* SPDX-License-Identifier: MIT */
/* Reject duplicate sof. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0102(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(1, 10);
}
