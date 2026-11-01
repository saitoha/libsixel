/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith rgb float samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0067(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(1, 0);
}
