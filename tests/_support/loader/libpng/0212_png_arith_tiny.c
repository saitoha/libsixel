/* SPDX-License-Identifier: MIT */
/* Nonneutral tiny positive and negative values with certified exact bits. */
#include <stdlib.h>
#include "png_arithmetic_test_common.h"

int
test_loader_0212_png_arith_tiny(int argc, char **argv)
{
    if (argc != 3) {
        return 1;
    }
    return png_arith_tiny(atoi(argv[1]), atoi(argv[2]));
}
