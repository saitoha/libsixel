/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith 12 seq samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0071(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(5, 0);
}
