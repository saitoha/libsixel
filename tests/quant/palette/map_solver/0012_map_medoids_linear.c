/* SPDX-License-Identifier: MIT */
/* Retain a CMS-loaded P3 primary and execute medoids in linear. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0012(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(6, 1, argv[1],
        "medoids:seed=1", "none", 0, 3) : 1;
}
