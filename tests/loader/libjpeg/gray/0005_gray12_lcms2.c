/* SPDX-License-Identifier: MIT */
/* Verify gray12 lcms2. */
#include "tests/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(12, SIXEL_CMS_ENGINE_LCMS2, 0);
}
