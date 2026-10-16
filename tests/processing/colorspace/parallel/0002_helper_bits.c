/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
int parallel_float_values(int frame_api);
int parallel_float_atomic(int frame_api);
int parallel_float_pipeline(int clustering, char const *path);

int
test_parallel_float_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return parallel_float_values(0);
}
