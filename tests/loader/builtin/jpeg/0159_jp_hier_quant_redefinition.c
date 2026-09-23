/* SPDX-License-Identifier: MIT */
/* Verify hier quant redefinition with independent samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0159(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(34, 0);
}
