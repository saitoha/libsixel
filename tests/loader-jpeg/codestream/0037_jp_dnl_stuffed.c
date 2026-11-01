/* SPDX-License-Identifier: MIT */
/* DNL redefinition skips stuffed entropy. */
#include "../../_support/loader/builtin/jpeg_dnl_test_common.h"

int
test_loader_jp_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(31);
}
