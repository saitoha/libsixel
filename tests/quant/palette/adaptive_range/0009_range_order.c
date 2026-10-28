/* SPDX-License-Identifier: MIT
 * Byte bin refinement is independent of sample traversal order.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0009_range_order(int argc, char **argv)
{
    sixel_palette_bin_range_t forward;
    sixel_palette_bin_range_t reverse;
    unsigned int value;
    unsigned int address;

    (void)argc;
    (void)argv;
    sixel_palette_bin_range_init(&forward, 64u);
    sixel_palette_bin_range_init(&reverse, 64u);
    for (value = 0u; value < 40u; ++value) {
        sixel_palette_bin_range_observe(&forward, 0u, value >> 2u, value);
        sixel_palette_bin_range_observe(&reverse, 0u,
            (39u - value) >> 2u, 39u - value);
    }
    sixel_palette_bin_range_finish(&forward, 1, 100u);
    sixel_palette_bin_range_finish(&reverse, 1, 100u);
    for (value = 0u; value < 40u; ++value) {
        address = sixel_palette_bin_range_index(&forward, 0u,
            value >> 2u, value);
        if (sixel_palette_bin_range_index(&reverse, 0u,
                value >> 2u, value) != address) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
