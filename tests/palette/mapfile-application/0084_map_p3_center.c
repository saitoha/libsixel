/* SPDX-License-Identifier: MIT */
/* Retain native entries with center and Display P3 wire output. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0022(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(6, 1, argv[1],
        "center:seed=1", "none", 1, 4) : 1;
}
