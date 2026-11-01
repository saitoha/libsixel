/* SPDX-License-Identifier: MIT */
/* PNG snapshot srgb rgb pixels. */
#include "tests/writer/png/color_test_common.h"

int
test_png_snapshot_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_snapshot_check(0, 0);
}
