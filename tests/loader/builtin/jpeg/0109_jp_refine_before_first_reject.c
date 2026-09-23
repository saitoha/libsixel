/* SPDX-License-Identifier: MIT */
/* Reject refine before first. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0109(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(3, 17);
}
