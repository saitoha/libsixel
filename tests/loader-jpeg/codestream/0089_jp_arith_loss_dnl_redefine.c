/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss dnl redefine samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0153(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(7, 102);
}
