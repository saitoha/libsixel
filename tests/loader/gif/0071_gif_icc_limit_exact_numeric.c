/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC icc limit exact. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0034(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_limit_test(0);
}
