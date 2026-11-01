/* SPDX-License-Identifier: MIT */
/* Retain a CMS-loaded P3 primary and execute center in cielab. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0009(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(6, 3, argv[1],
        "center:seed=1", "none", 0, 4) : 1;
}
