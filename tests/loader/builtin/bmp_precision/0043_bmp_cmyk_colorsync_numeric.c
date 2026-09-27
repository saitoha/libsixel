/* SPDX-License-Identifier: MIT */
/* Verify BMP cmyk colorsync with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0043(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(43, SIXEL_CMS_ENGINE_COLORSYNC);
}
