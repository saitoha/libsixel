/* SPDX-License-Identifier: MIT */
/* Verify bucket longest first with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0064(int argc, char **argv)
{
    sixel_index_t pixels[7 * 2];

    (void)argc;
    (void)argv;
    memset(pixels, 2, sizeof(pixels));
    pixels[0] = pixels[3] = 0;
    pixels[7] = pixels[13] = 1;
    return test_band_nodes_wire(pixels, 7, 2, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "#1A!5?A$#0@??@");
}
