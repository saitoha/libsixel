/* SPDX-License-Identifier: MIT */
/* Verify mask size protected hole with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0091(int argc, char **argv)
{
    sixel_index_t pixels[32];
    unsigned char mask[32];

    (void)argc;
    (void)argv;
    memset(pixels, 0, sizeof(pixels));
    memset(mask, 0, sizeof(mask));
    mask[1] = 255;
    return test_band_nodes_wire(pixels, 16, 2, 3, 2,
        SIXEL_ENCODEPOLICY_SIZE, 0, 0, 1, mask,
        "#0BA!14B");
}
