/* SPDX-License-Identifier: MIT */
/* Verify BMP p3 rgb10 with independent source values. */
#include "src/cms.h"
#include "tests/loader/bmp_precision_test_common.h"

int
test_bmpc_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return bmp_precision_test(12, SIXEL_CMS_ENGINE_BUILTIN);
}
