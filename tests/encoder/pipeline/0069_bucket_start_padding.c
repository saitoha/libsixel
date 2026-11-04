/* SPDX-License-Identifier: MIT */
/* Verify bucket start padding with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0069(int argc, char **argv)
{
    sixel_index_t pixels[65 * 1];

    (void)argc;
    (void)argv;
    memset(pixels, 2, sizeof(pixels));
    pixels[64] = 0;
    return test_band_nodes_wire(pixels, 65, 1, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "#0!64?@");
}
