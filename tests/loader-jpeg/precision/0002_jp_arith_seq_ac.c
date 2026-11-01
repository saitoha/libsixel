/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith seq ac samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0068(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(2, 0);
}
