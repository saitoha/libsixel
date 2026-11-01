/* SPDX-License-Identifier: MIT */
/* One independently reported unmanaged float contract. */
#include "tests/_support/loader/tiff_float_test_common.h"

int
test_loader_0246_tfloat(int argc, char **argv)
{
    return tiff_float_failure_test(argc, argv, 1);
}
