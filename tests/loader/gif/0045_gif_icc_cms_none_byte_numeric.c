/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC cms none byte independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(8, SIXEL_CMS_ENGINE_BUILTIN);
}
