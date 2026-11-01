/* SPDX-License-Identifier: MIT */
/* Verify BMP legacy ICC CMYK precision with independent source values. */
#include "src/cms.h"
#include "tests/_support/loader/bmp_precision_test_common.h"

int
test_bmpc_0045(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(45, SIXEL_CMS_ENGINE_BUILTIN);
}
