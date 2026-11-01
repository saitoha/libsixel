/* SPDX-License-Identifier: MIT */
/* Reject unknown scan component. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0104(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 12);
}
