/* SPDX-License-Identifier: MIT */
/* Verify mask sparse dense transition with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0085(int argc, char **argv)
{
    sixel_index_t pixels[33];
    unsigned char mask[33];

    (void)argc;
    (void)argv;
    memset(pixels, 0, sizeof(pixels));
    memset(mask, 0, sizeof(mask));
    memset(mask, 255, 15);
    mask[32] = 255;
    return test_band_nodes_wire(pixels, 33, 1, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, mask,
        "#0!15?!17@");
}
