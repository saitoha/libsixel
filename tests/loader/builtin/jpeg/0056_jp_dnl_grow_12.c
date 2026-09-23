/* SPDX-License-Identifier: MIT */
/* DNL growth preserves twelve-bit samples. */
#include "../jpeg_dnl_test_common.h"

int
test_loader_jp_0056(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(27);
}
