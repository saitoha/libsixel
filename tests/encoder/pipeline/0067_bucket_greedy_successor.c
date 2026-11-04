/* SPDX-License-Identifier: MIT */
/* Verify bucket greedy successor with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0067(int argc, char **argv)
{
    sixel_index_t pixels[16 * 2];

    (void)argc;
    (void)argv;
    memset(pixels, 3, sizeof(pixels));
    pixels[0] = pixels[5] = 0;
    pixels[16] = pixels[25] = 1;
    pixels[12] = pixels[15] = 2;
    return test_band_nodes_wire(pixels, 16, 2, 4, 3,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "#1A!8?A#2??@??@$#0@!4?@");
}
