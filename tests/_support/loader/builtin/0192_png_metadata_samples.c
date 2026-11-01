/* SPDX-License-Identifier: MIT */
/* Exercise one independent PNG metadata observation per invocation. */
#include <stdlib.h>
#include "loader_builtin_png_metadata_test_common.h"

int
test_loader_0192_png_metadata_samples(int argc, char **argv)
{
    if (argc != 4) {
        return 1;
    }
    return png_metadata_check(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
}
