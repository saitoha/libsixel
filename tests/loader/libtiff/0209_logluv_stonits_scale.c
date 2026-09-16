/* SPDX-License-Identifier: MIT */
/* One independently reported LogLuv publication observation. */
#include "tests/loader/tiff_logluv_test_common.h"

int
test_loader_0209_logluv(int argc, char **argv)
{
    return tiff_logluv_values_test(argc, argv, 9);
}
