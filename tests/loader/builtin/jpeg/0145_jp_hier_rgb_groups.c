/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier rgb groups samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0145(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(28, 0);
}
