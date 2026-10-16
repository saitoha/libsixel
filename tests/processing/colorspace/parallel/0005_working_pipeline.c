/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
int parallel_float_values(int frame_api);
int parallel_float_atomic(int frame_api);
int parallel_float_pipeline(int clustering, char const *path);

int
test_parallel_float_0005(int argc, char **argv)
{
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return parallel_float_pipeline(0, argv[1]);
}
