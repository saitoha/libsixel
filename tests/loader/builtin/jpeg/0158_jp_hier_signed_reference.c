/* SPDX-License-Identifier: MIT */
/* Verify hier signed reference with independent samples. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0158(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(33, 0);
}
