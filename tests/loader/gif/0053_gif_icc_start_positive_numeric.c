/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC start positive independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(16, SIXEL_CMS_ENGINE_BUILTIN);
}
