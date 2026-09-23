/* SPDX-License-Identifier: MIT */
/* Reject unknown trailing marker. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0134(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 45);
}
