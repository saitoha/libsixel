/* SPDX-License-Identifier: MIT */
/* Verify BMP partial background with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(21, SIXEL_CMS_ENGINE_BUILTIN);
}
