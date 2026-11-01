/* SPDX-License-Identifier: MIT */
/* Verify gray12 builtin. */
#include "tests/_support/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(12, SIXEL_CMS_ENGINE_BUILTIN, 0);
}
