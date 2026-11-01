/* SPDX-License-Identifier: MIT */
/* One original-byte TIFF publication observation. */
#include "tests/_support/loader/tiff_byte_test_common.h"

int
test_loader_0304_byte(int argc, char **argv)
{
    return tiff_byte_decode(argc, argv, 5);
}
