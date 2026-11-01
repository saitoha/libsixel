/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "../../_support/planner/pipeline/palette_auto_test_common.h"

int
test_palette_auto_0086(int argc, char **argv)
{
    if (argc != 2) return EXIT_FAILURE;
    return palette_auto_playback_test(atoi(argv[1]));
}
