/* SPDX-License-Identifier: MIT */
/* Verify map all sixbit values with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0093(int argc, char **argv)
{
    sixel_index_t pixels[64 * 6];
    int x;
    int y;

    (void)argc;
    (void)argv;
    /* Column x paints precisely the six row bits in x. */
    for (y = 0; y < 6; y++) {
        for (x = 0; x < 64; x++) {
            pixels[y * 64 + x] = (sixel_index_t)(
                (x & (1 << y)) ? 0 : 2);
        }
    }
    return test_band_nodes_wire(pixels, 64, 6, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, NULL,
        "#0?"
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
}
