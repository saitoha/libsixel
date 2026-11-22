/* SPDX-License-Identifier: MIT */
#include "stream_fault_common.h"

int
test_png_stream_0064(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_test_stream_fault(PNG_STREAM_LIBPNG_INFO_STRUCT_FAILURE);
}
