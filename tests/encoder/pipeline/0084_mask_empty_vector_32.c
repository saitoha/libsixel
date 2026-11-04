/* SPDX-License-Identifier: MIT */
/* Verify mask empty vector 32 with one fixed raster. */
#include <string.h>
#include <sixel.h>
int test_band_nodes_wire(sixel_index_t *, int, int, int, int, int, int,
    int, int, unsigned char const *, char const *);

int
test_cpu_serialize_0084(int argc, char **argv)
{
    sixel_index_t pixels[32];
    unsigned char mask[32];

    (void)argc;
    (void)argv;
    memset(pixels, 0, sizeof(pixels));
    memset(mask, 255, sizeof(mask));
    return test_band_nodes_wire(pixels, 32, 1, 3, 2,
        SIXEL_ENCODEPOLICY_FAST, 0, 0, 1, mask,
        "");
}
