/* SPDX-License-Identifier: MIT */
/* Verify hier arith multiblock prog. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0164(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(38, 0);
}
