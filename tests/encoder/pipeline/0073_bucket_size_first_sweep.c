/* SPDX-License-Identifier: MIT */
/* Verify bucket size first sweep with one fixed indexed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_band_nodes_0073(int argc, char **argv)
{
    sixel_index_t pixels[8 * 6];
    int x;
    int y;
    unsigned int masks[4] = {15, 3, 60, 48};

    (void)argc;
    (void)argv;
    for (y = 0; y < 6; ++y) {
        for (x = 0; x < 8; ++x) {
            pixels[y * 8 + x] = (sixel_index_t)(2 * (x / 4) +
                ((masks[x % 4] & (1U << y)) == 0));
        }
    }
    return test_band_nodes_wire(pixels, 8, 6, 4, -1,
        SIXEL_ENCODEPOLICY_SIZE, 0, 0, 1, NULL,
        "#1!4~#3!4~$#0NB{o#2NB{o");
}
