/* SPDX-License-Identifier: MIT */
/* DNL replaces an overestimated SOF height. */
#include "../jpeg_dnl_test_common.h"

int
test_loader_jp_0054(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(25);
}
