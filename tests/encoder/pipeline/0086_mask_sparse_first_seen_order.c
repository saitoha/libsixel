/* SPDX-License-Identifier: MIT */
/* Verify mask sparse first seen order with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0086(int argc, char **argv)
{
    sixel_index_t pixels[32];
    unsigned char mask[32];

    (void)argc;
    (void)argv;
    /* Color 1 is seen first; equal spans emit later-seen color 0 first.
     * The greedy sweep visits both color-0 spans before returning left. */
    memset(pixels, 1, 16);
    memset(pixels + 16, 0, 16);
    memset(mask, 255, sizeof(mask));
    mask[0] = mask[15] = mask[16] = mask[31] = 0;
    return test_band_nodes_wire(pixels, 16, 2, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, mask,
        "#0A!14?A$#1@!14?@");
}
