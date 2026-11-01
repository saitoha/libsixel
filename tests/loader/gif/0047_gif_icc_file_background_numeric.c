/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC file background independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(10, SIXEL_CMS_ENGINE_BUILTIN);
}
