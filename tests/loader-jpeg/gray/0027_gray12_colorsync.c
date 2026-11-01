/* SPDX-License-Identifier: MIT */
/* Verify gray12 colorsync. */
#include "tests/_support/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(12, SIXEL_CMS_ENGINE_COLORSYNC, 0);
}
