/* SPDX-License-Identifier: MIT */
/* Verify arith wide seq with independent samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0160(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(35, 0);
}
