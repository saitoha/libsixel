/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss p4 2 0 samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0080(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(14, 0);
}
