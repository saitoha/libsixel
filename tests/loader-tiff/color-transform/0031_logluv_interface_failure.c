/* SPDX-License-Identifier: MIT */
/* One independently reported LogLuv publication observation. */
#include "tests/_support/loader/tiff_logluv_test_common.h"

int
test_loader_0219_logluv(int argc, char **argv)
{
    return tiff_logluv_failure_test(argc, argv, 2);
}
