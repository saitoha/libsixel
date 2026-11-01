/* SPDX-License-Identifier: MIT */
/* One arithmetic observation, with setup shared across registered entries. */
#include <stdlib.h>
#include "png_arithmetic_test_common.h"

int
test_loader_0209_png_arith_invalid(int argc, char **argv)
{
    (void)argv;
    if (!(argc == 2)) {
        return 1;
    }
    return png_arith_pixel_atomic(atoi(argv[1]));
}
