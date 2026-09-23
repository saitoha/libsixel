/* SPDX-License-Identifier: MIT */
/* Check one JPEG source-domain ICC condition with independent values. */
#include "src/cms.h"
#include "tests/loader/jpeg_gray_test_common.h"
#include "tests/loader/builtin_jpeg_icc_test_common.h"

int
test_bjicc_0044(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_jpeg_ink_values(1, SIXEL_CMS_ENGINE_BUILTIN, 5);
}
