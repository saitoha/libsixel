/* SPDX-License-Identifier: MIT */
/* Verify arith wide prog with independent samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0161(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(36, 0);
}
