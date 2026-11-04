/* SPDX-License-Identifier: MIT */
/* Verify bucket size partial rows with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0074(int argc, char **argv)
{
    sixel_index_t pixels[2 * 2];

    (void)argc;
    (void)argv;
    pixels[0] = pixels[3] = 0;
    pixels[1] = pixels[2] = 1;
    return test_band_nodes_wire(pixels, 2, 2, 3, 2,
        SIXEL_ENCODEPOLICY_SIZE, 0, 0, 1, NULL,
        "#1BB$#0@A");
}
