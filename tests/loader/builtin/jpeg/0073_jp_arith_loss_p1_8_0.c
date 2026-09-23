/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss p1 8 0 samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0073(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(7, 0);
}
