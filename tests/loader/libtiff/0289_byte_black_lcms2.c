/* SPDX-License-Identifier: MIT */
/* One original-byte TIFF publication observation. */
#include "tests/loader/tiff_byte_test_common.h"

int
test_loader_0289_byte(int argc, char **argv)
{
    return tiff_byte_test(argc, argv, 1, 3, 0);
}
