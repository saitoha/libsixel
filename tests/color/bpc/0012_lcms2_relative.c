/* SPDX-License-Identifier: MIT */
#include "tests/_support/processing/cms/bpc_common.h"

int
test_bpc_11(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_bpc_native_test(SIXEL_CMS_ENGINE_LCMS2, 1);
}
