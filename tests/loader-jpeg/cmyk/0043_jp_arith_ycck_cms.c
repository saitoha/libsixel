/* SPDX-License-Identifier: MIT */
/* Preserve the source-domain CMYK ICC contract through arith decoding. */
#include "src/cms.h"
#include "tests/_support/loader/builtin_jpeg_icc_test_common.h"

int
test_loader_jp_0137(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return builtin_jpeg_ink_values(2, SIXEL_CMS_ENGINE_BUILTIN, 8);
}
