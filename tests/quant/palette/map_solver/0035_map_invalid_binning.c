/* SPDX-License-Identifier: MIT */
/* Reject unsupported image-map binning before any output. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0035(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(10, 0, argv[1],
        "center:seed=1", "soft", 0, 4) : 1;
}
