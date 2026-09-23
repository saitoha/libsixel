/* SPDX-License-Identifier: MIT */
/* Reject differential size. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0124(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(18, 32);
}
