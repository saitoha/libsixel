/* SPDX-License-Identifier: MIT
 * A narrow float interval is refined without absorbing a highlight.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0008_range_float(int argc, char **argv)
{
    sixel_palette_bin_range_t range;
    unsigned int value;
    unsigned int address;
    unsigned int previous;

    (void)argc;
    (void)argv;
    sixel_palette_bin_range_init(&range, 64u);
    for (value = 1u; value <= 4u; ++value) {
        sixel_palette_bin_range_observe(&range, 0u, 0u, value * 0.001);
    }
    sixel_palette_bin_range_observe(&range, 0u, 63u, 1.0);
    sixel_palette_bin_range_finish(&range, 0, 100u);
    previous = 64u;
    for (value = 1u; value <= 4u; ++value) {
        address = sixel_palette_bin_range_index(&range, 0u, 0u,
            value * 0.001);
        if (address >= 63u || address == previous) {
            return EXIT_FAILURE;
        }
        previous = address;
    }
    if (sixel_palette_bin_range_index(&range, 0u, 63u, 1.0) <= previous) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
