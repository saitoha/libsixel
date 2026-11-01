/* SPDX-License-Identifier: MIT */
/* Check one JPEG source-domain ICC condition with independent values. */
#include "src/cms.h"
#include "tests/_support/loader/jpeg_gray_test_common.h"
#include "tests/_support/loader/builtin_jpeg_icc_test_common.h"

int
test_bjicc_0033(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_jpeg_ink_values(7, SIXEL_CMS_ENGINE_COLORSYNC, 0);
}
