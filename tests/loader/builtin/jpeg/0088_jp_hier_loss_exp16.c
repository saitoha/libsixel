/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier loss exp16 samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0088(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(22, 0);
}
