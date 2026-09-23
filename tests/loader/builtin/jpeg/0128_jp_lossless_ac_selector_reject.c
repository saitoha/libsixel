/* SPDX-License-Identifier: MIT */
/* Reject lossless ac selector. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0128(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(7, 38);
}
