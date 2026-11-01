/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier arith seq samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0090(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(24, 0);
}
