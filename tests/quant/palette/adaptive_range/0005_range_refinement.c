/* SPDX-License-Identifier: MIT
 * All dark steps remain distinct and sparse highlights keep separate bins.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0005_range_refinement(int argc, char **argv)
{
    sixel_palette_bin_range_t range;
    unsigned int value;
    unsigned int previous;
    unsigned int address;

    (void)argc;
    (void)argv;
    sixel_palette_bin_range_init(&range, 64u);
    for (value = 0u; value < 40u; ++value) {
        sixel_palette_bin_range_observe(&range, 0u, value >> 2u, value);
    }
    sixel_palette_bin_range_observe(&range, 0u, 61u, 244.0);
    sixel_palette_bin_range_observe(&range, 0u, 63u, 255.0);
    sixel_palette_bin_range_finish(&range, 1, 100u);
    previous = 64u;
    for (value = 0u; value < 40u; ++value) {
        address = sixel_palette_bin_range_index(&range, 0u,
            value >> 2u, value);
        if (address >= 64u || address == previous) {
            return EXIT_FAILURE;
        }
        previous = address;
    }
    address = sixel_palette_bin_range_index(&range, 0u, 61u, 244.0);
    if (address <= previous ||
            sixel_palette_bin_range_index(&range, 0u, 63u, 255.0)
            <= address) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
