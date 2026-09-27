/* SPDX-License-Identifier: MIT */
/* Verify BMP transform fail with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0033(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(33, SIXEL_CMS_ENGINE_BUILTIN);
}
