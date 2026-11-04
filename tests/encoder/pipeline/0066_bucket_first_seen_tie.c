/* SPDX-License-Identifier: MIT */
/* Verify bucket first seen tie with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0066(int argc, char **argv)
{
    sixel_index_t pixels[4 * 2];

    (void)argc;
    (void)argv;
    memset(pixels, 1, 4);
    memset(pixels + 4, 0, 4);
    return test_band_nodes_wire(pixels, 4, 2, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "#0!4A$#1!4@");
}
