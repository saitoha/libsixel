/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier loss16 wrap samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0144(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(27, 0);
}
