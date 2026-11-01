/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier arith dnl redefine samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0155(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(24, 102);
}
