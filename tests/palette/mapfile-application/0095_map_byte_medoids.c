/* SPDX-License-Identifier: MIT */
/* Keep genuine byte map application with medoids. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0033(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(2, 0, argv[1],
        "medoids:seed=1", "none", 0, 3) : 1;
}
