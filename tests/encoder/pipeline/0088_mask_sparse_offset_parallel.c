/* SPDX-License-Identifier: MIT */
/* Verify mask sparse offset parallel with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0088(int argc, char **argv)
{
    sixel_index_t pixels[32];
    unsigned char mask[32];

    (void)argc;
    (void)argv;
    memset(pixels, 0, sizeof(pixels));
    memset(mask, 255, sizeof(mask));
    mask[15] = mask[16] = 0;
    return test_band_nodes_wire(pixels, 16, 2, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 3, 5, 2, mask,
        "#0!18?_-#0???@");
}
