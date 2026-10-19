/* SPDX-License-Identifier: MIT */
/* Verify retained-color residuals at the next lookup and immutable input. */
#include <sixel.h>
#include "tests/processing/dither/carry/carry_test_common.h"
int
test_perturb_0018_keep_lookup(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return carry_test_keep(SIXEL_DIFFUSE_FS, 1, 0);
}
