/* SPDX-License-Identifier: MIT */
/* One arithmetic observation, with setup shared across registered entries. */
#include <stdlib.h>
#include "png_arithmetic_test_common.h"

int
test_loader_0210_png_arith_loader(int argc, char **argv)
{
    (void)argv;
    if (!(argc == 4)) {
        return 1;
    }
    return png_arith_load(atoi(argv[1]), argv[2], atoi(argv[3]));
}
