/* SPDX-License-Identifier: MIT */
/* One independently reported unmanaged float contract. */
#include "tests/loader/tiff_float_test_common.h"

int
test_loader_0232_tfloat(int argc, char **argv)
{
    return tiff_float_values_test(argc, argv, 5);
}
