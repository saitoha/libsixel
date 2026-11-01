/* SPDX-License-Identifier: MIT */
/* Reject dac odd length. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0095(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 3);
}
