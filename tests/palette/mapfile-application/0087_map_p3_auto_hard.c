/* SPDX-License-Identifier: MIT */
/* Resolve AUTO to K-means with Display P3 output. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0025(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(6, 4, argv[1],
        "auto", "hard", 1, 2) : 1;
}
