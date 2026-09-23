/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith rgb loss8 samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0146(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(29, 0);
}
