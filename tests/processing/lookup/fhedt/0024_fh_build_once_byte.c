/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
int fhedt_build_encode(int quality, int overlap, char const *path);
int fhedt_build_trace(char const *path, int overlap);

int
test_fh_lookup_0024(int argc, char **argv)
{
    if (argc == 3) {
        return fhedt_build_trace(argv[1], 0);
    }
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return fhedt_build_encode(0, 0, argv[1]);
}
