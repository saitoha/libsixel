/* SPDX-License-Identifier: MIT */
#include "tests/_support/processing/cms/hdr_exposure_common.h"

int
test_hdr_numeric_16(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return cms_hdr_exposure_test(0, 1);
}
