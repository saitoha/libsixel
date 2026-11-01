/* SPDX-License-Identifier: MIT */
/* Keep genuine byte map application with auto. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0030(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(2, 0, argv[1],
        "auto", "none", 0, 1) : 1;
}
