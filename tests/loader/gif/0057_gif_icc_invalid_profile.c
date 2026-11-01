/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC invalid profile independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(20, SIXEL_CMS_ENGINE_BUILTIN);
}
