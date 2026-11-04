/* SPDX-License-Identifier: MIT */
/* Verify mask scalar tail 15 with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0080(int argc, char **argv)
{
    sixel_index_t pixels[15];
    unsigned char mask[15];

    (void)argc;
    (void)argv;
    memset(pixels, 0, sizeof(pixels));
    memset(mask, 255, sizeof(mask));
    mask[0] = mask[14] = 0;
    return test_band_nodes_wire(pixels, 15, 1, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, mask,
        "#0@!13?@");
}
