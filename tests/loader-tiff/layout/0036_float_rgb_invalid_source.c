/* SPDX-License-Identifier: MIT */
/* Failed managed ICC conversion preserves the original float samples. */
#include "tests/_support/loader/tiff_float_test_common.h"

int
test_loader_0238_tfloat(int argc, char **argv)
{
    return tiff_float_values_test(argc, argv, 11);
}
