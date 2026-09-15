/* SPDX-License-Identifier: MIT */
/* One arithmetic observation, with setup shared across registered entries. */
#include <stdlib.h>
#include "png_arithmetic_test_common.h"

int
test_loader_0203_png_arith_dispatch(int argc, char **argv)
{
    (void)argv;
    if (!(argc == 3)) {
        return 1;
    }
    return png_arith_dispatch(atoi(argv[1]), atoi(argv[2]));
}
