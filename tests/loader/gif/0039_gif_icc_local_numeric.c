/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC local independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(2, SIXEL_CMS_ENGINE_BUILTIN);
}
