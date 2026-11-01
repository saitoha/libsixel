/* SPDX-License-Identifier: MIT */
/* Verify BMP cmyk rle4 with independent source values. */
#include "src/cms.h"
#include "tests/_support/loader/bmp_precision_test_common.h"

int
test_bmpc_0025(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(25, SIXEL_CMS_ENGINE_BUILTIN);
}
