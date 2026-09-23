/* SPDX-License-Identifier: MIT */
/* Reject duplicate expansion. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0120(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(23, 28);
}
