/* SPDX-License-Identifier: MIT */
/* Verify gray12 cms none. */
#include "tests/_support/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(12, SIXEL_CMS_ENGINE_NONE, 6);
}
