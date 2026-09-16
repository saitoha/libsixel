/* SPDX-License-Identifier: MIT */
/* One original-byte TIFF publication observation. */
#include "tests/loader/tiff_byte_test_common.h"

int
test_loader_0303_byte(int argc, char **argv)
{
    return tiff_byte_decode(argc, argv, 4);
}
