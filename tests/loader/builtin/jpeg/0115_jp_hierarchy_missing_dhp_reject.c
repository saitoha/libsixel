/* SPDX-License-Identifier: MIT */
/* Reject hierarchy missing dhp. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0115(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(18, 23);
}
