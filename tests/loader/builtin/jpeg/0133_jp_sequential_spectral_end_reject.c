/* SPDX-License-Identifier: MIT */
/* Reject sequential spectral end. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0133(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 44);
}
