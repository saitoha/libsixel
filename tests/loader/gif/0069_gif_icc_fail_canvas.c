/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC fail canvas. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0032(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_failure_test(32);
}
