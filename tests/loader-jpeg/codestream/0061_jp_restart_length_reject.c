/* SPDX-License-Identifier: MIT */
/* Reject restart length. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0111(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(2, 19);
}
