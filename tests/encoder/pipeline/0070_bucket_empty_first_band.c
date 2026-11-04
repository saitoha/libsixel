/* SPDX-License-Identifier: MIT */
/* Verify bucket empty first band with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0070(int argc, char **argv)
{
    sixel_index_t pixels[1 * 7];

    (void)argc;
    (void)argv;
    memset(pixels, 2, sizeof(pixels));
    pixels[6] = 0;
    return test_band_nodes_wire(pixels, 1, 7, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "-#0@");
}
