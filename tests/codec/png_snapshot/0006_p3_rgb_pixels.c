/* SPDX-License-Identifier: MIT */
/* PNG snapshot p3 rgb pixels. */
#include "tests/writer/png/color_test_common.h"

int
test_png_snapshot_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_snapshot_check(1, 0);
}
