/* SPDX-License-Identifier: MIT */
/* Reject lossless point transform. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0127(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(7, 37);
}
