/* SPDX-License-Identifier: MIT */
/* PNG snapshot p3 hls pixels. */
#include "tests/writer/png/color_test_common.h"

int
test_png_snapshot_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_snapshot_check(1, 1);
}
