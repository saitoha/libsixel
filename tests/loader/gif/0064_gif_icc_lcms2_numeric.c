/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC lcms2 independently of CMS output. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(1, SIXEL_CMS_ENGINE_LCMS2);
}
