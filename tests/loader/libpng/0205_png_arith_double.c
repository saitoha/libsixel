/* SPDX-License-Identifier: MIT */
/* One arithmetic observation, with setup shared across registered entries. */
#include <stdlib.h>
#include "png_arithmetic_test_common.h"

int
test_loader_0205_png_arith_double(int argc, char **argv)
{
    int requested;

    if (argc != 2 && argc != 3) {
        return 1;
    }
    requested = argc == 3 ? atoi(argv[2]) : 1;
    if (requested != 0 && requested != 1) {
        return 1;
    }
    return png_arith_values(atoi(argv[1]), requested, 1, 0);
}
