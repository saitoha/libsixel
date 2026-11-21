/* SPDX-License-Identifier: MIT */
#include "stream_fault_common.h"

int
test_png_stream_0051(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_test_stream_fault(PNG_STREAM_IDAT_SHORT_WRITE);
}
