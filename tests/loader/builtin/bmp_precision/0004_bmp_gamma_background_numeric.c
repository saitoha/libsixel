/* SPDX-License-Identifier: MIT */
/* Verify BMP gamma background with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(4, SIXEL_CMS_ENGINE_BUILTIN);
}
