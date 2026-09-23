/* SPDX-License-Identifier: MIT */
/* Reject excessive approximation. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0110(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(3, 18);
}
