/* SPDX-License-Identifier: MIT */
/* Verify hier arith multiblock seq. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0163(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(37, 0);
}
