/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "../../_support/planner/pipeline/palette_auto_test_common.h"

int
test_palette_auto_0092(int argc, char **argv)
{
    if (argc != 3) return EXIT_FAILURE;
    return palette_auto_store_test(1, atoi(argv[1]), atoi(argv[2]));
}
