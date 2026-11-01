/* SPDX-License-Identifier: MIT */
/* Verify retained-color residuals at the next lookup and immutable input. */
#include <sixel.h>
#include "tests/_support/processing/dither/carry/carry_test_common.h"
int
test_filter_0017_filter_dither_6delta_stucki(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return carry_test_keep(SIXEL_DIFFUSE_STUCKI, 0, 0);
}
