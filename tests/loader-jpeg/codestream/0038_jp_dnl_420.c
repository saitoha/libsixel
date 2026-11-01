/* SPDX-License-Identifier: MIT */
/* DNL redefinition preserves subsampled pixels. */
#include "../../_support/loader/builtin/jpeg_dnl_test_common.h"

int
test_loader_jp_0061(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(32);
}
