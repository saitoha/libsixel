/* SPDX-License-Identifier: MIT */
/* Reject dac bounds. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0098(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 6);
}
