/* SPDX-License-Identifier: MIT */
/* Verify hier short entropy reject. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0165(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(23, 46);
}
