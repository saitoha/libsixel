/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier loss exp0 samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0086(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(20, 0);
}
