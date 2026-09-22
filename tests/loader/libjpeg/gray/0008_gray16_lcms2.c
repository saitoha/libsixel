/* SPDX-License-Identifier: MIT */
/* Verify gray16 lcms2. */
#include "tests/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(16, SIXEL_CMS_ENGINE_LCMS2, 0);
}
