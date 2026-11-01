/* SPDX-License-Identifier: MIT */
#include <stdlib.h>

int pipeline_worker_counts(char const *path, char const *plan, int automatic);

int
test_pipeline_worker_counts_18(int argc, char **argv)
{
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    return pipeline_worker_counts(argv[1], argv[2], 0);
}
