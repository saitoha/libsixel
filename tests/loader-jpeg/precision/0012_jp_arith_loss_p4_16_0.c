/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss p4 16 0 samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0078(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(12, 0);
}
