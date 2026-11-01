/* SPDX-License-Identifier: MIT */
/* DNL adjusts height within the final MCU row. */
#include "../../_support/loader/builtin/jpeg_dnl_test_common.h"

int
test_loader_jp_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(33);
}
