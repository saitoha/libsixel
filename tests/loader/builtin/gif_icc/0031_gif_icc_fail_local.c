/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC fail local. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0031(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_failure_test(31);
}
