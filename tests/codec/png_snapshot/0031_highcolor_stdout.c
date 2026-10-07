/* SPDX-License-Identifier: MIT */
/* Actual high-color CLI stdout pixels and P3 interpretation. */
#include "tests/writer/png/color_test_common.h"

int
test_png_snapshot_0031(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return png_snapshot_direct_check(1, 0);
}
