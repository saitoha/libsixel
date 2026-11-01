/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC disposal2 independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(13, SIXEL_CMS_ENGINE_BUILTIN);
}
