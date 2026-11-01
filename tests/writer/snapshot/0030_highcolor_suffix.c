/* SPDX-License-Identifier: MIT */
/* highcolor suffix snapshot contract. */
#include <stdlib.h>
int test_highcolor_png(int, char const *);

int
test_png_snapshot_0030(int argc, char **argv)
{
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return test_highcolor_png(6, argv[1]);
}
