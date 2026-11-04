/* SPDX-License-Identifier: MIT */
/* Verify mask sparse mono selector with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0087(int argc, char **argv)
{
    sixel_index_t pixels[17];
    unsigned char mask[17];

    (void)argc;
    (void)argv;
    memset(pixels, 1, sizeof(pixels));
    memset(mask, 255, sizeof(mask));
    mask[0] = mask[16] = 0;
    return test_band_nodes_wire(pixels, 17, 1, 2, 0,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, mask,
        "@!15?@");
}
