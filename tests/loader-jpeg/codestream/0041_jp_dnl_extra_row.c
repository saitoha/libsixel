/* SPDX-License-Identifier: MIT */
/* Reject known-height DNL with excess MCU rows. */
#include "../../_support/loader/builtin/jpeg_dnl_test_common.h"

int
test_loader_jp_0064(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_dnl_test(35);
}
