/* SPDX-License-Identifier: MIT */
/* One arithmetic observation, with setup shared across registered entries. */
#include <stdlib.h>
#include "png_arithmetic_test_common.h"

int
test_loader_0207_png_arith_srgb_target(int argc, char **argv)
{
    (void)argv;
    if (!(argc == 1)) {
        return 1;
    }
    return png_arith_target();
}
