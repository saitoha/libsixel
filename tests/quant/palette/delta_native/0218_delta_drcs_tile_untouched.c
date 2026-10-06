/* SPDX-License-Identifier: MIT */
/* Reject before opening or truncating the character-output file. */
#include <stdlib.h>
int test_delta_drcs_tile_file(char const *);

int
test_delta_native_0218(int argc, char **argv)
{
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    return test_delta_drcs_tile_file(argv[1]);
}
