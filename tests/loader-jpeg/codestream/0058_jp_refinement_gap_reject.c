/* SPDX-License-Identifier: MIT */
/* Reject refinement gap. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0108(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(3, 16);
}
