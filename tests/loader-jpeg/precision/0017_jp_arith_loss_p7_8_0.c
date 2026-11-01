/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss p7 8 0 samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0083(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(17, 0);
}
