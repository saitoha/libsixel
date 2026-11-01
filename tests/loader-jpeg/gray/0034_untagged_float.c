/* SPDX-License-Identifier: MIT */
/* Verify untagged float. */
#include "tests/_support/loader/jpeg_gray_test_common.h"
#include "src/cms.h"

int
test_jgray_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_gray_values(8, SIXEL_CMS_ENGINE_BUILTIN, 7);
}
