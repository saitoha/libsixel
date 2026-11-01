/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith loss p4 16 3 samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0079(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(13, 0);
}
