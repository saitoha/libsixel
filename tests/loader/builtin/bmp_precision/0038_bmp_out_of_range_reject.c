/* SPDX-License-Identifier: MIT */
/* Verify BMP out of range reject with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0038(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(38, SIXEL_CMS_ENGINE_BUILTIN);
}
