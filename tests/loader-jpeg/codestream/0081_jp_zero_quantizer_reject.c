/* SPDX-License-Identifier: MIT */
/* Reject zero quantizer. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0131(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(0, 42);
}
