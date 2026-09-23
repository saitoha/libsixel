/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith prog dnl zero samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0149(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(3, 101);
}
