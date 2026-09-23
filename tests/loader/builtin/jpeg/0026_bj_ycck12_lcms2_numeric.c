/* SPDX-License-Identifier: MIT */
/* Check one JPEG source-domain ICC condition with independent values. */
#include "src/cms.h"
#include "tests/loader/jpeg_gray_test_common.h"
#include "tests/loader/builtin_jpeg_icc_test_common.h"

int
test_bjicc_0026(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_jpeg_ink_values(5, SIXEL_CMS_ENGINE_LCMS2, 0);
}
