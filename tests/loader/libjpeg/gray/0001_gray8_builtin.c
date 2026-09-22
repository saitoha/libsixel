/* SPDX-License-Identifier: MIT */
/* Verify gray8 builtin. */
#include "tests/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(8, SIXEL_CMS_ENGINE_BUILTIN, 0);
}
