/* SPDX-License-Identifier: MIT */
/* Keep ordered unrounded fixed entries
 * and FS selection with medoids;
 * skip construction. */
#include <stddef.h>
int test_map_solver(int mode, int space, char const *path,
                    char const *quantizer, char const *binning,
                    int output_p3, int solver);

int
test_map_solver_0029(int argc, char **argv)
{
    return argc == 2 ? test_map_solver(1, 0, argv[1],
        "medoids:seed=1", "soft", 0, 3) : 1;
}
