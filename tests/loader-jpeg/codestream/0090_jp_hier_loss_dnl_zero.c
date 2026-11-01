/* SPDX-License-Identifier: MIT */
/* Preserve numeric hier loss dnl zero samples. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0154(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(23, 101);
}
