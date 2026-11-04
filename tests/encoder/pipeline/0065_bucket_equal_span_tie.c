/* SPDX-License-Identifier: MIT */
/* Verify bucket equal span tie with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0065(int argc, char **argv)
{
    sixel_index_t pixels[4 * 2];

    (void)argc;
    (void)argv;
    memset(pixels, 0, 4);
    memset(pixels + 4, 1, 4);
    return test_band_nodes_wire(pixels, 4, 2, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "#1!4A$#0!4@");
}
