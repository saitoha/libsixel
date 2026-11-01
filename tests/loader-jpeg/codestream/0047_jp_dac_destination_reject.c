/* SPDX-License-Identifier: MIT */
/* Reject dac destination. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0097(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 5);
}
