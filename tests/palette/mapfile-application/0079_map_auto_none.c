/* SPDX-License-Identifier: MIT */
/* Resolve none AUTO image-map construction without changing the request. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0017(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(6, 0, argv[1],
        "auto", "none", 0, 1) : 1;
}
