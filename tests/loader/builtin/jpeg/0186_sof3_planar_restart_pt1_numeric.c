/* SPDX-License-Identifier: MIT */
/* Compare every SOF3 sample with the literal unsigned source. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0186(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(59, 0);
}
