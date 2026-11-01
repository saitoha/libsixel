/* SPDX-License-Identifier: MIT */
/* Reject unknown frame component. */
#include "../../_support/loader/builtin/jpeg_process_test_common.h"

int
test_loader_jp_0123(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(18, 31);
}
