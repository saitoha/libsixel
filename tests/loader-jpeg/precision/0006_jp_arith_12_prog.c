/* SPDX-License-Identifier: MIT */
/* Preserve numeric arith 12 prog samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0072(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(6, 0);
}
