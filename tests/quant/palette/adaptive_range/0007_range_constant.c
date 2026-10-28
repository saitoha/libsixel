/* SPDX-License-Identifier: MIT
 * Constant black and a white highlight do not divide by zero.
 */
#include <stdlib.h>
#include "src/palette-bin-histogram.h"

int
test_0007_range_constant(int argc, char **argv)
{
    sixel_palette_bin_range_t range;
    unsigned int value;

    (void)argc;
    (void)argv;
    sixel_palette_bin_range_init(&range, 32u);
    for (value = 0u; value < 4096u; ++value) {
        sixel_palette_bin_range_observe(&range, 0u, 0u, 0.0);
    }
    sixel_palette_bin_range_observe(&range, 0u, 31u, 255.0);
    sixel_palette_bin_range_finish(&range, 1, 100u);
    if (range.active[0] || range.active[1] || range.active[2] ||
            sixel_palette_bin_range_index(&range, 0u, 0u, 0.0) != 0u ||
            sixel_palette_bin_range_index(&range, 0u, 31u, 255.0) != 31u) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
