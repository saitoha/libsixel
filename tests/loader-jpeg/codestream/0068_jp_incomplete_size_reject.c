/* SPDX-License-Identifier: MIT */
/* Reject incomplete size. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0118(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(18, 26);
}
