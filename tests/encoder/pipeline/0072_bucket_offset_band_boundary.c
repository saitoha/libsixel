/* SPDX-License-Identifier: MIT */
/* Verify bucket offset band boundary with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0072(int argc, char **argv)
{
    sixel_index_t pixels[1 * 2];

    (void)argc;
    (void)argv;
    pixels[0] = 0;
    pixels[1] = 1;
    return test_band_nodes_wire(pixels, 1, 2, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 3, 5, 1, NULL,
        "#0???_-#1???@");
}
