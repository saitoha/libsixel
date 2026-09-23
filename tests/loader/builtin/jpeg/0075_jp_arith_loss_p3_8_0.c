/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss p3 8 0 samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0075(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(9, 0);
}
