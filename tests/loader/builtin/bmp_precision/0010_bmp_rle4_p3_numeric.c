/* SPDX-License-Identifier: MIT */
/* Verify BMP rle4 p3 with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(10, SIXEL_CMS_ENGINE_BUILTIN);
}
